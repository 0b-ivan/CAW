// Pack files, the manifest and the decks built from chosen packs. Synthetic cards only.
#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>

#include "net/Messages.h"
#include "packs/CsvPack.h"
#include "packs/PackLibrary.h"

using caw::CardPack;
using caw::CsvPack;
using caw::PackLibrary;

namespace {
CardPack pack(const std::string& id, const std::string& language, const std::string& csv) {
  CardPack result{id, "Titel " + id, language, "CC0", "Test", "https://example.org", {}, {}};
  std::string error;
  REQUIRE(CsvPack::read(csv, result, error));
  return result;
}
}  // namespace

TEST_CASE("CSV cards: quotes, line breaks, blanks, Pick and Draw") {
  CardPack result;
  std::string error;
  const std::string csv =
      "\xEF\xBB\xBFtyp,text\r\n"
      "b,\"Frage mit ___.\"\r\n"
      "b,\"___ und ___ und ____.\"\n"
      "b,Frage ohne Lücke?\n"
      "w,\"Antwort mit \"\"Zitat\"\"\"\n"
      "\n"
      "w,Grüße\n"
      "b,\"Schritt 1: ___.\r\nSchritt 2: Gewinn\"\n";
  REQUIRE(CsvPack::read(csv, result, error));
  REQUIRE(result.prompts.size() == 4);
  // A line break inside a quoted card is a space.
  CHECK(result.prompts[3].text == "Schritt 1: ___. Schritt 2: Gewinn");
  CHECK(result.prompts[0].pick == 1);
  CHECK(result.prompts[1].pick == 3);
  CHECK(result.prompts[1].draw == 2);
  CHECK(result.prompts[2].pick == 1);  // no blank: the answer goes at the end
  REQUIRE(result.answers.size() == 2);
  CHECK(result.answers[0] == "Antwort mit \"Zitat\"");
  CHECK(result.answers[1] == "Grüße");
}

TEST_CASE("a broken CSV adds nothing and names the line") {
  for (const std::string& csv :
       {std::string("text,typ\nb,Frage ___.\n"),       // wrong header
        std::string("typ,text\nx,Unbekannter Typ\n"),  // type
        std::string("typ,text\nw,\n"),                 // empty
        std::string("typ,text\nb,___ ___ ___ ___\n"),  // four blanks
        std::string("typ,text\nw,\"offenes Zitat\n"),  // quote
        std::string("typ,text\nw,kaputt \xC3\n"),      // UTF-8
        std::string("typ,text\nw,Tab\tdrin\n"),        // control character
        std::string("typ,text\n"),                     // no cards
        "typ,text\nw," + std::string(CsvPack::kMaxCardBytes + 1, 'a') + "\n"}) {
    CardPack result;
    result.answers.push_back("schon da");
    std::string error;
    CHECK_FALSE(CsvPack::read(csv, result, error));
    CHECK_FALSE(error.empty());
    CHECK(result.answers.size() == 1);
    CHECK(result.prompts.empty());
  }
}

TEST_CASE("the library builds one deck per choice, in a fixed order") {
  PackLibrary library;
  CHECK(library.add(pack("b-pack", "de", "typ,text\nb,Frage B ___.\nw,Antwort B\nw,Gleich\n")));
  CHECK(library.add(pack("a-pack", "de", "typ,text\nb,Frage A ___.\nw,Antwort A\nw,Gleich\n")));
  CHECK(library.add(pack("prompts-only", "de", "typ,text\nb,Nur Frage ___.\n")));
  CHECK(library.add(pack("english", "en", "typ,text\nb,Prompt ___.\nw,Answer\n")));
  CHECK_FALSE(library.add(pack("a-pack", "de", "typ,text\nw,Doppelt\n")));
  CHECK_FALSE(library.add(pack("Bad Id", "de", "typ,text\nw,X\n")));

  const cardparty::Deck* first = library.deck({"b-pack", "a-pack"});
  const cardparty::Deck* second = library.deck({"a-pack", "b-pack"});
  REQUIRE(first != nullptr);
  CHECK(first == second);  // built once, any order of the same choice
  CHECK(std::string(first->prompts[0].text) == "Frage A ___.");  // id order
  CHECK(first->answersCount == 3);  // "Gleich" is in both packs and played once

  CHECK(library.deck({"a-pack", "a-pack"}) == nullptr);
  CHECK(library.deck({"missing"}) == nullptr);
  CHECK(library.deck({}) == nullptr);
  CHECK(library.deck({"prompts-only"}) == nullptr);  // nothing to answer with
  CHECK(library.deck({"prompts-only", "a-pack"}) != nullptr);

  CHECK(library.defaultSelection("en") == std::vector<std::string>{"english"});
  CHECK(library.defaultSelection("de").size() == 3);
  PackLibrary german;
  german.add(pack("only-de", "de", "typ,text\nb,F ___.\nw,A\n"));
  CHECK(german.defaultSelection("en") ==
        std::vector<std::string>{"only-de"});  // nothing in English
}

TEST_CASE("opt-in card packs remain available without joining the default deck") {
  PackLibrary library;
  auto ordinary = pack("ordinary", "de", "typ,text\nb,Frage ___.\nw,Antwort\n");
  auto adult = pack("adult", "de", "typ,text\nb,Frage 18 ___.\nw,Antwort 18\n");
  adult.defaultSelected = false;
  REQUIRE(library.add(std::move(ordinary)));
  REQUIRE(library.add(std::move(adult)));
  CHECK(library.defaultSelection("de") == std::vector<std::string>{"ordinary"});
  CHECK(library.defaultSelection("en") == std::vector<std::string>{"ordinary"});
  REQUIRE(library.deck({"adult"}) != nullptr);

  const auto manifest = caw::Messages::readPackManifest(R"({"packs":[
    {"id":"adult","file":"adult.csv","title":"Adults","language":"de",
     "license":"TEST","rightsHolder":"TEST","sourceUrl":"https://example.org",
     "defaultSelected":false}]})");
  REQUIRE(manifest);
  CHECK_FALSE(manifest->at(0).defaultSelected);
  CHECK_FALSE(caw::Messages::readPackManifest(R"({"packs":[
    {"id":"adult","file":"adult.csv","title":"Adults","language":"de",
     "license":"TEST","rightsHolder":"TEST","sourceUrl":"https://example.org",
     "defaultSelected":"false"}]})"));
}

TEST_CASE("at most eight packs play together, like on the X4") {
  PackLibrary library;
  std::vector<std::string> ids;
  for (int i = 0; i < 9; ++i) {
    const std::string id = "p" + std::to_string(i);
    library.add(pack(id, "de", "typ,text\nb,F" + id + " ___.\nw,A" + id + "\n"));
    ids.push_back(id);
  }
  CHECK(library.deck(ids) == nullptr);
  ids.pop_back();
  CHECK(library.deck(ids) != nullptr);
  CHECK(library.defaultSelection("de").size() == 8);
}

TEST_CASE("manifests need every credit and stay in their own folder") {
  const auto entries =
      caw::Messages::readPackManifest(R"({"packs":[{"id":"x","file":"x.csv","title":"X",
    "language":"de","license":"CC0","rightsHolder":"Wir","sourceUrl":"https://example.org"}]})");
  REQUIRE(entries);
  CHECK(entries->at(0).file == "x.csv");
  CHECK_FALSE(caw::Messages::readPackManifest(R"({"packs":[{"id":"x","file":"x.csv","title":"X",
    "language":"de","license":"","rightsHolder":"Wir","sourceUrl":"https://example.org"}]})"));
  CHECK_FALSE(caw::Messages::readPackManifest(R"({"packs":[{"id":"x","file":"../x.csv","title":"X",
    "language":"de","license":"CC0","rightsHolder":"Wir","sourceUrl":"https://example.org"}]})"));
  CHECK_FALSE(caw::Messages::readPackManifest("[]"));
}

TEST_CASE("a pack directory loads its good packs and skips broken ones") {
  const auto root = std::filesystem::temp_directory_path() / "caw-test-packs";
  std::filesystem::remove_all(root);
  std::filesystem::create_directories(root / "source");
  std::ofstream(root / "source" / "packs.json") << R"({"packs":[
    {"id":"good","file":"good.csv","title":"Gut","language":"de","license":"CC0","rightsHolder":"Wir","sourceUrl":"https://example.org"},
    {"id":"broken","file":"broken.csv","title":"Kaputt","language":"de","license":"CC0","rightsHolder":"Wir","sourceUrl":"https://example.org"},
    {"id":"absent","file":"absent.csv","title":"Fehlt","language":"de","license":"CC0","rightsHolder":"Wir","sourceUrl":"https://example.org"}]})";
  std::ofstream(root / "source" / "good.csv") << "typ,text\nb,Frage ___.\nw,Antwort\n";
  std::ofstream(root / "source" / "broken.csv") << "falsch\n";
  PackLibrary library;
  CHECK(library.loadDirectory(root.string()) == 1);
  REQUIRE(library.find("good") != nullptr);
  CHECK(library.find("good")->license == "CC0");
  CHECK(library.find("broken") == nullptr);
  CHECK(library.loadDirectory((root / "missing").string()) == 0);
  std::filesystem::remove_all(root);
}

// Firmware regression scenarios, adapted only to the doctest harness.
#include <doctest/doctest.h>
// The GitHub pack list parser: whatever the network sends, only plain .dat
// names, drawable titles and bounded sizes get through.
#include <cstdio>
#include <cstring>
#include <string>

#include "CardPartyCatalog.h"

using cardparty::catalog::Catalog;

static bool parse(Catalog& c, const std::string& text) { return c.parse(text.data(), text.size()); }

TEST_CASE("upstream test_catalog") {
  Catalog c;
  REQUIRE((parse(c, "# CAW packs\nDeutsch.dat|Deutsch - alle Karten|40770\r\n\nmini_2.dat|Mini Grüße|1024\n")));
  REQUIRE((c.count() == 2));
  REQUIRE((std::strcmp(c.at(0).file, "Deutsch.dat") == 0 && std::strcmp(c.at(0).title, "Deutsch - alle Karten") == 0));
  REQUIRE((c.at(0).bytes == 40770 && std::strcmp(c.at(1).title, "Mini Grüße") == 0));
  REQUIRE((parse(c, "only.dat|One|99999")));  // no trailing newline
  REQUIRE((c.count() == 1));

  // A rejected list leaves the previous one in place.
  const char* bad[] = {
      "",                                    // nothing listed
      "# comment only\n",                    // nothing listed
      "../x.dat|Up|100\n",                   // path
      "sub/x.dat|Dir|100\n",                 // path
      ".hidden.dat|Hidden|100\n",            // hidden
      "x.txt|Text|100\n",                    // not a pack
      "x.DAT|Upper|100\n",                   // suffix is exact
      "x.dat||100\n",                        // empty title
      "x.dat|Title|\n",                      // no size
      "x.dat|Title|12a\n",                   // not a number
      "x.dat|Title|32\n",                    // header only
      "x.dat|Title|196609\n",                // over the pack limit
      "x.dat|Title|100|extra\n",             // extra field
      "x.dat|Title\n",                       // missing field
      "x.dat|\xc3|100\n",                    // broken UTF-8
      "x.dat|Tab\there|100\n",               // control character
      "x.dat|A|100\nx.dat|B|100\n",          // duplicate file
      "https://evil.example/x.dat|U|100\n",  // URL
  };
  for (const char* text : bad) {
    REQUIRE((!parse(c, text)));
    REQUIRE((c.count() == 1 && std::strcmp(c.at(0).file, "only.dat") == 0));
  }
  std::string longName(43, 'a');
  REQUIRE((parse(c, longName + ".dat|Long|100\n")));   // 47 bytes fits
  REQUIRE((!parse(c, longName + "a.dat|Long|100\n")));  // 48 does not
  std::string longTitle(48, 't');
  REQUIRE((!parse(c, "x.dat|" + longTitle + "|100\n")));
  std::string nine;
  for (int i = 0; i < 9; ++i) nine += "p" + std::to_string(i) + ".dat|P|100\n";
  REQUIRE((!parse(c, nine)));
  REQUIRE((!parse(c, std::string(cardparty::catalog::kMaxBytes + 1, '#'))));
  std::puts("cardparty catalog: names, titles, sizes, duplicates, limits, atomic replace OK");
}

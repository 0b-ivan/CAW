// Validation follows the firmware's pack reader: no empty or oversized cards,
// no control characters, at most three blanks, valid UTF-8.
#include "CsvPack.h"

#include <vector>

namespace caw {
namespace {

bool fail(std::string& error, std::string message) {
  error = std::move(message);
  return false;
}

// Bytes in the UTF-8 sequence that starts with this byte; 0 if it cannot start one.
std::size_t sequenceLength(unsigned char byte) {
  if (byte < 0x80) return 1;
  if ((byte >> 5) == 0x6) return 2;
  if ((byte >> 4) == 0xe) return 3;
  if ((byte >> 3) == 0x1e) return 4;
  return 0;
}

bool validUtf8(std::string_view text) {
  for (std::size_t i = 0; i < text.size();) {
    const std::size_t length = sequenceLength(static_cast<unsigned char>(text[i]));
    if (length == 0 || i + length > text.size()) return false;
    for (std::size_t k = 1; k < length; ++k)
      if ((static_cast<unsigned char>(text[i + k]) >> 6) != 0x2) return false;
    i += length;
  }
  return true;
}

bool hasControlCharacter(std::string_view text) {
  for (char c : text)
    if (static_cast<unsigned char>(c) < 0x20) return true;
  return false;
}

// Splits CSV text into records of fields. A quoted field may hold commas, "" for a quote
// and line breaks; a line break inside a card becomes a space. False on an unclosed quote.
bool readRecords(std::string_view text, std::vector<std::vector<std::string>>& records) {
  std::vector<std::string> record(1);
  bool quoted = false;
  for (std::size_t i = 0; i < text.size(); ++i) {
    const char c = text[i];
    std::string& field = record.back();
    if (quoted) {
      if (c == '"' && i + 1 < text.size() && text[i + 1] == '"') {
        field += '"';
        ++i;
      } else if (c == '"') {
        quoted = false;
      } else if (c == '\r' || c == '\n') {
        if (c == '\r' && i + 1 < text.size() && text[i + 1] == '\n') ++i;
        field += ' ';
      } else {
        field += c;
      }
    } else if (c == '"' && field.empty()) {
      quoted = true;
    } else if (c == ',') {
      record.emplace_back();
    } else if (c == '\r' || c == '\n') {
      if (c == '\r' && i + 1 < text.size() && text[i + 1] == '\n') ++i;
      records.push_back(std::move(record));
      record.assign(1, std::string());
    } else {
      field += c;
    }
  }
  if (quoted) return false;
  records.push_back(std::move(record));
  return true;
}

std::size_t countBlanks(const std::string& text) {
  std::size_t blanks = 0;
  for (std::size_t i = 0; i < text.size();) {
    std::size_t run = 0;
    while (i + run < text.size() && text[i + run] == '_') ++run;
    if (run >= 3) ++blanks;
    i += run > 0 ? run : 1;
  }
  return blanks;
}

}  // namespace

PromptCard CsvPack::prompt(std::string text) {
  const std::size_t blanks = countBlanks(text);
  PromptCard card;
  card.pick = static_cast<std::uint8_t>(blanks == 0 ? 1 : blanks);
  // Like the printed "Draw 2, Pick 3" cards.
  card.draw = card.pick == 3 ? 2 : 0;
  card.text = std::move(text);
  return card;
}

bool CsvPack::read(std::string_view text, CardPack& pack, std::string& error) {
  if (text.substr(0, 3) == "\xEF\xBB\xBF") text.remove_prefix(3);
  std::vector<std::vector<std::string>> records;
  if (!readRecords(text, records)) return fail(error, "unclosed quote");
  if (records.empty() || records[0] != std::vector<std::string>{"typ", "text"})
    return fail(error, "header must be typ,text");
  std::vector<PromptCard> prompts;
  std::vector<std::string> answers;
  for (std::size_t i = 1; i < records.size(); ++i) {
    std::vector<std::string>& record = records[i];
    if (record.size() == 1 && record[0].empty()) continue;  // blank line
    const std::string where = "record " + std::to_string(i) + ": ";
    if (record.size() != 2) return fail(error, where + "not a typ,text pair");
    std::string& card = record[1];
    while (!card.empty() && card.back() == ' ') card.pop_back();
    if (card.empty() || card.size() > kMaxCardBytes)
      return fail(error, where + "empty or too long");
    if (!validUtf8(card) || hasControlCharacter(card))
      return fail(error, where + "invalid characters");
    if (record[0] == "w") {
      answers.push_back(std::move(card));
    } else if (record[0] == "b") {
      if (countBlanks(card) > 3) return fail(error, where + "more than three blanks");
      prompts.push_back(prompt(std::move(card)));
    } else {
      return fail(error, where + "type must be b or w");
    }
  }
  if (prompts.empty() && answers.empty()) return fail(error, "no cards");
  for (PromptCard& card : prompts) pack.prompts.push_back(std::move(card));
  for (std::string& card : answers) pack.answers.push_back(std::move(card));
  return true;
}

}  // namespace caw

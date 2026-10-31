// Values go into SQL only through quote(); nothing here builds SQL from raw input.
#include "PostgresStore.h"

#include <pqxx/pqxx>

#include "log.h"
#include "net/Messages.h"
#include "session/PlayerWords.h"

namespace caw {
namespace {

constexpr const char* kSchema = R"SQL(
CREATE TABLE IF NOT EXISTS players (
  id text PRIMARY KEY,
  name text NOT NULL,
  word smallint NOT NULL,
  games integer NOT NULL DEFAULT 0,
  wins integer NOT NULL DEFAULT 0,
  rounds integer NOT NULL DEFAULT 0,
  last_seen timestamptz NOT NULL DEFAULT now()
);
CREATE TABLE IF NOT EXISTS winning_lines (
  id bigserial PRIMARY KEY,
  player_id text NOT NULL REFERENCES players(id) ON DELETE CASCADE,
  sentence text NOT NULL,
  created_at timestamptz NOT NULL DEFAULT now()
);
)SQL";

void recordEvent(pqxx::work& tx, const HistoryEvent& event) {
  const std::string id = tx.quote(event.playerId);
  tx.exec("INSERT INTO players (id, name, word, last_seen) VALUES (" + id + ", " +
          tx.quote(event.name) + ", " + std::to_string(event.word) +
          ", now()) ON CONFLICT (id) DO UPDATE SET name = EXCLUDED.name, word = EXCLUDED.word, "
          "last_seen = now()");
  if (event.kind == HistoryEvent::Kind::RoundWon) {
    tx.exec("UPDATE players SET rounds = rounds + 1 WHERE id = " + id);
    tx.exec("INSERT INTO winning_lines (player_id, sentence) VALUES (" + id + ", " +
            tx.quote(event.sentence) + ")");
  } else {
    tx.exec("UPDATE players SET games = games + 1, wins = wins + " +
            std::string(event.wonMatch ? "1" : "0") + " WHERE id = " + id);
  }
}

// No IPs, no tickets, nothing older than needed.
void dropOldRows(pqxx::work& tx) {
  tx.exec("DELETE FROM players WHERE last_seen < now() - interval '" +
          std::to_string(PostgresStore::kKeepDays) + " days'");
  tx.exec(
      "DELETE FROM winning_lines WHERE id <= (SELECT id FROM winning_lines ORDER BY id DESC "
      "OFFSET " +
      std::to_string(PostgresStore::kKeepLines) + " LIMIT 1)");
}

}  // namespace

// A dead database must not hold the persistence thread for long.
PostgresStore::PostgresStore(std::string url, FameCache& fame)
    : url_(url + (url.find('?') == std::string::npos ? "?" : "&") + "connect_timeout=2"),
      fame_(fame) {}

PostgresStore::~PostgresStore() = default;

bool PostgresStore::connect() {
  try {
    if (connection_ && connection_->is_open()) return true;
    connection_ = std::make_unique<pqxx::connection>(url_);
    if (!schemaReady_) {
      pqxx::work tx(*connection_);
      tx.exec(kSchema);
      tx.commit();
      schemaReady_ = true;
    }
    return true;
  } catch (const std::exception& error) {
    log::warn(std::string("Postgres unavailable: ") + error.what());
    connection_.reset();
    return false;
  }
}

bool PostgresStore::prepare() {
  if (!connect()) return false;
  refreshFame();
  return true;
}

// A Postgres restart leaves the old connection dead; one fresh attempt saves the batch.
bool PostgresStore::record(const std::vector<HistoryEvent>& events) {
  for (int attempt = 0; attempt < 2; ++attempt) {
    if (!connect()) return false;
    try {
      pqxx::work tx(*connection_);
      for (const HistoryEvent& event : events) recordEvent(tx, event);
      dropOldRows(tx);
      tx.commit();
      refreshFame();
      return true;
    } catch (const std::exception& error) {
      log::warn(std::string("Statistics not saved: ") + error.what());
      connection_.reset();
    }
  }
  return false;
}

void PostgresStore::refreshFame() {
  try {
    pqxx::work tx(*connection_);
    std::vector<FamePlayer> players;
    for (const auto& row :
         tx.exec("SELECT name, word, games, wins, rounds FROM players "
                 "ORDER BY wins DESC, rounds DESC, games DESC, last_seen DESC LIMIT 100"))
      players.push_back(
          {row[0].as<std::string>(), playerWord(static_cast<std::uint8_t>(row[1].as<int>())),
           row[2].as<std::uint32_t>(), row[3].as<std::uint32_t>(), row[4].as<std::uint32_t>()});
    std::vector<FameLine> lines;
    for (const auto& row :
         tx.exec("SELECT p.name, p.word, l.sentence FROM winning_lines l "
                 "JOIN players p ON p.id = l.player_id ORDER BY l.id DESC LIMIT 20"))
      lines.push_back({row[0].as<std::string>(),
                       playerWord(static_cast<std::uint8_t>(row[1].as<int>())),
                       row[2].as<std::string>()});
    fame_.set(Messages::fame(players, lines));
  } catch (const std::exception& error) {
    log::warn(std::string("Wall of Fame not refreshed: ") + error.what());
    connection_.reset();
  }
}

}  // namespace caw

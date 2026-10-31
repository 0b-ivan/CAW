// Statistics in Postgres: players by device id, rounds and matches won, recent winning
// sentences. Used only from the persistence thread; it owns its connection.
#pragma once

#include <memory>
#include <string>
#include <vector>

#include "store/FameCache.h"
#include "store/HistoryStore.h"

namespace pqxx {
class connection;
}

namespace caw {

class PostgresStore final : public HistoryStore {
 public:
  // Kept: players seen within this time, and this many recent winning sentences.
  static constexpr int kKeepDays = 180;
  static constexpr int kKeepLines = 500;

  PostgresStore(std::string url, FameCache& fame);
  ~PostgresStore() override;
  PostgresStore(const PostgresStore&) = delete;
  PostgresStore& operator=(const PostgresStore&) = delete;

  // Creates the tables if needed and fills the fame cache. False if Postgres is unreachable;
  // the game runs anyway and the statistics wait for the next successful record().
  bool prepare();
  bool record(const std::vector<HistoryEvent>& events) override;

 private:
  bool connect();
  void refreshFame();

  std::string url_;
  FameCache& fame_;
  std::unique_ptr<pqxx::connection> connection_;
  bool schemaReady_ = false;
};

}  // namespace caw

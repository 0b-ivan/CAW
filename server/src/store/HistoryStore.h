// Where finished rounds and matches go. An interface only so tests can replace Postgres.
#pragma once

#include <vector>

#include "store/HistoryEvent.h"

namespace caw {

class HistoryStore {
 public:
  virtual ~HistoryStore() = default;
  // Records the events in one go. False if the store is unavailable; they are then lost.
  virtual bool record(const std::vector<HistoryEvent>& events) = 0;
};

}  // namespace caw

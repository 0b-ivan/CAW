// Readiness probes for the two internal data services.
#pragma once
#include "config.h"

namespace caw {
bool healthy(const Config& config);
}  // namespace caw

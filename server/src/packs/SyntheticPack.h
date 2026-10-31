// Numbered placeholder cards: for tests, and for a server started without any pack files.
#pragma once

#include "packs/CardPack.h"

namespace caw {

// 60 prompts (every 10th a Pick 3 with Draw 2, every 4th a Pick 2) and 400 answers.
CardPack syntheticPack();

}  // namespace caw

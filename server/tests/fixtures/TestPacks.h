// One shared library with the numbered test pack, for every test that needs a session.
#pragma once

#include "packs/PackLibrary.h"
#include "packs/SyntheticPack.h"

inline caw::PackLibrary& testPacks() {
  static caw::PackLibrary packs;
  if (packs.packs().empty()) packs.add(caw::syntheticPack());
  return packs;
}

inline const std::vector<std::string> kTestPacks{"test"};

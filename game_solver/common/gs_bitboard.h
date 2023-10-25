#pragma once

#include "gs_configuration.h"
#include <bitset>

namespace gamesolver {

#if KILLALLGO
const int kBitboardSize = 361;
#elif HEX
const int kBitboardSize = 64;
#endif

typedef std::bitset<kBitboardSize> GSBitboard;

} // namespace gamesolver

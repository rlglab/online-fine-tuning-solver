#pragma once

#include "base_env.h"
#include <vector>

namespace gamesolver {

typedef uint64_t GSHashKey;

const int kMaxPossibleActions = 400;
extern GSHashKey turn_hash_key;
extern std::vector<std::vector<GSHashKey>> player_hash_key;
extern std::vector<std::vector<minizero::env::GamePair<GSHashKey>>> sequence_hash_key;

void initialize();
GSHashKey getPlayerHashKey(int position, minizero::env::Player p);
GSHashKey getMoveHashKey(int move, int position, minizero::env::Player p);

} // namespace gamesolver

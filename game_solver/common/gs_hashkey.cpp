#include "gs_hashkey.h"
#include "gs_configuration.h"
#include <random>

namespace gamesolver {

using namespace minizero;

GSHashKey turn_hash_key;
std::vector<std::vector<GSHashKey>> player_hash_key;
std::vector<std::vector<env::GamePair<GSHashKey>>> sequence_hash_key;

void initialize()
{
    std::mt19937_64 generator;
    generator.seed(0);
    turn_hash_key = generator();

    // player hash key
    player_hash_key.resize(kMaxPossibleActions);
    for (size_t i = 0; i < player_hash_key.size(); ++i) {
        player_hash_key[i].resize(static_cast<int>(env::Player::kPlayerSize));
        for (auto& key : player_hash_key[i]) { key = generator(); }
    }

    // sequence hash key
    sequence_hash_key.resize(2 * kMaxPossibleActions);
    for (int move = 0; move < 2 * kMaxPossibleActions; ++move) {
        sequence_hash_key[move].resize(kMaxPossibleActions);
        for (int pos = 0; pos < kMaxPossibleActions; ++pos) {
            sequence_hash_key[move][pos].get(env::Player::kPlayer1) = generator();
            sequence_hash_key[move][pos].get(env::Player::kPlayer2) = generator();
        }
    }
}

GSHashKey getPlayerHashKey(int position, env::Player p)
{
    assert(position >= 0 && position < kMaxPossibleActions);
    assert(p == env::Player::kPlayerNone || p == env::Player::kPlayer1 || p == env::Player::kPlayer2);
    return player_hash_key[position][static_cast<int>(p)];
}

GSHashKey getMoveHashKey(int move, int position, env::Player p)
{
    assert(move >= 0 && move < 2 * kMaxPossibleActions);
    assert(position >= 0 && position <= kMaxPossibleActions);
    assert(p == env::Player::kPlayer1 || p == env::Player::kPlayer2);
    return sequence_hash_key[move][position].get(p);
}

} // namespace gamesolver

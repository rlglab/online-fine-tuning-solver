#include "hex_knowledge_handler.h"
#include <algorithm>
#include <vector>

namespace gamesolver {

using namespace minizero;
using namespace minizero::env;
using namespace minizero::env::hex;

#if HEX

env::GamePair<GSBitboard> HexKnowledgeHandler::getStoneBitboard(const HexEnv& env) const
{
    env::GamePair<GSBitboard> stone_bitboard;
    const std::vector<Cell>& board = env.getBoard();
    for (size_t i = 0; i < board.size(); ++i) {
        if (board[i].player == Player::kPlayerNone) { continue; }
        stone_bitboard.get(board[i].player).set(i);
    }

    return stone_bitboard;
}

env::Player HexKnowledgeHandler::getWinner(const HexEnv& env)
{
    return env.getWinner() == env::charToPlayer(gamesolver::solved_player) ? env::charToPlayer(gamesolver::solved_player) : env::Player::kPlayerNone;
}

std::vector<GSHashKey> HexKnowledgeHandler::getHashKeySequence(const HexEnv& env)
{
    GSBitboard solved_stone_bitboard = getStoneBitboard(env).get(env::charToPlayer(gamesolver::solved_player));
    return getHashKeySequenceInBitboard(env, solved_stone_bitboard);
}

std::vector<GSHashKey> HexKnowledgeHandler::getHashKeySequenceInBitboard(const HexEnv& env, GSBitboard bitboard)
{
    std::vector<GSHashKey> hashkey_sequence;
    hashkey_sequence.push_back(0); // TODO: add 0 for improving the performance of timestamp, to remove this.
    GSHashKey solved_stone_hashkey;
    while (!bitboard.none()) {
        int pos = bitboard._Find_first();
        bitboard.reset(pos);
        solved_stone_hashkey ^= getPlayerHashKey(pos, env::charToPlayer(gamesolver::solved_player));
    }
    hashkey_sequence.push_back(solved_stone_hashkey);
    return hashkey_sequence;
}

#endif

} // namespace gamesolver

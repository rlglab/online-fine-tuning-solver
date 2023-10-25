#include "hex_rzone_handler.h"
#include <vector>

namespace gamesolver {

#if HEX
using namespace minizero;
using namespace minizero::env;
using namespace minizero::env::hex;

GSBitboard HexRZoneHandler::getWinnerRZoneBitboard(const minizero::env::hex::HexEnv& env)
{
    const std::vector<int>& winner_stone = env.getWinningStonesPosition();
    GSBitboard winner_rzone;
    for (size_t i = 0; i < winner_stone.size(); ++i) { winner_rzone.set(winner_stone[i]); }

    return winner_rzone;
}

GSBitboard HexRZoneHandler::getWinnerRZoneBitboard(const minizero::env::hex::HexEnv& env, const GSBitboard& child_bitboard, const Action& win_action)
{
    GSBitboard winner_rzone_bitboard = child_bitboard;
    winner_rzone_bitboard.set(win_action.getActionID());
    return winner_rzone_bitboard;
}

bool HexRZoneHandler::isRelevantMove(const minizero::env::hex::HexEnv& env, const GSBitboard& rzone_bitboard, const Action& action)
{
    return rzone_bitboard.test(action.getActionID());
}

GSBitboard HexRZoneHandler::getLoserRZoneBitboard(const minizero::env::hex::HexEnv& env, const GSBitboard& union_bitboard, const minizero::env::Player& player)
{
    return union_bitboard;
}

ZonePattern HexRZoneHandler::extractZonePattern(const minizero::env::hex::HexEnv& env, const GSBitboard& rzone_bitboard)
{
    env::GamePair<GSBitboard> stone_bitboard = knowledge_handler_->getStoneBitboard(env);
    GSBitboard black_bitboard_in_rz = stone_bitboard.get(Player::kPlayer1) & rzone_bitboard;
    GSBitboard white_bitboard_in_rz = stone_bitboard.get(Player::kPlayer2) & rzone_bitboard;

    return ZonePattern(rzone_bitboard, env::GamePair<GSBitboard>(black_bitboard_in_rz, white_bitboard_in_rz));
}

RZoneTTPattern HexRZoneHandler::extractRZoneTTPattern(const minizero::env::hex::HexEnv& env, GSMCTSNode* node, int winner_aciton_id /*= -1*/)
{
    assert(node->getRZoneDataIndex() != -1);

    RZoneTTPattern pattern;
    pattern.node_ = node;
    pattern.turn_ = env.getTurn();
    return pattern;
}

bool HexRZoneHandler::isRZonePatternMatch(const minizero::env::hex::HexEnv& env, const RZoneTTPattern& rzone_pattern, const TreeRZoneData& zone_table)
{
    const ZonePattern& zone_pattern = zone_table.getData(rzone_pattern.node_->getRZoneDataIndex());
    GSBitboard rzone_bitboard = zone_pattern.getRZone();
    GSBitboard black_bitboard_in_rz = zone_pattern.getRZoneStone(Player::kPlayer1);
    GSBitboard white_bitboard_in_rz = zone_pattern.getRZoneStone(Player::kPlayer2);
    env::GamePair<GSBitboard> stone_bitboard = knowledge_handler_->getStoneBitboard(env);

    if (rzone_pattern.turn_ != env.getTurn()) { return false; }
    if ((rzone_bitboard & stone_bitboard.get(Player::kPlayer1)) != black_bitboard_in_rz) { return false; }
    if ((rzone_bitboard & stone_bitboard.get(Player::kPlayer2)) != white_bitboard_in_rz) { return false; }

    return true;
}

#endif

} // namespace gamesolver

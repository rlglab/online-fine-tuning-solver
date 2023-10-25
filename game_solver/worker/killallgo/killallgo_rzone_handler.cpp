#include "killallgo_rzone_handler.h"
#include <vector>

namespace gamesolver {

using namespace minizero;
using namespace minizero::env;
using namespace minizero::env::go;
using namespace minizero::env::killallgo;

#if KILLALLGO
GSBitboard KillallGoRZoneHandler::getWinnerRZoneBitboard(const KillAllGoEnv& env)
{
    return env.getBensonBitboard().get(Player::kPlayer2);
}

GSBitboard KillallGoRZoneHandler::getWinnerRZoneBitboard(const KillAllGoEnv& env, const GSBitboard& child_bitboard, const Action& win_action)
{
    GoBitboard updated_rzone_bitboard = child_bitboard;
    GoBitboard move_influence_bitboard = getMoveInfluenceInRZ(env, child_bitboard, win_action);

    if (move_influence_bitboard.any()) {
        GoBitboard own_influence_block = move_influence_bitboard & env.getStoneBitboard().get(win_action.getPlayer());
        updated_rzone_bitboard = getMoveRZone(env, child_bitboard, own_influence_block);
        updated_rzone_bitboard |= move_influence_bitboard;
    }

    return updated_rzone_bitboard;
}

bool KillallGoRZoneHandler::isRelevantMove(const KillAllGoEnv& env, const GSBitboard& rzone_bitboard, const Action& action)
{
    GoBitboard influence_bitboard = getMoveInfluenceInRZ(env, rzone_bitboard, action);
    return influence_bitboard.any();
}

GSBitboard KillallGoRZoneHandler::getLoserRZoneBitboard(const KillAllGoEnv& env, const GSBitboard& union_bitboard, const Player& player)
{
    // update 2-liberties and suicidal R-zone
    GoBitboard before_bitboard = union_bitboard;
    GoBitboard after_bitboard;
    do {
        before_bitboard = getLegalizedRZone(env, before_bitboard, player);
        after_bitboard = getSuicidalRZone(env, before_bitboard, player);
        if (after_bitboard == before_bitboard) { break; }
        before_bitboard = after_bitboard;
    } while (1);

    return after_bitboard;
}

ZonePattern KillallGoRZoneHandler::extractZonePattern(const KillAllGoEnv& env, const GSBitboard& rzone_bitboard)
{
    GoBitboard black_bitboard = env.getStoneBitboard().get(Player::kPlayer1) & rzone_bitboard;
    GoBitboard white_bitboard = env.getStoneBitboard().get(Player::kPlayer2) & rzone_bitboard;
    return ZonePattern(rzone_bitboard, env::GamePair<GoBitboard>(black_bitboard, white_bitboard));
}

RZoneTTPattern KillallGoRZoneHandler::extractRZoneTTPattern(const KillAllGoEnv& env, GSMCTSNode* node, int winner_aciton_id /*= -1*/)
{
    assert(node->getRZoneDataIndex() != -1);

    RZoneTTPattern pattern;
    pattern.node_ = node;
    pattern.turn_ = env.getTurn();
    pattern.ko_position_ = knowledge_handler_->getWinByKoPosition(env, winner_aciton_id);
    return pattern;
}

bool KillallGoRZoneHandler::isRZonePatternMatch(const KillAllGoEnv& env, const RZoneTTPattern& tt_pattern, const TreeRZoneData& zone_table)
{
    const ZonePattern& zone_pattern = zone_table.getData(tt_pattern.node_->getRZoneDataIndex());
    GSBitboard rzone_bitboard = zone_pattern.getRZone();
    GSBitboard env_black_in_rz = rzone_bitboard & env.getStoneBitboard().get(Player::kPlayer1);
    GSBitboard env_white_in_rz = rzone_bitboard & env.getStoneBitboard().get(Player::kPlayer2);

    if (tt_pattern.turn_ != env.getTurn()) { return false; }
    if (env_black_in_rz != zone_pattern.getRZoneStone(Player::kPlayer1)) { return false; }
    if (env_white_in_rz != zone_pattern.getRZoneStone(Player::kPlayer2)) { return false; }
    if (!matchRZonePatternKoPosition(env, tt_pattern.ko_position_)) { return false; }
    if (tt_pattern.node_->isInLoop()) { return false; }

    return true;
}

GSBitboard KillallGoRZoneHandler::getMoveRZone(const KillAllGoEnv& env, const GoBitboard& rzone_bitboard, GoBitboard own_block_influence)
{
    GoBitboard result_bitboard = rzone_bitboard;
    // find own block that has no z-liberty in rzone_bitboard
    std::vector<const GoBlock*> own_blocks;
    while (!own_block_influence.none()) {
        int pos = own_block_influence._Find_first();
        own_block_influence.reset(pos);
        const GoBlock* block = env.getGrid(pos).getBlock();
        assert(block != nullptr);
        const GoBitboard& liberty_bitboard = block->getLibertyBitboard();
        // no intersection
        if ((liberty_bitboard & rzone_bitboard).none()) { own_blocks.push_back(block); }
        own_block_influence &= ~block->getGridBitboard();
    }

    if (own_blocks.size() == 1) {
        const GoBlock* block = own_blocks[0];
        const GoBitboard& liberty_bitboard = block->getLibertyBitboard();
        result_bitboard.set(liberty_bitboard._Find_first());
    } else if (own_blocks.size() > 1) {
        GoBitboard common_liberty_bitboard;
        common_liberty_bitboard = own_blocks[0]->getLibertyBitboard();
        for (unsigned int iBlock = 1; iBlock < own_blocks.size(); ++iBlock) {
            const GoBitboard& liberty_bitboard = own_blocks[iBlock]->getLibertyBitboard();
            common_liberty_bitboard &= liberty_bitboard;
        }
        result_bitboard |= common_liberty_bitboard;

        // add one liberty of the block which has no common liberties
        for (unsigned int iBlock = 0; iBlock < own_blocks.size(); ++iBlock) {
            const GoBitboard& liberty_bitboard = own_blocks[iBlock]->getLibertyBitboard();
            if ((common_liberty_bitboard & liberty_bitboard).any()) { continue; }
            result_bitboard.set(liberty_bitboard._Find_first());
        }
    }

    return result_bitboard;
}

GSBitboard KillallGoRZoneHandler::getMoveInfluenceInRZ(const KillAllGoEnv& env, const GoBitboard& rzone_bitboard, const Action& action)
{
    GoBitboard move_influecne;
    const Player player = action.getPlayer();
    const Player opp_player = getNextPlayer(player, kKillAllGoNumPlayer);
    if (knowledge_handler_->isCaptureMove(env, action)) {
        const GoGrid& grid = env.getGrid(action.getActionID());
        GoBitboard deadstone_bitboard;
        GoBitboard bitboard_checked;
        for (const auto& neighbor_pos : grid.getNeighbors()) {
            const GoGrid& nbr_grid = env.getGrid(neighbor_pos);
            if (nbr_grid.getPlayer() != opp_player) { continue; }
            if (bitboard_checked.test(neighbor_pos)) { continue; }

            const GoBlock* nbr_block = nbr_grid.getBlock();
            if (nbr_block->getNumLiberty() == 1 && (nbr_block->getGridBitboard() & rzone_bitboard).any()) {
                deadstone_bitboard |= nbr_block->getGridBitboard();
            }
            bitboard_checked |= nbr_block->getGridBitboard();
        }

        GoBitboard nbr_own_block_bitboard = env.dilateBitboard(deadstone_bitboard) & env.getStoneBitboard().get(player);
        while (!nbr_own_block_bitboard.none()) {
            int pos = nbr_own_block_bitboard._Find_first();
            nbr_own_block_bitboard.reset(pos);
            const GoBlock* own_block = env.getGrid(pos).getBlock();
            nbr_own_block_bitboard &= ~(own_block->getGridBitboard());
            move_influecne |= own_block->getGridBitboard();
        }
        move_influecne |= deadstone_bitboard;
        // if capture relevant block, the stone block after this move should be included
        if (deadstone_bitboard.any()) { move_influecne |= knowledge_handler_->getStoneBitBoardAfterPlay(env, action); }
    }

    if (!env.isPassAction(action)) {
        GoBitboard stone_bitboard_after_play = knowledge_handler_->getStoneBitBoardAfterPlay(env, action);
        if ((stone_bitboard_after_play & rzone_bitboard).any()) { move_influecne |= stone_bitboard_after_play; }
    }

    return move_influecne;
}

GSBitboard KillallGoRZoneHandler::getLegalizedRZone(const KillAllGoEnv& env, GoBitboard bitboard, const Player& player)
{
    GoBitboard result_bitboard = bitboard;
    // Legalize R-zone
    while (!bitboard.none()) {
        int pos = bitboard._Find_first();
        bitboard.reset(pos);
        const GoGrid& grid = env.getGrid(pos);
        if (grid.getPlayer() == Player::kPlayerNone) { continue; }

        const GoBlock* block = grid.getBlock();
        bitboard &= (~block->getGridBitboard());
        if (block->getPlayer() != player) { continue; }

        result_bitboard |= block->getGridBitboard();
        const GoBitboard& liberty_bitboard = block->getLibertyBitboard();
        int num_overlapped_lib = (liberty_bitboard & result_bitboard).count();
        if (num_overlapped_lib >= 2) { continue; }

        // numRequiredLib = 2 or 1
        int num_required_lib = 2 - num_overlapped_lib;
        GoBitboard remaining_liberty_bitboard = liberty_bitboard & (~result_bitboard);
        for (int i = 0; i < num_required_lib; i++) {
            if (remaining_liberty_bitboard.none()) { break; }

            int next_liberty = remaining_liberty_bitboard._Find_first();
            result_bitboard.set(next_liberty);
            remaining_liberty_bitboard.reset(next_liberty);
        }
    }

    return result_bitboard;
}

GSBitboard KillallGoRZoneHandler::getSuicidalRZone(const KillAllGoEnv& env, GoBitboard bitboard, const Player& player)
{
    Player opp_player = getNextPlayer(player, kKillAllGoNumPlayer);
    GoBitboard result_bitboard = bitboard;
    while (!bitboard.none()) {
        int pos = bitboard._Find_first();
        bitboard.reset(pos);
        const GoGrid& grid = env.getGrid(pos);
        if (grid.getPlayer() != Player::kPlayerNone) {
            bitboard &= ~(grid.getBlock()->getGridBitboard());
            continue;
        }
        KillAllGoAction action(pos, opp_player);
        if (!knowledge_handler_->isSuicidalMove(env, action)) { continue; }

        GoBitboard dead_stone_bitboard = knowledge_handler_->getStoneBitBoardAfterPlay(env, action);
        GoBitboard nbr_block_bitboard = env.dilateBitboard(dead_stone_bitboard) & env.getStoneBitboard().get(player);
        result_bitboard |= nbr_block_bitboard;
        result_bitboard |= dead_stone_bitboard;
    }

    return result_bitboard;
}

bool KillallGoRZoneHandler::matchRZonePatternKoPosition(const KillAllGoEnv& env, const int16_t& ko_position)
{
    if (ko_position == -1) { return true; }
    if (env.getGrid(ko_position).getPlayer() != env::Player::kPlayerNone) { return false; }

    KillAllGoAction ko_action(ko_position, env.getTurn());
    if (!knowledge_handler_->isEatKoMove(env, ko_action)) { return false; }

    if (env.getTurn() == Player::kPlayer1 && !env.isLegalAction(ko_action)) {
        return true;
    } else if (env.getTurn() == Player::kPlayer2 && env.isLegalAction(ko_action)) {
        return true;
    }

    return false;
}
#endif

} // namespace gamesolver

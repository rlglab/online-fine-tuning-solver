#include "killallgo_knowledge_handler.h"
#include "sgf_loader.h"
#include <algorithm>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace gamesolver {

using namespace minizero;
using namespace minizero::actor;
using namespace minizero::env;
using namespace minizero::env::go;
using namespace minizero::env::killallgo;
using namespace minizero::utils;

#if KILLALLGO
GSHashKey KillallGoKnowledgeHandler::getSequenceHashKey(const std::string& sgf_string)
{
    SGFLoader sgf_loader;
    sgf_loader.loadFromString(sgf_string);
    const std::vector<std::pair<std::vector<std::string>, std::string>>& actions = sgf_loader.getActions();
    GSHashKey sequence_hash_key = 0;
    for (size_t action_index = 0; action_index < actions.size(); ++action_index) {
        KillAllGoAction action(actions.at(action_index).first, stoi(sgf_loader.getTags().at("SZ")));
        sequence_hash_key ^= getMoveHashKey(action_index, action.getActionID(), action.getPlayer());
    }

    return sequence_hash_key;
}

GSHashKey KillallGoKnowledgeHandler::getSequenceHashKey(const KillAllGoEnv& env)
{
    GSHashKey sequence_hash_key = 0;
    for (size_t action_index = 0; action_index < env.getActionHistory().size(); ++action_index) {
        const KillAllGoAction& action = (env.getActionHistory().at(action_index));
        sequence_hash_key ^= getMoveHashKey(action_index, action.getActionID(), action.getPlayer());
    }

    return sequence_hash_key;
}

bool KillallGoKnowledgeHandler::isCaptureMove(const KillAllGoEnv& env, const KillAllGoAction& action)
{
    if (env.isPassAction(action)) { return false; }

    const Player player = action.getPlayer();
    const GoGrid& grid = env.getGrid(action.getActionID());
    assert(grid.getPlayer() == Player::kPlayerNone);

    for (const auto& neighbor_pos : grid.getNeighbors()) {
        const GoGrid& neighbor_grid = env.getGrid(neighbor_pos);
        const GoBlock* neighbor_block = neighbor_grid.getBlock();
        if (neighbor_block == nullptr) { continue; }
        if (neighbor_block->getPlayer() == player) { continue; }
        if (neighbor_block->getNumLiberty() == 1) { return true; }
    }

    return false;
}

bool KillallGoKnowledgeHandler::isSuicidalMove(const KillAllGoEnv& env, const KillAllGoAction& action)
{
    return (getNumLibertyAfterPlay(env, action) == 0);
}

bool KillallGoKnowledgeHandler::isEatKoMove(const KillAllGoEnv& env, const KillAllGoAction& action)
{
    if (env.isPassAction(action)) { return false; }

    const Player player = action.getPlayer();
    const GoGrid& grid = env.getGrid(action.getActionID());
    assert(grid.getPlayer() == Player::kPlayerNone);

    bool is_eat_ko = false;
    for (const auto& neighbor_pos : grid.getNeighbors()) {
        const GoGrid& neighbor_grid = env.getGrid(neighbor_pos);
        const GoBlock* neighbor_block = neighbor_grid.getBlock();

        if (neighbor_block == nullptr) { return false; }
        if (neighbor_block->getPlayer() == player) { return false; }
        if (neighbor_block->getNumLiberty() == 1) {
            if (neighbor_block->getNumGrid() > 1) { return false; }
            // lib==1 && num stone==1
            if (is_eat_ko) {
                return false;
            } else {
                is_eat_ko = true;
            }
        }
    }

    return is_eat_ko;
}

int KillallGoKnowledgeHandler::getWinByKoPosition(const KillAllGoEnv& env, const int& winner_action_id)
{
    if (env.getTurn() == env::Player::kPlayer1) {
        const KillAllGoAction& previous_action = env.getActionHistory().back();
        if (env.isPassAction(previous_action)) { return -1; }

        const GoGrid& grid = env.getGrid(previous_action.getActionID());
        for (const auto& neighbor_pos : grid.getNeighbors()) {
            const GoGrid& nbr_grid = env.getGrid(neighbor_pos);
            if (nbr_grid.getPlayer() != Player::kPlayerNone) { continue; }

            Player opp_turn = getNextPlayer(previous_action.getPlayer(), kKillAllGoNumPlayer);
            KillAllGoAction ko_action(neighbor_pos, opp_turn);
            if (isEatKoMove(env, ko_action) && !env.isLegalAction(ko_action)) { return ko_action.getActionID(); }
        }
    } else if (env.getTurn() == env::Player::kPlayer2 && isEatKoMove(env, KillAllGoAction(winner_action_id, env::Player::kPlayer2))) {
        return winner_action_id;
    }

    return -1;
}

GoBitboard KillallGoKnowledgeHandler::getStoneBitBoardAfterPlay(const KillAllGoEnv& env, const KillAllGoAction& action)
{
    assert(!env.isPassAction(action));

    const Player player = action.getPlayer();
    const GoGrid& grid = env.getGrid(action.getActionID());
    assert(grid.getPlayer() == Player::kPlayerNone);

    GoBitboard stone_bitboard_after_play;
    for (const auto& neighbor_pos : grid.getNeighbors()) {
        const GoGrid& nbr_grid = env.getGrid(neighbor_pos);
        if (nbr_grid.getPlayer() == player) { stone_bitboard_after_play |= nbr_grid.getBlock()->getGridBitboard(); }
    }

    stone_bitboard_after_play.set(action.getActionID());
    return stone_bitboard_after_play;
}

GoBitboard KillallGoKnowledgeHandler::getLibertyBitBoardAfterPlay(const KillAllGoEnv& env, const KillAllGoAction& action)
{
    assert(!env.isPassAction(action));
    assert(env.getGrid(action.getActionID()).getPlayer() == Player::kPlayerNone);

    const Player player = action.getPlayer();
    const Player opp_player = getNextPlayer(player, kKillAllGoNumPlayer);
    const GoGrid& grid = env.getGrid(action.getActionID());
    assert(grid.getPlayer() == Player::kPlayerNone);

    GoBitboard stone_bitboard = (env.getStoneBitboard().get(env::Player::kPlayer1) | env.getStoneBitboard().get(env::Player::kPlayer2));
    GoBitboard liberty_bitboard_after_play;
    liberty_bitboard_after_play.set(action.getActionID());
    liberty_bitboard_after_play = env.dilateBitboard(liberty_bitboard_after_play);

    for (const auto& neighbor_pos : grid.getNeighbors()) {
        const GoGrid& nbr_grid = env.getGrid(neighbor_pos);
        if (nbr_grid.getPlayer() == player) {
            liberty_bitboard_after_play |= env.dilateBitboard(nbr_grid.getBlock()->getGridBitboard());
        } else if (nbr_grid.getPlayer() == opp_player) {
            if (nbr_grid.getBlock()->getNumLiberty() == 1) {
                stone_bitboard &= ~(nbr_grid.getBlock()->getGridBitboard());
            }
        }
    }

    liberty_bitboard_after_play &= (~stone_bitboard);
    liberty_bitboard_after_play.reset(action.getActionID());
    return liberty_bitboard_after_play;
}

int KillallGoKnowledgeHandler::getNumLibertyAfterPlay(const KillAllGoEnv& env, const KillAllGoAction& action)
{
    return getLibertyBitBoardAfterPlay(env, action).count();
}

GSHashKey KillallGoKnowledgeHandler::getHashKeyAfterPlay(const KillAllGoEnv& env, const KillAllGoAction& action)
{
    assert(env.getGrid(action.getActionID()).getPlayer() == Player::kPlayerNone);
    assert(!isSuicidalMove(env, action));

    const Player player = action.getPlayer();
    const GoGrid& grid = env.getGrid(action.getActionID());
    GSHashKey new_hash_key = env.getHashKey() ^ go::getGoTurnHashKey() ^ go::getGoGridHashKey(action.getActionID(), player);
    GoBitboard check_neighbor_block_bitboard;
    for (const auto& neighbor_pos : grid.getNeighbors()) {
        const GoGrid& neighbor_grid = env.getGrid(neighbor_pos);
        if (neighbor_grid.getPlayer() == Player::kPlayerNone) {
            continue;
        } else {
            const GoBlock* neighbor_block = neighbor_grid.getBlock();
            if (check_neighbor_block_bitboard.test(neighbor_block->getID())) { continue; }
            check_neighbor_block_bitboard.set(neighbor_block->getID());
            if (neighbor_block->getPlayer() == player) { continue; }
            if (neighbor_block->getNumLiberty() == 1) { new_hash_key ^= neighbor_block->getHashKey(); }
        }
    }

    return new_hash_key;
}

GamePair<GoBitboard> KillallGoKnowledgeHandler::findSekiBitboard(const KillAllGoEnv& env, const KillAllGoAction& action)
{
    if (env.isPassAction(action) || !env.getBensonBitboard().get(Player::kPlayer2).none()) { return GamePair<GoBitboard>(); }

    const GoGrid& grid = env.getGrid(action.getActionID());
    assert(grid.getPlayer() != Player::kPlayerNone);

    const GoArea* seki_area = nullptr;
    if (grid.getPlayer() == Player::kPlayer1) {
        if (grid.getArea(Player::kPlayer2) && isEnclosedSeki(env, grid.getArea(Player::kPlayer2))) { seki_area = grid.getArea(Player::kPlayer2); }
    } else if (grid.getPlayer() == Player::kPlayer2) {
        const GoBlock* block = grid.getBlock();
        GoBitboard area_bitboard_id = block->getNeighborAreaIDBitboard();
        while (!area_bitboard_id.none()) {
            int area_id = area_bitboard_id._Find_first();
            area_bitboard_id.reset(area_id);

            const GoArea* area = &env.getArea(area_id);
            if (!isEnclosedSeki(env, area)) { continue; }
            seki_area = area;
            break;
        }
    }

    if (!seki_area) { return GamePair<GoBitboard>(); }
    GamePair<GoBitboard> seki_bitboard;
    seki_bitboard.get(Player::kPlayer2) |= seki_area->getAreaBitboard();
    seki_bitboard.get(Player::kPlayer2) |= env.getBlock(seki_area->getNeighborBlockIDBitboard()._Find_first()).getGridBitboard();
    return seki_bitboard;
}

bool KillallGoKnowledgeHandler::isEnclosedSeki(const KillAllGoEnv& env, const GoArea* area)
{
    assert(area && area->getPlayer() == Player::kPlayer2);
    if (area->getNeighborBlockIDBitboard().count() != 1) { return false; }
    if (area->getNumGrid() < 5 || area->getNumGrid() > 7) { return false; }

    const GoBlock* surrounding_block = &env.getBlock(area->getNeighborBlockIDBitboard()._Find_first());
    if ((surrounding_block->getLibertyBitboard() & area->getAreaBitboard()).count() != 2) { return false; }

    std::vector<const GoBlock*> inner_blocks;
    GoBitboard inner_stone_bitboard = area->getAreaBitboard() & env.getStoneBitboard().get(Player::kPlayer1);
    while (!inner_stone_bitboard.none()) {
        int pos = inner_stone_bitboard._Find_first();
        const GoBlock* block = env.getGrid(pos).getBlock();
        if (block->getNumLiberty() != 2) { return false; }
        inner_blocks.push_back(block);
        inner_stone_bitboard &= ~block->getGridBitboard();
    }
    if (inner_blocks.size() > 2) { return false; }

    if (inner_blocks.size() == 1) {
        const GoBlock* inner_block = inner_blocks[0];
        if (!(area->getAreaBitboard() & ~inner_block->getGridBitboard() & ~surrounding_block->getLibertyBitboard()).none()) { return false; }
    } else if (inner_blocks.size() == 2) {
        GoBitboard remaining_empty_bitboard = (area->getAreaBitboard() & ~surrounding_block->getLibertyBitboard() & ~env.getStoneBitboard().get(Player::kPlayer1));
        if (remaining_empty_bitboard.count() != 1) { return false; }
        if (env.getGrid(remaining_empty_bitboard._Find_first()).getNeighbors().size() != 2) { return false; }
        if ((inner_blocks[0]->getLibertyBitboard() | inner_blocks[1]->getLibertyBitboard()).count() == 2) { return false; }
    }
    return enclosedSekiSearch(env, surrounding_block, area, Player::kPlayer1);
}

bool KillallGoKnowledgeHandler::enclosedSekiSearch(const KillAllGoEnv& env, const GoBlock* block, const GoArea* area, Player turn)
{
    assert(block && area);

    if (!env.getBensonBitboard().get(Player::kPlayer2).none()) { // white win if is Benson
        return true;
    } else if ((block->getGridBitboard() & env.getStoneBitboard().get(Player::kPlayer2)).none()) { // black win if white block has been eaten
        return false;
    }

    int block_pos = block->getGridBitboard()._Find_first();
    GoBitboard area_bitboard = area->getAreaBitboard();
    // only white can choose to play PASS
    if (turn == Player::kPlayer2) { area_bitboard.set(kKillAllGoBoardSize * kKillAllGoBoardSize); }
    while (!area_bitboard.none()) {
        int pos = area_bitboard._Find_first();
        area_bitboard.reset(pos);

        KillAllGoAction action(pos, turn);
        if (!env.isLegalAction(action)) { continue; }

        KillAllGoEnv env_copy = env;
        env_copy.act(action);
        const GoBlock* new_block = env_copy.getGrid(block_pos).getBlock();
        if (turn == Player::kPlayer2 && new_block && (new_block->getLibertyBitboard() & area->getAreaBitboard()).count() < 2) { continue; }

        bool is_seki = enclosedSekiSearch(env_copy, block, area, getNextPlayer(turn, kKillAllGoNumPlayer));
        if (!is_seki && turn == Player::kPlayer1) {
            return false;
        } else if (is_seki && turn == Player::kPlayer2) {
            return true;
        }
    }
    return (turn == Player::kPlayer1 ? true : false);
}

env::Player KillallGoKnowledgeHandler::getWinner(const KillAllGoEnv& env)
{
    // we only consider the case for White winning currently
    return (env.getBensonBitboard().get(env::Player::kPlayer2).any() ? env::Player::kPlayer2 : env::Player::kPlayerNone);
}

std::vector<GSHashKey> KillallGoKnowledgeHandler::getHashKeySequence(const KillAllGoEnv& env)
{
    return getHashKeySequenceInBitboard(env, env.getStoneBitboard().get(env::charToPlayer(gamesolver::solved_player)));
}

std::vector<GSHashKey> KillallGoKnowledgeHandler::getHashKeySequenceInBitboard(const KillAllGoEnv& env, GSBitboard bitboard)
{
    std::vector<GSHashKey> hashkey_sequence;
    hashkey_sequence.push_back(0); // add 0 for improving the performance of timestamp
    while (!bitboard.none()) {
        int pos = bitboard._Find_first();
        const GoBlock* block = env.getGrid(pos).getBlock();
        assert(block && block->getPlayer() == env::Player::kPlayer2);
        bitboard &= ~block->getGridBitboard();
        hashkey_sequence.push_back(block->getHashKey());
    }
    std::sort(hashkey_sequence.begin(), hashkey_sequence.end());
    return hashkey_sequence;
}

void KillallGoKnowledgeHandler::findGHI(const KillAllGoEnv& env, std::vector<MCTSNode*>& node_path, std::shared_ptr<GSMCTS> mcts)
{
    const std::vector<GoHashKey>& hashkey_history = env.getHashKeyHistory();
    GoHashKey longest_loop_hash_key = 0;
    size_t virtual_hashkey_history_size = hashkey_history.size() + 1;
    size_t longest_loop_start_index = virtual_hashkey_history_size - 1;
    for (int pos = 0; pos < env.getBoardSize() * env.getBoardSize(); ++pos) {
        if (env.getGrid(pos).getPlayer() != env::Player::kPlayerNone) { continue; }
        const Action action(pos, env.getTurn());
        if (isSuicidalMove(env, action)) { continue; }

        GoHashKey hashkey_after_play = getHashKeyAfterPlay(env, action);
        if (!env.getHashTable().count(hashkey_after_play)) { continue; }

        // the repetitive_index is the longest loop index before leaf
        size_t repetitive_index = 0;
        for (size_t i = 0; i < hashkey_history.size() || (i + 1) <= longest_loop_start_index; ++i) {
            if (hashkey_history[i] != hashkey_after_play) { continue; }
            repetitive_index = i + 1;
            break;
        }
        if (repetitive_index >= longest_loop_start_index) { continue; }

        longest_loop_hash_key = hashkey_after_play;
        longest_loop_start_index = repetitive_index;
    }

    if (longest_loop_hash_key == 0) { return; }

    int node_path_ghi_start_index = longest_loop_start_index - (virtual_hashkey_history_size - node_path.size());
    if (node_path_ghi_start_index < 0) { mcts->addGHINodes(static_cast<GSMCTSNode*>(node_path.back()), node_path_ghi_start_index); }
    node_path_ghi_start_index = node_path_ghi_start_index + 1 > 0 ? node_path_ghi_start_index + 1 : 0;
    for (size_t i = node_path_ghi_start_index; i < node_path.size(); ++i) { static_cast<GSMCTSNode*>(node_path[i])->setInLoop(true); }
    for (auto& node : node_path) { static_cast<GSMCTSNode*>(node)->setGHI(true); }
}

std::vector<env::GamePair<GSBitboard>> KillallGoKnowledgeHandler::getAncestorPositions(const KillAllGoEnv& env, const std::vector<MCTSNode*>& node_path)
{
    std::vector<env::GamePair<GSBitboard>> ancestor_positions;
    if (node_path.size() == 1) { return ancestor_positions; }

    KillAllGoEnv env_copy = env;
    env::Player solved_player = env::charToPlayer(gamesolver::solved_player);
    if (node_path[0]->getAction().getPlayer() == solved_player) { ancestor_positions.push_back(env_copy.getStoneBitboard()); } // the root position
    // do not push back the last position since it is the current node
    for (size_t i = 1; i < node_path.size() - 1; ++i) {
        env_copy.act(node_path[i]->getAction());
        if (node_path[i]->getAction().getPlayer() != solved_player) { continue; }
        ancestor_positions.push_back(env_copy.getStoneBitboard());
    }

    return ancestor_positions;
}
#endif

} // namespace gamesolver

#include "base_solver.h"
#include "tree_logger.h"
#include <memory>
#include <unordered_map>
#include <vector>

namespace gamesolver {

using namespace minizero;
using namespace minizero::env;
using namespace minizero::actor;
using namespace minizero::network;

void BaseSolver::reset()
{
    actor::ZeroActor::reset();
    is_idle_ = true;
    solver_job_.reset();
    if (!rzone_handler_) { rzone_handler_ = createRZoneHandler(); }
    if (!knowledge_handler_) {
        knowledge_handler_ = createKnowledgeHandler();
        rzone_tt_handler_.setKnowledgeHandler(knowledge_handler_);
        rzone_tt_handler_.setRZoneHandler(rzone_handler_);
    }
}

void BaseSolver::resetSearch()
{
    GSActor::resetSearch();
    rzone_tt_handler_.clear();
}

void BaseSolver::setSolverJob(const SolverJob& solver_job)
{
    reset();
    is_idle_ = false;
    solver_job_ = solver_job;
    utils::SGFLoader sgf_loader;
    sgf_loader.loadFromString(solver_job_.sgf_);
    for (auto& action_pair : sgf_loader.getActions()) {
        act(Action(action_pair.first, stoi(sgf_loader.getTags().at("SZ")))); // TODO: fix this to support other games
    }
}

Action BaseSolver::think(bool with_play /*= false*/, bool display_board /*= false*/)
{
    is_idle_ = false;
    resetSearch();
    while (!isSearchDone()) {
        beforeNNEvaluation();
        if (pcn_network_->getBatchSize() == 0) { continue; }
        afterNNEvaluation(pcn_network_->forward()[getNNEvaluationBatchIndex()]);
    }
    return Action();
}

void BaseSolver::beforeNNEvaluation()
{
    mcts_search_data_.node_path_ = selection();
    if (isSearchDone()) {
        handleSearchDone();
        nn_evaluation_batch_id_ = -1;
        return;
    }
    Environment env_transition = getEnvironmentTransition(mcts_search_data_.node_path_);
    nn_evaluation_batch_id_ = pcn_network_->pushBack(env_transition.getFeatures());
}

void BaseSolver::afterNNEvaluation(const std::shared_ptr<NetworkOutput>& network_output)
{
    GSActor::afterNNEvaluation(network_output);

    Environment env_transition = getEnvironmentTransition(mcts_search_data_.node_path_);
    env::Player winner = knowledge_handler_->getWinner(env_transition);
    if (winner != env::Player::kPlayerNone) {
        GSBitboard rzone_bitboard = rzone_handler_->getWinnerRZoneBitboard(env_transition);
        const MCTSNode* leaf = mcts_search_data_.node_path_.back();
        SolverStatus status = (leaf->getAction().getPlayer() == winner ? SolverStatus::kSolverWin : SolverStatus::kSolverLoss);
        updateSolverStatus(status, mcts_search_data_.node_path_, rzone_bitboard);
    }
    if (isSearchDone()) { handleSearchDone(); }
}

void BaseSolver::handleSearchDone()
{
    assert(isSearchDone());
    solver_job_.solver_status_ = getMCTS()->getRootNode()->getSolverStatus();
    if (getMCTS()->getRootNode()->getRZoneDataIndex() != -1) { solver_job_.rzone_bitboard_ = getMCTS()->getTreeRZoneData().getData(getMCTS()->getRootNode()->getRZoneDataIndex()).getRZone(); }
    solver_job_.nodes_ = getMCTS()->getRootNode()->getCount();
    if (gamesolver::use_ghi_check) { collectGHIInfo(getMCTS()->getRootNode(), solver_job_.ghi_data_); }
    if (gamesolver::log_solver_sgf) { TreeLogger::saveTreeStringToFile(gamesolver::solver_output_directory + "/tree_" + std::to_string(solver_job_.job_id_) + ".sgf", env_, getMCTS()); }
}

std::vector<minizero::actor::MCTSNode*> BaseSolver::selection()
{
    minizero::actor::MCTSNode* node = getMCTS()->getRootNode();
    std::vector<minizero::actor::MCTSNode*> node_path{node};

    Environment env_transition = env_;
    if (findTTAndUpdateSolverStatus(env_transition, node_path)) { return node_path; }
    while (!node->isLeaf()) {
        node = getMCTS()->selectChildByPUCTScore(node);
        node_path.push_back(node);
        env_transition.act(node->getAction());
        if (findTTAndUpdateSolverStatus(env_transition, node_path)) {
            if (isSearchDone()) { break; }
            node = getMCTS()->getRootNode();
            node_path = {node};
            env_transition = env_;
        }
    }
    return node_path;
}

void BaseSolver::updateSolverStatus(SolverStatus status, std::vector<minizero::actor::MCTSNode*> node_path, const GSBitboard& rzone_bitboard)
{
    assert(status != SolverStatus::kSolverUnknown);

    Environment leaf_env = getEnvironmentTransition(node_path);
    GSMCTSNode* leaf = static_cast<GSMCTSNode*>(node_path.back());
    leaf->setSolverStatus(status);
    setNodeRZone(leaf, rzone_handler_->extractZonePattern(leaf_env, rzone_bitboard));

    while (node_path.size() >= 2) {
        GSMCTSNode* node = static_cast<GSMCTSNode*>(node_path[node_path.size() - 1]);
        GSMCTSNode* parent = static_cast<GSMCTSNode*>(node_path[node_path.size() - 2]);

        node_path.pop_back();
        Environment env_transition = getEnvironmentTransition(node_path);
        if (node->getSolverStatus() == SolverStatus::kSolverWin) {
            parent->setSolverStatus(SolverStatus::kSolverLoss);
            if (gamesolver::use_rzone) { updateWinnerRZone(env_transition, parent, node); }
        } else if (node->getSolverStatus() == SolverStatus::kSolverLoss) {
            if (gamesolver::use_rzone) { pruneNodesOutsideRZone(env_transition, parent, node); }
            if (isAllChildrenSolutionLoss(parent)) {
                parent->setSolverStatus(SolverStatus::kSolverWin);
                if (gamesolver::use_rzone) {
                    updateLoserRZone(env_transition, parent);
                    if (gamesolver::use_ghi_check) { knowledge_handler_->findGHI(env_transition, node_path, getMCTS()); }
                }
            } else {
                break;
            }
        }
    }
}

void BaseSolver::updateWinnerRZone(const Environment& env, GSMCTSNode* parent, const GSMCTSNode* child)
{
    GSBitboard child_rzone_bitboard = getMCTS()->getTreeRZoneData().getData(child->getRZoneDataIndex()).getRZone();
    GSBitboard parent_rzone_bitboard = rzone_handler_->getWinnerRZoneBitboard(env, child_rzone_bitboard, child->getAction());
    setNodeRZone(parent, rzone_handler_->extractZonePattern(env, parent_rzone_bitboard));
    storeTT(parent, env, child->getAction().getActionID());
}

void BaseSolver::pruneNodesOutsideRZone(const Environment& env, const GSMCTSNode* parent, GSMCTSNode* node)
{
    int rzone_index = node->getRZoneDataIndex();
    if (rzone_index == -1) { return; }
    const GSBitboard& child_rzone_bitboard = getMCTS()->getTreeRZoneData().getData(rzone_index).getRZone();
    if (rzone_handler_->isRelevantMove(env, child_rzone_bitboard, node->getAction())) { return; }

    for (int i = 0; i < parent->getNumChildren(); ++i) {
        GSMCTSNode* child = parent->getChild(i);
        if (child->getSolverStatus() != SolverStatus::kSolverUnknown) { continue; }

        if (!child_rzone_bitboard.test(child->getAction().getActionID())) {
            child->setSolverStatus(SolverStatus::kSolverLoss);
            child->setEqualLossNode(node);
        }
    }
}

bool BaseSolver::isAllChildrenSolutionLoss(const GSMCTSNode* node) const
{
    for (int i = 0; i < node->getNumChildren(); ++i) {
        const GSMCTSNode* child = node->getChild(i);
        if (child->getSolverStatus() != SolverStatus::kSolverLoss) { return false; }
    }
    return true;
}

void BaseSolver::updateLoserRZone(const Environment& env, GSMCTSNode* parent)
{
    GSBitboard union_bitboard;
    for (int i = 0; i < parent->getNumChildren(); ++i) {
        GSMCTSNode* child = parent->getChild(i);
        if (child->getRZoneDataIndex() == -1) { continue; }

        int rzone_index = child->getRZoneDataIndex();
        GSBitboard child_rzone_bitboard = getMCTS()->getTreeRZoneData().getData(rzone_index).getRZone();
        union_bitboard |= child_rzone_bitboard;
    }

    GSBitboard parent_rzone_bitboard = rzone_handler_->getLoserRZoneBitboard(env, union_bitboard, parent->getAction().getPlayer());
    setNodeRZone(parent, rzone_handler_->extractZonePattern(env, parent_rzone_bitboard));
    storeTT(parent, env);
}

void BaseSolver::setNodeRZone(GSMCTSNode* node, const ZonePattern& zone_pattern)
{
    int rzone_index = node->getMatchTTNode() != nullptr ? node->getMatchTTNode()->getRZoneDataIndex() : getMCTS()->getTreeRZoneData().store(zone_pattern);
    node->setRZoneDataIndex(rzone_index);
}

bool BaseSolver::findTTAndUpdateSolverStatus(const Environment& env, const std::vector<MCTSNode*>& node_path)
{
    RZoneTTPattern pattern;
    GSMCTSNode* node = static_cast<GSMCTSNode*>(node_path.back());
    if (rzone_tt_handler_.lookupTT(env, node, pattern, getMCTS()->getTreeRZoneData())) {
        bool can_use_tt = true;
        if (gamesolver::use_ghi_check) {
            std::vector<env::GamePair<GSBitboard>> ancestor_positions = knowledge_handler_->getAncestorPositions(env_, node_path);
            if (!isValidSimulation(pattern.node_, ancestor_positions)) { can_use_tt = false; }
        }
        if (can_use_tt) {
            node->setMatchTTNode(pattern.node_);
            updateSolverStatus(pattern.node_->getSolverStatus(), node_path, getMCTS()->getTreeRZoneData().getData(pattern.node_->getRZoneDataIndex()).getRZone());
            return true;
        }
    }

    return false;
}

void BaseSolver::storeTT(GSMCTSNode* node, const Environment& env, int winner_aciton_id)
{
    if (!gamesolver::use_block_tt && !gamesolver::use_grid_tt) { return; }
    if (node->isInLoop()) { return; }
    rzone_tt_handler_.storeTT(env, rzone_handler_->extractRZoneTTPattern(env, node, winner_aciton_id), getMCTS()->getTreeRZoneData());
}

bool BaseSolver::isValidSimulation(const GSMCTSNode* node, const std::vector<env::GamePair<GSBitboard>>& ancestor_positions) const
{
    if (!node->isSolved() || !node->isGHI() || node->getEqualLossNode() != nullptr) { return true; }

    if (node->isInLoop() && node->getAction().getPlayer() == env::charToPlayer(gamesolver::solved_player)) {
        if (hasRZonePatternInPositions(getMCTS()->getTreeRZoneData().getData(node->getRZoneDataIndex()), ancestor_positions)) { return false; }
    }

    for (int i = 0; i < node->getNumChildren(); ++i) {
        GSMCTSNode* next_node = node->getChild(i)->getMatchTTNode() != nullptr
                                    ? node->getChild(i)->getMatchTTNode()
                                    : node->getChild(i);
        if (!isValidSimulation(next_node, ancestor_positions)) { return false; }
    }

    return true;
}

void BaseSolver::collectGHIInfo(GSMCTSNode* node, GHIData& ghi_data)
{
    if (!node->isSolved() || !node->isGHI() || node->getEqualLossNode() != nullptr) { return; }

    if (node->isInLoop() && node->getAction().getPlayer() == env::charToPlayer(gamesolver::solved_player)) {
        ghi_data.addPattern(getMCTS()->getTreeRZoneData().getData(node->getRZoneDataIndex()));
        const std::unordered_map<GSMCTSNode*, int>& ghi_node_map = getMCTS()->getGHINodeMap();
        if (ghi_node_map.count(node) && ghi_node_map.find(node)->second < ghi_data.getMinLoopOffsetBeforeRoot()) { ghi_data.setMinLoopOffsetBeforeRoot(ghi_node_map.find(node)->second); }
    }

    for (int i = 0; i < node->getNumChildren(); ++i) {
        GSMCTSNode* next_node = node->getChild(i)->getMatchTTNode() != nullptr
                                    ? node->getChild(i)->getMatchTTNode()
                                    : node->getChild(i);
        collectGHIInfo(next_node, ghi_data);
    }
}

bool BaseSolver::hasRZonePatternInPositions(const ZonePattern& pattern, const std::vector<env::GamePair<GSBitboard>>& ancestor_positions) const
{
    for (size_t position_index = 0; position_index < ancestor_positions.size(); ++position_index) {
        auto position = ancestor_positions.at(position_index);
        GSBitboard black_in_zone = position.get(env::Player::kPlayer1) & pattern.getRZone();
        GSBitboard white_in_zone = position.get(env::Player::kPlayer2) & pattern.getRZone();
        if (black_in_zone == pattern.getRZoneStone(env::Player::kPlayer1) && white_in_zone == pattern.getRZoneStone(env::Player::kPlayer2)) { return true; }
    }

    return false;
}

} // namespace gamesolver

#include "manager.h"
#include "gs_configuration.h"
#include "sgf_loader.h"
#include "utils.h"
#include <algorithm>
#include <boost/thread.hpp>

namespace gamesolver {

using namespace minizero;
using namespace minizero::actor;

void Manager::reset()
{
    BaseSolver::reset();
    quit_ = false;
    job_handler_.removeJobs(this);
}

void Manager::resetSearch()
{
    BaseSolver::resetSearch();
    JobResultDeque::clear();
    recent_selection_path_.reset();
}

void Manager::solve()
{
    resetSearch();
    while (!isSearchDone()) {
        beforeNNEvaluation();
        std::shared_ptr<network::NetworkOutput> network_output = ((pcn_network_->getBatchSize() > 0) ? pcn_network_->forward()[getNNEvaluationBatchIndex()] : nullptr);
        afterNNEvaluation(network_output);
        handleJobCommands();
        broadcastCriticalPositions();
    }
    job_handler_.removeJobs(this);
}

void Manager::beforeNNEvaluation()
{
    BaseSolver::beforeNNEvaluation();
    if (isSearchDone()) { return; }
    for (auto& node : mcts_search_data_.node_path_) { node->addVirtualLoss(); }
}

void Manager::afterNNEvaluation(const std::shared_ptr<network::NetworkOutput>& network_output)
{
    if (network_output) {
        std::shared_ptr<ProofCostNetworkOutput> pcn_output = std::static_pointer_cast<ProofCostNetworkOutput>(network_output);
        const std::vector<MCTSNode*>& node_path = mcts_search_data_.node_path_;
        Environment env_transition = getEnvironmentTransition(node_path);
        MCTSNode* leaf = node_path.back();
        if (static_cast<GSMCTSNode*>(leaf)->isVirtualSolved()) {
        } else if (!env_transition.isTerminal() &&
                   leaf->getCount() == 0 &&
                   (!gamesolver::manager_send_and_player_job || (gamesolver::manager_send_and_player_job && leaf->getAction().getPlayer() == env::charToPlayer(gamesolver::solved_player))) &&
                   pcn_output->value_n_ < gamesolver::manager_pcn_value_threshold) {
            while (!job_handler_.hasIdleSolvers()) {
                handleSolverJobResults();
                boost::this_thread::sleep(boost::posix_time::milliseconds(50));
            }
            addVirtualSolvedNode(leaf, (node_path.size() >= 2 ? node_path[node_path.size() - 2] : nullptr));
            job_handler_.addJob(this, leaf, SolverJob(getSolverJobSgf(node_path), pcn_output->value_n_, node_path));
            job_handler_.log(getMCTS()->getRootNode()->isVirtualSolved() ? "is_root_virtual_solved_1" : "is_root_virtual_solved_0");
        } else {
            BaseSolver::afterNNEvaluation(network_output);
            for (auto& node : node_path) { node->removeVirtualLoss(); }
        }
    }
    if (!isSearchDone()) { handleSolverJobResults(); }
}

void Manager::RecentSelectionPath::TrieNode::reset(const minizero::actor::MCTSNode* node)
{
    count_ = 0;
    node_ = node;
    children_.clear();
}

Manager::RecentSelectionPath::RecentSelectionPath()
{
    nodes_.resize(gamesolver::manager_critical_positions_n * 30); // here we assume the maximum number of average selection depth is 30
    reset();
}

void Manager::RecentSelectionPath::reset()
{
    node_count_ = 0;
    getRoot()->reset(nullptr);
}

void Manager::RecentSelectionPath::addSelectionPath(const std::vector<MCTSNode*>& node_path)
{
    TrieNode* trie_node = getRoot();
    for (size_t i = 1; i < node_path.size(); ++i) {
        const MCTSNode* node = node_path[i];
        const int action_id = node->getAction().getActionID();
        if (!trie_node->children_.count(action_id)) {
            trie_node->children_[action_id] = newNode();
            trie_node->children_[action_id]->reset(node);
        }
        ++trie_node->count_;
        trie_node = trie_node->children_[action_id];
    }
}

std::string Manager::RecentSelectionPath::summarize(std::string& prefix, TrieNode* node) const
{
    std::string res;
    float count_threshold = getRoot()->count_ * static_cast<float>(gamesolver::manager_critical_positions_m) / gamesolver::manager_critical_positions_n;
    for (const auto& p : node->children_) {
        const auto& child = p.second;
        if (child->count_ < count_threshold) { continue; }

        std::string sgf_move = ";";
        sgf_move += env::playerToChar(child->node_->getAction().getPlayer());
        sgf_move += "[" + utils::SGFLoader::actionIDToSGFString(p.first, gamesolver::env_board_size) + "]";
        prefix += sgf_move;
        res += summarize(prefix, child);
        prefix.erase(prefix.length() - sgf_move.length());
    }

    if (res.empty() && node != getRoot() && !reinterpret_cast<const GSMCTSNode*>(node->node_)->isSolved()) { res = " " + prefix + ")"; }
    return res;
}

std::vector<MCTSNode*> Manager::selection()
{
    MCTSNode* node = getMCTS()->getRootNode();
    std::vector<MCTSNode*> node_path{node};

    Environment env_transition = env_;
    if (findTTAndUpdateSolverStatus(env_transition, node_path)) { return node_path; }
    while (!node->isLeaf()) {
        MCTSNode* next_node = ((!getMCTS()->getRootNode()->isVirtualSolved() && node->getAction().getPlayer() == env::charToPlayer(gamesolver::solved_player))
                                   ? getMCTS()->selectChildByPUCTScore(node, (node->getCount() >= gamesolver::manager_top_k_selection ? gamesolver::manager_top_k_selection : 1), true)
                                   : getMCTS()->selectChildByPUCTScore(node));
        if (!next_node) {
            addVirtualSolvedNode(node, (node_path.size() >= 2 ? node_path[node_path.size() - 2] : nullptr));
            node = getMCTS()->getRootNode();
            node_path = {node};
            env_transition = env_;
            continue;
        }
        node = next_node;
        node_path.push_back(node);

        env_transition.act(node->getAction());
        if (findTTAndUpdateSolverStatus(env_transition, node_path)) {
            if (isSearchDone()) { break; }
            node = getMCTS()->getRootNode();
            node_path = {node};
            env_transition = env_;
            continue;
        }
    }
    if (gamesolver::use_online_fine_tuning && gamesolver::use_critical_positions && !env_transition.isTerminal()) { recent_selection_path_.addSelectionPath(node_path); }
    return node_path;
}

bool Manager::isValidSimulation(const GSMCTSNode* node, const std::vector<env::GamePair<GSBitboard>>& ancestor_positions) const
{
    if (!node->isSolved() || !node->isGHI() || node->getEqualLossNode() != nullptr) { return true; }

    if (node->isInLoop() && node->getAction().getPlayer() == env::charToPlayer(gamesolver::solved_player)) {
        if (hasRZonePatternInPositions(getMCTS()->getTreeRZoneData().getData(node->getRZoneDataIndex()), ancestor_positions)) { return false; }
    }

    if (node->getGHIIndex() != -1) {
        const GHIData& ghi_data = getMCTS()->getTreeGHIData().getData(node->getGHIIndex());
        for (auto& pattern : ghi_data.getPatterns()) {
            if (hasRZonePatternInPositions(pattern, ancestor_positions)) { return false; }
        }
    }

    for (int i = 0; i < node->getNumChildren(); ++i) {
        GSMCTSNode* next_node = node->getChild(i)->getMatchTTNode() != nullptr
                                    ? node->getChild(i)->getMatchTTNode()
                                    : node->getChild(i);
        if (!isValidSimulation(next_node, ancestor_positions)) { return false; }
    }

    return true;
}

void Manager::addVirtualSolvedNode(minizero::actor::MCTSNode* child, minizero::actor::MCTSNode* parent)
{
    if (!gamesolver::use_virtual_solved) { return; }
    static_cast<GSMCTSNode*>(child)->setVirtualSolved(true);
    if (child->getAction().getPlayer() == env::charToPlayer(gamesolver::solved_player) && parent) { static_cast<GSMCTSNode*>(parent)->setVirtualSolved(true); }
}

void Manager::handleSolverJobResults()
{
    SolverJob job_result;
    std::string solved_sgf_message = "";
    while (popJobResult(job_result)) {
        // remove virtual loss and virtual solved
        const std::vector<minizero::actor::MCTSNode*>& node_path = job_result.node_path_;
        int num_virtual_loss = node_path.back()->getVirtualLoss();
        for (const auto& node : node_path) { node->removeVirtualLoss(num_virtual_loss); }
        static_cast<GSMCTSNode*>(node_path.back())->setVirtualSolved(false);

        // check: skip already solved?
        if (gamesolver::use_online_fine_tuning && gamesolver::use_solved_positions && job_result.solver_status_ != SolverStatus::kSolverUnknown) {
            solved_sgf_message += " " + getSolverJobSgf(node_path);
        }

        // check whether the job is already solved
        bool is_already_solved = false;
        for (const auto& node : node_path) {
            if (!static_cast<GSMCTSNode*>(node)->isSolved()) { continue; }
            is_already_solved = true;
            break;
        }
        if (is_already_solved) { job_handler_.log("already solved"); }
        if (is_already_solved) { continue; }

        // update rzone
        if (job_result.solver_status_ == SolverStatus::kSolverUnknown) { // returning job is unsolved
            // TODO: expand?
            getMCTS()->backup(node_path, gamesolver::manager_pcn_value_threshold);
            for (const auto& node : node_path) { static_cast<GSMCTSNode*>(node)->setVirtualSolved(false); }
            job_handler_.log("unsolved!");
        } else {
            int value = (((node_path.back()->getAction().getPlayer() != env::charToPlayer(gamesolver::solved_player) && job_result.solver_status_ == SolverStatus::kSolverLoss) ||
                          (node_path.back()->getAction().getPlayer() == env::charToPlayer(gamesolver::solved_player) && job_result.solver_status_ == SolverStatus::kSolverWin))
                             ? 0
                             : config::nn_discrete_value_size);
            if (value != 0) { job_handler_.log("black wins!"); }
            getMCTS()->backup(node_path, value);
            updateSolverStatus(job_result.solver_status_, node_path, job_result.rzone_bitboard_);
            updateGHIData(job_result);
        }
    }
    if (isSearchDone()) { handleSearchDone(); }
    if (!solved_sgf_message.empty()) { job_handler_.outputAsync("solver solved_sgf" + solved_sgf_message); }
}

void Manager::handleJobCommands()
{
    std::string job_command;
    while (job_handler_.popCommand(job_command)) {
        if (job_command.find("load_model") == 0) {
            std::vector<std::string> args = utils::stringToVector(job_command);
            assert(args.size() == 2);
            config::nn_file_name = args[1];
            pcn_network_->loadModel(config::nn_file_name, pcn_network_->getGPUID());
        } else if (job_command.find("quit") == 0) {
            quit_ = true;
        }
    }
}

std::string Manager::getSolverJobSgf(const std::vector<MCTSNode*>& node_path)
{
    std::ostringstream oss;
    oss << "(;FF[4]CA[UTF-8]SZ[" << gamesolver::env_board_size << "]";
    for (const auto& action : env_.getActionHistory()) { // env action
        oss << ";" << env::playerToChar(action.getPlayer())
            << "[" << utils::SGFLoader::actionIDToSGFString(action.getActionID(), gamesolver::env_board_size) << "]";
    }
    for (size_t i = 1; i < node_path.size(); ++i) { // node action
        const MCTSNode* node = node_path[i];
        oss << ";" << env::playerToChar(node->getAction().getPlayer())
            << "[" << utils::SGFLoader::actionIDToSGFString(node->getAction().getActionID(), gamesolver::env_board_size) << "]";
    }
    oss << ")";
    return oss.str();
}

void Manager::updateGHIData(const SolverJob& job_result)
{
    if (job_result.ghi_data_.empty()) { return; }

    const std::vector<minizero::actor::MCTSNode*>& node_path = job_result.node_path_;
    static_cast<GSMCTSNode*>(node_path.back())->setGHIIndex(getMCTS()->getTreeGHIData().store(job_result.ghi_data_));
    for (const auto& node : node_path) { static_cast<GSMCTSNode*>(node)->setGHI(true); }
    int start_loop_index = std::max(0, static_cast<int>(node_path.size()) + job_result.ghi_data_.getMinLoopOffsetBeforeRoot());
    // index start from the next node of repetitive node
    for (size_t index = start_loop_index; index < node_path.size(); ++index) { static_cast<GSMCTSNode*>(node_path[index])->setInLoop(true); }
}

void Manager::broadcastCriticalPositions()
{
    if (!gamesolver::use_online_fine_tuning || !gamesolver::use_critical_positions) { return; }
    if (recent_selection_path_.getRoot()->count_ < gamesolver::manager_critical_positions_n) { return; }

    std::string sgf_prefix = gamesolver::manager_job_sgf.substr(0, gamesolver::manager_job_sgf.length() - 1);
    std::string sgf_openings = recent_selection_path_.summarize(sgf_prefix, recent_selection_path_.getRoot());
    if (!sgf_openings.empty()) { job_handler_.outputAsync("solver openings" + sgf_openings); }
    recent_selection_path_.reset();
}

} // namespace gamesolver

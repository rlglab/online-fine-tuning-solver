#include "gs_actor.h"
#include "gs_configuration.h"
#include "random.h"
#include "sgf_loader.h"
#include "time_system.h"
#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace gamesolver {

using namespace minizero;
using namespace minizero::actor;
using namespace minizero::network;
using namespace minizero::utils;

void GSActor::reset()
{
    GumbelZeroActor::reset();

    // load opening
    utils::SGFLoader sgf_loader;
    sgf_loader.loadFromString(sgf_openings_.empty() ? gamesolver::trainer_opening_sgf : sgf_openings_[Random::randInt() % sgf_openings_.size()]);
    opening_length_ = sgf_loader.getActions().size();
    for (auto action_pair : sgf_loader.getActions()) { act(action_pair.first); }
    if (gamesolver::actor_use_random_op) {
        int random_length = utils::Random::randInt() % gamesolver::actor_random_op_max_length + 1;
        opening_length_ = std::max(opening_length_, random_length);
        // GumbelZeroActor::reset will call resetSearch, but at that time the opening length haven't been decided yet.
        is_ro_move_ = isRandomOpeningAction();
    }
}

void GSActor::resetSearch()
{
    GumbelZeroActor::resetSearch();
    is_ro_move_ = isRandomOpeningAction();
}

bool GSActor::isSearchDone() const
{
    bool ro_move_check = (is_ro_move_ && !getMCTS()->getRootNode()->isLeaf());
    return GumbelZeroActor::isSearchDone() || ro_move_check;
}

std::string GSActor::getRecord(std::unordered_map<std::string, std::string> tags /* = {} */) const
{
    tags["OP"] = std::to_string(opening_length_);
    return GumbelZeroActor::getRecord(tags);
}

void GSActor::beforeNNEvaluation()
{
    mcts_search_data_.node_path_ = selection();
    Environment env_transition = getEnvironmentTransition(mcts_search_data_.node_path_);
    nn_evaluation_batch_id_ = pcn_network_->pushBack(env_transition.getFeatures());
}

void GSActor::afterNNEvaluation(const std::shared_ptr<NetworkOutput>& network_output)
{
    const std::vector<minizero::actor::MCTSNode*>& node_path = mcts_search_data_.node_path_;
    minizero::actor::MCTSNode* leaf_node = node_path.back();
    Environment env_transition = getEnvironmentTransition(node_path);
    if (!env_transition.isTerminal()) {
        std::shared_ptr<ProofCostNetworkOutput> pcn_output = std::static_pointer_cast<ProofCostNetworkOutput>(network_output);
        getMCTS()->expand(leaf_node, calculateActionPolicy(env_transition, pcn_output));
        getMCTS()->backup(node_path, pcn_output->value_n_);
    } else {
        float value = (env_transition.getEvalScore() == 1 ? config::nn_discrete_value_size - 1 : 0);
        getMCTS()->backup(node_path, value);
    }
    if (leaf_node == getMCTS()->getRootNode()) { addNoiseToNodeChildren(leaf_node); }
    if (isSearchDone()) { handleSearchDone(); }

    if (config::actor_use_gumbel) { sequentialHalving(); }
}

void GSActor::step()
{
    beforeNNEvaluation();
    afterNNEvaluation(pcn_network_->forward()[getNNEvaluationBatchIndex()]);
}

void GSActor::handleSearchDone()
{
    assert(isSearchDone());
    mcts_search_data_.selected_node_ = decideActionNode();
    const Action action = getSearchAction();
    std::ostringstream oss;
    oss << "model file name: " << config::nn_file_name << std::endl
        << utils::TimeSystem::getTimeString("[Y/m/d H:i:s.f] ")
        << "move number: " << env_.getActionHistory().size()
        << ", action: " << action.toConsoleString()
        << " (" << action.getActionID() << ")"
        << ", player: " << env::playerToChar(action.getPlayer())
        << ", value bound: (" << getMCTS()->getTreeValueMap().begin()->first
        << ", " << getMCTS()->getTreeValueMap().rbegin()->first << ")" << std::endl
        << "  root node info: " << getMCTS()->getRootNode()->toString() << std::endl
        << "action node info: " << mcts_search_data_.selected_node_->toString() << std::endl
        << "opening length: " << opening_length_ << std::endl;
    mcts_search_data_.search_info_ = oss.str();
}

minizero::actor::MCTSNode* GSActor::decideActionNode()
{
    if (is_ro_move_) {
        return getMCTS()->selectChildByRandomOpening(getMCTS()->getRootNode());
    } else if (config::actor_use_gumbel) {
        return GumbelZeroActor::decideActionNode();
    } else {
        return ZeroActor::decideActionNode();
    }
    assert(false);
    return nullptr;
}

bool GSActor::isRandomOpeningAction() const
{
    int current_game_length = env_.getActionHistory().size();
    env::Player solved_player = env::charToPlayer(gamesolver::solved_player);
    bool is_solved_player = (getMCTS()->getRootNode()->getAction().nextPlayer() == solved_player);
    return gamesolver::actor_use_random_op && current_game_length < opening_length_ && !is_solved_player;
}

std::vector<GSMCTS::ActionCandidate> GSActor::calculateActionPolicy(const Environment& env_transition, const std::shared_ptr<ProofCostNetworkOutput>& pcn_output)
{
    std::vector<GSMCTS::ActionCandidate> action_candidates;
    for (size_t action_id = 0; action_id < pcn_output->policy_.size(); ++action_id) {
        Action action(action_id, env_transition.getTurn());
        if (!env_transition.isLegalAction(action)) { continue; }
        action_candidates.push_back(GSMCTS::ActionCandidate(action, pcn_output->policy_[action_id], pcn_output->policy_logits_[action_id]));
    }
    sort(action_candidates.begin(), action_candidates.end(), [](const GSMCTS::ActionCandidate& lhs, const GSMCTS::ActionCandidate& rhs) {
        return lhs.policy_ > rhs.policy_;
    });
    return action_candidates;
}

} // namespace gamesolver

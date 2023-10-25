#include "gs_mcts.h"
#include "gs_configuration.h"
#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <limits>
#include <utility>

namespace gamesolver {

using namespace minizero;

void GSMCTSNode::reset()
{
    MCTSNode::reset();
    is_virtual_solved_ = false;
    ghi_ = false;
    in_loop_ = false;
    rzone_data_index_ = -1;
    ghi_data_index_ = -1;
    tt_start_lookup_id_ = 0;
    match_tt_node_ = nullptr;
    equal_loss_node_ = nullptr;
    solver_status_ = SolverStatus::kSolverUnknown;
}

std::string GSMCTSNode::toString() const
{
    std::ostringstream oss;
    if (getNumChildren() == 0) { oss << "Leaf node, "; }
    oss.precision(4);
    oss << "solver_status: ";
    switch (solver_status_) {
        case SolverStatus::kSolverWin: oss << "WIN, "; break;
        case SolverStatus::kSolverDraw: oss << "DRAW, "; break;
        case SolverStatus::kSolverLoss: oss << "LOSS, "; break;
        case SolverStatus::kSolverUnknown: oss << "UNKNOWN, "; break;
        default: break;
    }
    oss << std::fixed << "p = " << policy_
        << ", p_logit = " << policy_logit_
        << ", p_noise = " << policy_noise_
        << ", v = " << value_
        << ", mean = " << mean_
        << ", count = " << count_
        << ", equal_loss = " << (equal_loss_node_ ? equal_loss_node_->getAction().getActionID() : -1)
        << ", match_tt = " << (match_tt_node_ ? "true" : "false")
        << ", check_ghi = " << (ghi_ ? "true" : "false")
        << ", rzone_data_index = ~" << rzone_data_index_ << "~"
        << ", ghi_data_index = @" << ghi_data_index_ << "@";

    return oss.str();
}

bool ZonePattern::operator==(const ZonePattern& rhs)
{
    return rzone_bitboard_ == rhs.getRZone() && stone_bitboard_ == rhs.getRZoneStonePair();
}

void GHIData::reset()
{
    min_loop_offset_before_root_ = 0;
    patterns_.clear();
}

void GHIData::parseFromString(const std::string& ghi_string)
{
    size_t semicolon_index = ghi_string.find_last_of(";");
    if (semicolon_index == std::string::npos) { return; }

    std::string all_pattern_string = ghi_string.substr(0, semicolon_index);
    std::vector<std::string> patterns;
    boost::split(patterns, all_pattern_string, boost::is_any_of("-"));
    for (const std::string& pattern_string : patterns) {
        std::vector<std::string> bitboards;
        boost::split(bitboards, pattern_string, boost::is_any_of(":"));
        assert(bitboards.size() == 3);
        GSBitboard rzone_bitboard = std::stoull(bitboards[0], nullptr, 16);
        GSBitboard black_bitboard = std::stoull(bitboards[1], nullptr, 16);
        GSBitboard white_bitboard = std::stoull(bitboards[2], nullptr, 16);
        addPattern(ZonePattern(rzone_bitboard, env::GamePair<GSBitboard>(black_bitboard, white_bitboard)));
    }
    min_loop_offset_before_root_ = std::stoi(ghi_string.substr(semicolon_index + 1));
}

void GHIData::addPattern(const ZonePattern& rzone_data)
{
    for (size_t index = 0; index < patterns_.size(); ++index) {
        if (patterns_[index] == rzone_data) { return; }
    }

    patterns_.push_back(rzone_data);
}

std::string GHIData::toString() const
{
    std::ostringstream oss;
    bool is_first_pattern = true;
    for (auto& pattern : patterns_) {
        if (is_first_pattern) {
            is_first_pattern = false;
        } else {
            oss << "-";
        }
        oss << std::hex
            << pattern.getRZone().to_ullong() << ":"
            << pattern.getRZoneStone(env::Player::kPlayer1).to_ullong() << ":"
            << pattern.getRZoneStone(env::Player::kPlayer2).to_ullong();
    }
    if (patterns_.size() != 0) { oss << ";" << std::dec << min_loop_offset_before_root_; }
    return oss.str();
}

void GSMCTS::reset()
{
    MCTS::reset();
    tree_rzone_data_.reset();
    tree_ghi_data_.reset();
    ghi_nodes_map_.clear();
}

void GSMCTS::backup(const std::vector<actor::MCTSNode*>& node_path, const float value, const float reward /* = 0.0f */)
{
    assert(node_path.size() > 0);

    // value from root's perspective
    float root_value = value;
    for (int i = static_cast<int>(node_path.size() - 1); i > 0; --i) {
        if (node_path[i]->getAction().getPlayer() == env::charToPlayer(gamesolver::solved_player)) { continue; }
        root_value += std::log10(config::nn_action_size);
    }
    root_value = fmax(0, fmin(config::nn_discrete_value_size - 1, root_value));

    // update value
    node_path.back()->setValue(value);
    for (int i = static_cast<int>(node_path.size() - 1); i >= 0; --i) {
        actor::MCTSNode* node = node_path[i];
        float original_mean = node->getMean();
        node->add(root_value);
        updateTreeValueMap(original_mean, node->getMean());
    }
}

minizero::actor::MCTSNode* GSMCTS::selectChildByPUCTScore(const minizero::actor::MCTSNode* node, int top_k_selection, bool skip_virtual_solved_nodes) const
{
    assert(node && !node->isLeaf() && top_k_selection > 0);
    minizero::actor::MCTSNode* selected = nullptr;
    int total_simulation = node->getCountWithVirtualLoss();
    float init_q_value = calculateInitQValue(node);
    float best_score = -std::numeric_limits<float>::max();
    std::vector<std::pair<minizero::actor::MCTSNode*, float>> candidates;
    for (int i = 0; i < node->getNumChildren(); ++i) {
        minizero::actor::MCTSNode* child = node->getChild(i);
        if (static_cast<GSMCTSNode*>(child)->isSolved()) { continue; }
        if (skip_virtual_solved_nodes && static_cast<GSMCTSNode*>(child)->isVirtualSolved()) { continue; }
        float score = child->getNormalizedPUCTScore(total_simulation, tree_value_map_, init_q_value);
        candidates.push_back({child, score});
        if (score <= best_score) { continue; }
        best_score = score;
        selected = child;
    }

    if (top_k_selection == 1) { return selected; }
    std::sort(candidates.begin(), candidates.end(), [](const std::pair<minizero::actor::MCTSNode*, float>& l, const std::pair<minizero::actor::MCTSNode*, float>& r) { return l.second > r.second; });
    if (static_cast<int>(candidates.size()) > top_k_selection) { candidates.resize(top_k_selection); }
    return (candidates.empty() ? nullptr : candidates[utils::Random::randInt() % candidates.size()].first);
}

minizero::actor::MCTSNode* GSMCTS::selectChildByRandomOpening(const minizero::actor::MCTSNode* node) const
{
    assert(node && !node->isLeaf());
    if (gamesolver::actor_random_op_use_softmax) {
        minizero::actor::MCTSNode* selected = nullptr;
        float total_sum = 0.0;
        float probability_sum = 0.0;
        float temperature = gamesolver::actor_random_op_softmax_temperature;
        for (int i = 0; i < node->getNumChildren(); i++) {
            GSMCTSNode* child = static_cast<GSMCTSNode*>(node->getChild(i));
            total_sum += std::exp(child->getPolicyLogit() / temperature);
        }
        for (int i = 0; i < node->getNumChildren() && probability_sum <= gamesolver::actor_random_op_softmax_sum_limit; i++) {
            GSMCTSNode* child = static_cast<GSMCTSNode*>(node->getChild(i));
            float policy = std::exp(child->getPolicyLogit() / temperature) / total_sum;
            probability_sum += policy;
            float rand = utils::Random::randReal(probability_sum);
            if (selected == nullptr || rand < policy) { selected = child; }
        }
        assert(selected != nullptr);
        return selected;
    } else {
        int selected_node_index = utils::Random::randInt() % node->getNumChildren();
        return node->getChild(selected_node_index);
    }
}

} // namespace gamesolver

#pragma once

#include "gs_mcts.h"
#include "gumbel_zero_actor.h"
#include "proof_cost_network.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace gamesolver {

class GSActor : public minizero::actor::GumbelZeroActor {
public:
    GSActor(uint64_t tree_node_size)
        : minizero::actor::GumbelZeroActor(tree_node_size)
    {
        pcn_network_ = nullptr;
    }

    void reset() override;
    void resetSearch() override;
    bool isSearchDone() const override;
    std::string getRecord(std::unordered_map<std::string, std::string> tags = {}) const override;
    void beforeNNEvaluation() override;
    void afterNNEvaluation(const std::shared_ptr<minizero::network::NetworkOutput>& network_output) override;
    bool isResign() const override { return false; }
    virtual std::string getActionComment() const override { return (minizero::config::actor_use_gumbel ? GumbelZeroActor::getActionComment() : ZeroActor::getActionComment()); }
    void setNetwork(const std::shared_ptr<minizero::network::Network>& network) override { pcn_network_ = std::static_pointer_cast<ProofCostNetwork>(network); }
    std::shared_ptr<minizero::actor::Search> createSearch() override { return std::make_shared<GSMCTS>(tree_node_size_); }
    std::shared_ptr<GSMCTS> getMCTS() { return std::static_pointer_cast<GSMCTS>(search_); }
    const std::shared_ptr<GSMCTS> getMCTS() const { return std::static_pointer_cast<GSMCTS>(search_); }
    bool isValidOpening() const { return static_cast<int>(env_.getActionHistory().size()) > opening_length_; }

    inline void setSgfOpenings(const std::vector<std::string>& sgf_openings) { sgf_openings_ = sgf_openings; }

protected:
    void step() override;
    void handleSearchDone() override;
    minizero::actor::MCTSNode* decideActionNode() override;
    virtual std::vector<minizero::actor::MCTSNode*> selection() override { return (minizero::config::actor_use_gumbel ? GumbelZeroActor::selection() : ZeroActor::selection()); }

    std::vector<GSMCTS::ActionCandidate> calculateActionPolicy(const Environment& env_transition, const std::shared_ptr<ProofCostNetworkOutput>& pcn_output);
    bool isRandomOpeningAction() const;

    int opening_length_;
    std::vector<std::string> sgf_openings_;
    std::shared_ptr<ProofCostNetwork> pcn_network_;
    bool is_ro_move_;
};

} // namespace gamesolver

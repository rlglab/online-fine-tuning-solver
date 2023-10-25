#pragma once

#include "job_handler.h"
#include "solver.h"
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>

namespace gamesolver {

class Manager : public Solver, protected JobResultDeque {
public:
    Manager(uint64_t tree_node_size, JobHandler& handler = JobHandler::instance())
        : Solver(tree_node_size), job_handler_(handler)
    {
    }

    void reset() override;
    void resetSearch() override;
    void solve() override;
    void beforeNNEvaluation() override;
    void afterNNEvaluation(const std::shared_ptr<minizero::network::NetworkOutput>& network_output) override;
    bool isSearchDone() const override { return (quit_ || Solver::isSearchDone()); }

private:
    class RecentSelectionPath {
    public:
        class TrieNode {
        public:
            int count_;
            const minizero::actor::MCTSNode* node_;
            std::unordered_map<int, TrieNode*> children_;
            TrieNode() { reset(nullptr); }
            void reset(const minizero::actor::MCTSNode* node);
        };

        RecentSelectionPath();
        void reset();
        void addSelectionPath(const std::vector<minizero::actor::MCTSNode*>& node_path);
        std::string summarize(std::string& prefix, TrieNode* node) const;
        inline TrieNode* getRoot() { return &nodes_[0]; }
        inline const TrieNode* getRoot() const { return &nodes_[0]; }

    private:
        inline TrieNode* newNode() { return &nodes_[++node_count_]; }

        int node_count_;
        std::vector<TrieNode> nodes_;
    };

    std::vector<minizero::actor::MCTSNode*> selection() override;
    bool isValidSimulation(const GSMCTSNode* node, const std::vector<minizero::env::GamePair<GSBitboard>>& ancestor_positions) const override;
    void addVirtualSolvedNode(minizero::actor::MCTSNode* child, minizero::actor::MCTSNode* parent);
    void handleSolverJobResults();
    void handleJobCommands();
    std::string getSolverJobSgf(const std::vector<minizero::actor::MCTSNode*>& node_path);
    void updateGHIData(const SolverJob& job_result);
    void broadcastCriticalPositions();

    bool quit_;
    JobHandler& job_handler_;
    RecentSelectionPath recent_selection_path_;
};

} // namespace gamesolver

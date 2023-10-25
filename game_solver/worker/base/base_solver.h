#pragma once

#include "gs_actor.h"
#include "knowledge_handler.h"
#include "rzone_handler.h"
#include "rzone_tt_handler.h"
#include "solver_job.h"
#include <memory>
#include <vector>

namespace gamesolver {

class BaseSolver : public GSActor {
public:
    BaseSolver(uint64_t tree_node_size)
        : GSActor(tree_node_size),
          rzone_handler_(nullptr),
          knowledge_handler_(nullptr)
    {
    }

    void reset() override;
    void resetSearch() override;
    virtual void setSolverJob(const SolverJob& solver_job);
    virtual void solve() { think(); }
    Action think(bool with_play = false, bool display_board = false) override;
    void beforeNNEvaluation() override;
    void afterNNEvaluation(const std::shared_ptr<minizero::network::NetworkOutput>& network_output) override;
    bool isSearchDone() const override { return (getMCTS()->reachMaximumSimulation() || getMCTS()->getRootNode()->isSolved()); }

    inline void setIsIdle(bool is_idle) { is_idle_ = is_idle; }
    inline bool isIdle() const { return is_idle_; }
    inline const SolverJob& getSolverJob() const { return solver_job_; }

protected:
    void handleSearchDone() override;
    std::vector<minizero::actor::MCTSNode*> selection() override;
    void updateSolverStatus(SolverStatus status, std::vector<minizero::actor::MCTSNode*> node_path, const GSBitboard& rzone_bitboard);
    void updateWinnerRZone(const Environment& env, GSMCTSNode* parent, const GSMCTSNode* child);
    void pruneNodesOutsideRZone(const Environment& env, const GSMCTSNode* parent, GSMCTSNode* node);
    bool isAllChildrenSolutionLoss(const GSMCTSNode* node) const;
    void updateLoserRZone(const Environment& env, GSMCTSNode* parent);
    void setNodeRZone(GSMCTSNode* node, const ZonePattern& zone_pattern);
    bool findTTAndUpdateSolverStatus(const Environment& env, const std::vector<minizero::actor::MCTSNode*>& node_path);
    void storeTT(GSMCTSNode* node, const Environment& env, int winner_aciton_id = -1);
    virtual bool isValidSimulation(const GSMCTSNode* node, const std::vector<minizero::env::GamePair<GSBitboard>>& ancestor_positions) const;
    void collectGHIInfo(GSMCTSNode* node, GHIData& ghi_data);
    bool hasRZonePatternInPositions(const ZonePattern& pattern, const std::vector<minizero::env::GamePair<GSBitboard>>& ancestor_positions) const;

    virtual std::shared_ptr<RZoneHandler> createRZoneHandler() = 0;
    virtual std::shared_ptr<KnowledgeHandler> createKnowledgeHandler() = 0;

    bool is_idle_;
    SolverJob solver_job_;
    RZoneTTHandler rzone_tt_handler_;
    std::shared_ptr<RZoneHandler> rzone_handler_;
    std::shared_ptr<KnowledgeHandler> knowledge_handler_;
};

} // namespace gamesolver

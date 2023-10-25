#pragma once

#include "gs_bitboard.h"
#include "mcts.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace gamesolver {

enum class SolverStatus {
    kSolverWin = 1,
    kSolverDraw = 0,
    kSolverLoss = -1,
    kSolverUnknown = -2
};

class GSMCTSNode : public minizero::actor::MCTSNode {
public:
    GSMCTSNode() {}

    void reset() override;
    std::string toString() const override;

    inline bool displayInTreeLog() const override { return solver_status_ != SolverStatus::kSolverUnknown || count_ > 0; }

    // setter
    inline void setVirtualSolved(bool is_virtual_solved) { is_virtual_solved_ = is_virtual_solved; }
    inline void setGHI(bool check_ghi) { ghi_ = check_ghi; }
    inline void setInLoop(bool in_loop) { in_loop_ = in_loop; }
    inline void setRZoneDataIndex(int rzone_data_index) { rzone_data_index_ = rzone_data_index; }
    inline void setGHIIndex(int ghi_index) { ghi_data_index_ = ghi_index; }
    inline void setTTStartLookupID(int tt_start_lookup_id) { tt_start_lookup_id_ = tt_start_lookup_id; }
    inline void setMatchTTNode(GSMCTSNode* match_tt_node) { match_tt_node_ = match_tt_node; }
    inline void setEqualLossNode(GSMCTSNode* equal_loss_node) { equal_loss_node_ = equal_loss_node; }
    inline void setSolverStatus(SolverStatus result) { solver_status_ = result; }
    inline void setFirstChild(GSMCTSNode* first_child) { minizero::actor::TreeNode::setFirstChild(first_child); }

    // getter
    inline bool isGHI() const { return ghi_; }
    inline bool isInLoop() const { return in_loop_; }
    inline int getRZoneDataIndex() const { return rzone_data_index_; }
    inline int getGHIIndex() const { return ghi_data_index_; }
    inline int getTTStartLookupID() const { return tt_start_lookup_id_; }
    inline GSMCTSNode* getMatchTTNode() const { return match_tt_node_; }
    inline GSMCTSNode* getEqualLossNode() const { return equal_loss_node_; }
    inline SolverStatus getSolverStatus() const { return solver_status_; }
    inline virtual GSMCTSNode* getChild(int index) const override { return (index < num_children_ ? static_cast<GSMCTSNode*>(first_child_) + index : nullptr); }

    inline bool isSolved() const { return solver_status_ != SolverStatus::kSolverUnknown; }
    inline bool isVirtualSolved() const { return is_virtual_solved_; }

private:
    bool is_virtual_solved_;
    bool ghi_;
    bool in_loop_;
    int rzone_data_index_;
    int ghi_data_index_;
    int tt_start_lookup_id_;
    GSMCTSNode* match_tt_node_;
    GSMCTSNode* equal_loss_node_; // TODO: replace by match_tt_node_ (?)
    SolverStatus solver_status_;
};

class ZonePattern {
public:
    ZonePattern(const GSBitboard& rzone_bitboard, const minizero::env::GamePair<GSBitboard>& stone_bitboard)
        : rzone_bitboard_(rzone_bitboard), stone_bitboard_(stone_bitboard) {}

    GSBitboard getRZone() const { return rzone_bitboard_; }
    GSBitboard getRZoneStone(minizero::env::Player player) const { return stone_bitboard_.get(player); }
    minizero::env::GamePair<GSBitboard> getRZoneStonePair() const { return stone_bitboard_; }

    bool operator==(const ZonePattern& rhs);

private:
    GSBitboard rzone_bitboard_;
    minizero::env::GamePair<GSBitboard> stone_bitboard_;
};
typedef minizero::actor::TreeData<ZonePattern> TreeRZoneData;

class GHIData {
public:
    GHIData() { reset(); }

public:
    void reset();
    void parseFromString(const std::string& ghi_string);
    void addPattern(const ZonePattern& rzone_data);
    std::string toString() const;

    inline void setMinLoopOffsetBeforeRoot(int min_loop_offset) { min_loop_offset_before_root_ = min_loop_offset; }
    inline int getMinLoopOffsetBeforeRoot() const { return min_loop_offset_before_root_; }
    inline const std::vector<ZonePattern>& getPatterns() const { return patterns_; }
    inline bool empty() const { return patterns_.empty(); }

private:
    int min_loop_offset_before_root_;
    std::vector<ZonePattern> patterns_;
};
typedef minizero::actor::TreeData<GHIData> TreeGHIData;

class GSMCTS : public minizero::actor::MCTS {
public:
    GSMCTS(uint64_t tree_node_size)
        : minizero::actor::MCTS(tree_node_size) {}

    void reset() override;
    void backup(const std::vector<minizero::actor::MCTSNode*>& node_path, const float value, const float reward = 0.0f) override;
    minizero::actor::MCTSNode* selectChildByPUCTScore(const minizero::actor::MCTSNode* node) const override { return selectChildByPUCTScore(node, 1, false); }
    virtual minizero::actor::MCTSNode* selectChildByPUCTScore(const minizero::actor::MCTSNode* node, int top_k_selection, bool skip_virtual_solved_nodes) const;
    minizero::actor::MCTSNode* selectChildByRandomOpening(const minizero::actor::MCTSNode* node) const;

    inline GSMCTSNode* getRootNode() { return static_cast<GSMCTSNode*>(minizero::actor::Tree::getRootNode()); }
    inline const GSMCTSNode* getRootNode() const { return static_cast<const GSMCTSNode*>(minizero::actor::Tree::getRootNode()); }
    inline TreeRZoneData& getTreeRZoneData() { return tree_rzone_data_; }
    inline const TreeRZoneData& getTreeRZoneData() const { return tree_rzone_data_; }
    inline TreeGHIData& getTreeGHIData() { return tree_ghi_data_; }
    inline const TreeGHIData& getTreeGHIData() const { return tree_ghi_data_; }
    inline std::unordered_map<GSMCTSNode*, int>& getGHINodeMap() { return ghi_nodes_map_; }
    inline void addGHINodes(GSMCTSNode* node, int loop_above_offset) { ghi_nodes_map_.insert({node, loop_above_offset}); }

protected:
    minizero::actor::TreeNode* createTreeNodes(uint64_t tree_node_size) override { return new GSMCTSNode[tree_node_size]; }
    minizero::actor::TreeNode* getNodeIndex(int index) override { return getRootNode() + index; }

    TreeRZoneData tree_rzone_data_;
    TreeGHIData tree_ghi_data_;
    std::unordered_map<GSMCTSNode*, int> ghi_nodes_map_;
};

} // namespace gamesolver

#pragma once

#include "gs_mcts.h"
#include "knowledge_handler.h"
#include "open_address_hash_table.h"
#include "rzone_handler.h"
#include "rzone_tt_pattern.h"
#include "stop_timer.h"
#include <deque>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace gamesolver {

class GridHeatMap {
public:
    GridHeatMap() {}

    void clear();
    void reconstructOrder();
    void addRZoneBitboard(GSBitboard rzone_bitboard);
    std::string toString() const { return ""; } // TODO: to be implemented

    inline int getPatternCount() const { return pattern_count_; }
    inline void setOrder(const std::vector<int>& order) { order_ = order; }
    inline const std::vector<int>& getOrder() const { return order_; }

private:
    int heat_count_;
    int total_count_;
    int pattern_count_;
    GSBitboard heat_bitboard_;
    std::vector<int> order_;
    std::vector<int> grid_counts_;

    const float kReconstructionThreshold = 0.8f;
};

class RZoneTTStatistic {
public:
    RZoneTTStatistic() { clear(); }

    void clear();
    std::string toString() const;

public:
    int num_pattern_size_;
    int num_lookup_;
    int num_store_;
    int num_hit_;
    int num_reconstruct_;
    int num_reconstruct_store_;
    uint64_t num_traverse_;
    uint64_t num_compare_;
    StopTimer lookup_timer_;
    StopTimer store_timer_;
    std::map<int, int> num_block_record_;
};

class RZoneTTData {
public:
    RZoneTTData() { clear(); }

    void clear()
    {
        tt_max_id_ = 0;
        patterns_.clear();
    }

    RZoneTTData(const RZoneTTPattern& pattern)
    {
        clear();
        patterns_.push_back(pattern);
    }

    int tt_max_id_;
    std::deque<RZoneTTPattern> patterns_;
};

class RZoneTT : public OpenAddressHashTable<RZoneTTData> {
public:
    RZoneTT(int bit_size = 12)
        : OpenAddressHashTable<RZoneTTData>(bit_size)
    {
    }
    void clear();
    void storeTTPattern(const HashKey& key, const RZoneTTPattern& tt_pattern);
    inline void setStatistic(const RZoneTTStatistic& statistic) { statistic_ = statistic; }
    inline RZoneTTStatistic& getStatistic() { return statistic_; }
    inline const RZoneTTStatistic& getStatistic() const { return statistic_; }
    inline int getTTSize() const { return tt_size_; }

private:
    int tt_size_;
    RZoneTTStatistic statistic_;
};

class RZoneTTHandler {
public:
    RZoneTTHandler(int grid_tt_size = gamesolver::grid_tt_size, int block_tt_size = gamesolver::block_tt_size)
        : grid_tt_(grid_tt_size),
          block_tt_(block_tt_size)
    {
    }

    void clear();
    void storeTT(const Environment& env, RZoneTTPattern tt_pattern, const TreeRZoneData& zone_table);
    bool lookupTT(const Environment& env, GSMCTSNode* node, RZoneTTPattern& tt_pattern, const TreeRZoneData& zone_table);
    std::string getStatisticString() const;

    inline const GridHeatMap& getGridHeatMap() const { return grid_heat_map_; }
    inline void setRZoneHandler(const std::shared_ptr<RZoneHandler>& rzone_handler) { rzone_handler_ = rzone_handler; }
    inline void setKnowledgeHandler(const std::shared_ptr<KnowledgeHandler>& knowledge_handler) { knowledge_handler_ = knowledge_handler; }
    inline const RZoneTT& getGridTT() const { return grid_tt_; }
    inline const RZoneTT& getBlockTT() const { return block_tt_; }

private:
    void storeGridTT(const RZoneTTPattern& tt_pattern);
    bool lookupGridTT(const Environment& env, RZoneTTPattern& tt_pattern, int start_id);
    bool lookupGridTTRecursive(int start, GSHashKey& accumulated_key, const std::vector<int>& heat_map_order, const Environment& env, RZoneTTPattern& tt_pattern) const;
    void reconstructGridTT();
    void storeBlockTT(const Environment& env, const RZoneTTPattern& tt_pattern, const TreeRZoneData& zone_table);
    bool lookupBlockTT(const Environment& env, RZoneTTPattern& tt_pattern, int start_id, const TreeRZoneData& zone_table);
    bool lookupBlockTTRecursive(int start, GSHashKey& accumulated_key, const std::vector<GSHashKey>& block_hashkey_sequence, const Environment& env, RZoneTTPattern& tt_pattern, int start_id, const TreeRZoneData& zone_table);

    GridHeatMap grid_heat_map_;
    RZoneTT grid_tt_;
    RZoneTT block_tt_;
    std::shared_ptr<RZoneHandler> rzone_handler_;
    std::shared_ptr<KnowledgeHandler> knowledge_handler_;
    const int kReconstructionCount = 100;
};

} // namespace gamesolver

#include "rzone_tt_handler.h"
#include "gs_hashkey.h"
#include <algorithm>
#include <limits>
#include <string>
#include <vector>

namespace gamesolver {

using namespace minizero;
using namespace minizero::env;

void GridHeatMap::clear()
{
    heat_count_ = 0;
    total_count_ = 0;
    pattern_count_ = 0;
    heat_bitboard_.reset();
    order_ = {0};
    grid_counts_.clear();
    grid_counts_.resize(kBitboardSize, 0.0);
}

void GridHeatMap::reconstructOrder()
{
    std::vector<int> grid_counts = grid_counts_;
    order_.resize(kBitboardSize);
    std::iota(order_.begin(), order_.end(), 0);
    std::sort(order_.begin(), order_.end(), [&grid_counts](const size_t& l, const size_t& r) { return grid_counts[l] > grid_counts[r]; });

    size_t index = 0;
    float accumulate_count = 0.0f;
    while (index < order_.size() && ((accumulate_count / total_count_) < kReconstructionThreshold)) {
        accumulate_count += grid_counts_[order_[index++]];
    }
    order_.resize(index);
}

void GridHeatMap::addRZoneBitboard(GSBitboard rzone_bitboard)
{
    ++pattern_count_;
    while (!rzone_bitboard.none()) {
        int pos = rzone_bitboard._Find_first();
        rzone_bitboard.reset(pos);

        ++total_count_;
        ++grid_counts_[pos];
        if (heat_bitboard_.test(pos)) { ++heat_count_; }
    }
}

void RZoneTTStatistic::clear()
{
    num_pattern_size_ = 0;
    num_lookup_ = 0;
    num_store_ = 0;
    num_hit_ = 0;
    num_reconstruct_ = 0;
    num_reconstruct_store_ = 0;
    num_traverse_ = 0;
    num_compare_ = 0;
    lookup_timer_.reset();
    store_timer_.reset();
}

std::string RZoneTTStatistic::toString() const
{
    std::ostringstream oss;
    oss << std::fixed
        << num_pattern_size_ << "\t"
        << num_lookup_ << "\t"
        << num_store_ << "\t"
        << num_hit_ << "\t"
        << num_reconstruct_ << "\t"
        << num_reconstruct_store_ << "\t"
        << num_traverse_ << "\t"
        << num_compare_ << "\t"
        << lookup_timer_.getAccumulatedPTime().total_microseconds() / 1000000.f << "\t"
        << store_timer_.getAccumulatedPTime().total_microseconds() / 1000000.f << "\t"
        << std::endl;
    return oss.str();
}

void RZoneTT::clear()
{
    OpenAddressHashTable<RZoneTTData>::clear();
    tt_size_ = 0;
    statistic_.clear();
}

void RZoneTT::storeTTPattern(const HashKey& key, const RZoneTTPattern& tt_pattern)
{
    unsigned int index = lookup(key);
    if (index == std::numeric_limits<unsigned int>::max()) {
        store(key, RZoneTTData(tt_pattern));
    } else {
        entry_[index].getData().patterns_.emplace_front(tt_pattern);
    }
    ++tt_size_;
}

void RZoneTTHandler::clear()
{
    grid_heat_map_.clear();
    grid_tt_.clear();
    block_tt_.clear();
}

void RZoneTTHandler::storeTT(const Environment& env, RZoneTTPattern tt_pattern, const TreeRZoneData& zone_table)
{
    if (gamesolver::use_block_tt) {
        tt_pattern.timestamp_ = block_tt_.getTTSize();
        storeBlockTT(env, tt_pattern, zone_table);
    } else if (gamesolver::use_grid_tt) {
        tt_pattern.timestamp_ = grid_tt_.getTTSize();
        storeGridTT(tt_pattern);
        reconstructGridTT();
    }
}

bool RZoneTTHandler::lookupTT(const Environment& env, GSMCTSNode* node, RZoneTTPattern& tt_pattern, const TreeRZoneData& zone_table)
{
    bool success = false;
    int tt_start_id = node->getTTStartLookupID();
    if (gamesolver::use_block_tt) {
        success = lookupBlockTT(env, tt_pattern, tt_start_id, zone_table);
        if (!success) { node->setTTStartLookupID(block_tt_.getTTSize()); }
    } else if (gamesolver::use_grid_tt) {
        success = lookupGridTT(env, tt_pattern, tt_start_id);
        if (!success) { node->setTTStartLookupID(grid_tt_.getTTSize()); }
    }

    return success;
}

std::string RZoneTTHandler::getStatisticString() const
{
    std::ostringstream oss;
    if (gamesolver::use_block_tt) {
        oss << block_tt_.getStatistic().toString();
    } else if (gamesolver::use_grid_tt) {
        oss << grid_tt_.getStatistic().toString();
        oss << grid_heat_map_.toString() << std::endl;
    }
    return oss.str();
}

void RZoneTTHandler::storeGridTT(const RZoneTTPattern& tt_pattern)
{
    // TODO: fix this (rockmanray)
    // RZoneTTStatistic& statistic = grid_tt_.getStatistic();

    // ++statistic.num_store_;
    // ++statistic.num_pattern_size_;
    // statistic.store_timer_.start();
    // GSHashKey accumulated_hashkey = 0;
    // const std::vector<int>& heat_map_order = grid_heat_map_.getOrder();
    // for (size_t i = 0; i < heat_map_order.size(); ++i) {
    //     const int& pos = heat_map_order[i];
    //     if (!tt_pattern.rzone_bitboard_.test(pos)) { continue; }
    //     if (tt_pattern.empty_bitboard_.test(pos)) {
    //         accumulated_hashkey ^= getPlayerHashKey(pos, env::Player::kPlayerNone);
    //     } else if (tt_pattern.stone_bitboard_.get(env::Player::kPlayer1).test(pos)) {
    //         accumulated_hashkey ^= getPlayerHashKey(pos, env::Player::kPlayer1);
    //     } else if (tt_pattern.stone_bitboard_.get(env::Player::kPlayer2).test(pos)) {
    //         accumulated_hashkey ^= getPlayerHashKey(pos, env::Player::kPlayer2);
    //     }

    //     if (grid_tt_.lookup(accumulated_hashkey) != std::numeric_limits<unsigned int>::max()) { continue; }
    //     grid_tt_.store(accumulated_hashkey, {});
    // }
    // grid_tt_.storeTTPattern(accumulated_hashkey, tt_pattern);
    // grid_heat_map_.addRZoneBitboard(tt_pattern.rzone_bitboard_);
    // statistic.store_timer_.stopAndAddAccumulatedTime();
}

bool RZoneTTHandler::lookupGridTT(const Environment& env, RZoneTTPattern& tt_pattern, int start_id)
{
    // TODO: fix this (rockmanray)
    // RZoneTTStatistic& statistic = grid_tt_.getStatistic();
    // ++statistic.num_lookup_;
    // statistic.lookup_timer_.start();
    // GSHashKey accumulated_key = 0;
    // const std::vector<int>& heat_map_order = grid_heat_map_.getOrder();
    // bool success = lookupGridTTRecursive(0, accumulated_key, heat_map_order, env, tt_pattern);
    // statistic.lookup_timer_.stopAndAddAccumulatedTime();
    // if (success) { ++statistic.num_hit_; }
    // return success;
    return false;
}

bool RZoneTTHandler::lookupGridTTRecursive(int start, GSHashKey& accumulated_key, const std::vector<int>& heat_map_order, const Environment& env, RZoneTTPattern& tt_pattern) const
{
    // TODO: fix this (rockmanray)
    // unsigned int index = grid_tt_.lookup(accumulated_key);
    // if (accumulated_key != 0 && index == std::numeric_limits<unsigned int>::max()) {
    //     return false;
    // } else if (index != std::numeric_limits<unsigned int>::max()) {
    //     for (const auto& pattern : grid_tt_.getEntry(index).getData().patterns_) {
    //         if (!rzone_handler_->isRZonePatternMatch(env, pattern)) { continue; }
    //         tt_pattern = pattern;
    //         return true;
    //     }
    // }

    // for (size_t i = start; i < heat_map_order.size(); ++i) {
    //     GSHashKey key;
    //     const int& pos = heat_map_order[i];
    //     const GoGrid& grid = env.getGrid(pos);
    //     key = getPlayerHashKey(pos, grid.getPlayer());

    //     accumulated_key ^= key;
    //     if (lookupGridTTRecursive(start + 1, accumulated_key, heat_map_order, env, tt_pattern)) { return true; }
    //     accumulated_key ^= key;
    // }
    return false;
}

void RZoneTTHandler::reconstructGridTT()
{
    if (grid_heat_map_.getPatternCount() % kReconstructionCount != 0) { return; }

    RZoneTTStatistic& statistic = grid_tt_.getStatistic();
    ++statistic.num_reconstruct_;
    grid_heat_map_.reconstructOrder();
    std::vector<int> heat_map_order = grid_heat_map_.getOrder();
    std::vector<RZoneTTPattern> all_tt_patterns;
    for (size_t i = 0; i < grid_tt_.getSize(); ++i) {
        for (auto& pattern : grid_tt_.getEntry(i).getData().patterns_) { all_tt_patterns.push_back(pattern); }
    }

    RZoneTTStatistic statistic_backup = statistic;
    grid_tt_.clear();
    grid_tt_.setStatistic(statistic_backup);
    statistic.num_pattern_size_ = 0;
    grid_heat_map_.clear();
    grid_heat_map_.setOrder(heat_map_order);
    for (auto& pattern : all_tt_patterns) { storeGridTT(pattern); }

    statistic.num_reconstruct_store_ += all_tt_patterns.size();
}

void RZoneTTHandler::storeBlockTT(const Environment& env, const RZoneTTPattern& tt_pattern, const TreeRZoneData& zone_table)
{
    RZoneTTStatistic& statistic = block_tt_.getStatistic();
    ++statistic.num_store_;
    ++statistic.num_pattern_size_;
    statistic.store_timer_.start();
    GSHashKey accumulated_hashkey = 0;
    GSBitboard block_bitboard = zone_table.getData(tt_pattern.node_->getRZoneDataIndex()).getRZoneStone(env::charToPlayer(gamesolver::solved_player));
    std::vector<GSHashKey> hashkey_sequence = knowledge_handler_->getHashKeySequenceInBitboard(env, block_bitboard);
    for (size_t i = 0; i < hashkey_sequence.size(); ++i) {
        accumulated_hashkey ^= hashkey_sequence[i];
        if (block_tt_.lookup(accumulated_hashkey) == std::numeric_limits<unsigned int>::max()) {
            block_tt_.store(accumulated_hashkey, {});
        }
        block_tt_.getEntry(block_tt_.lookup(accumulated_hashkey)).getData().tt_max_id_ = block_tt_.getTTSize();
    }
    block_tt_.storeTTPattern(accumulated_hashkey, tt_pattern);
    statistic.store_timer_.stopAndAddAccumulatedTime();
}

bool RZoneTTHandler::lookupBlockTT(const Environment& env, RZoneTTPattern& tt_pattern, int start_id, const TreeRZoneData& zone_table)
{
    RZoneTTStatistic& statistic = block_tt_.getStatistic();
    ++statistic.num_lookup_;
    statistic.lookup_timer_.start();
    GSHashKey accumulated_key = 0;
    std::vector<GSHashKey> hashkey_sequence = knowledge_handler_->getHashKeySequence(env);
    // start from 1 since it is the first block hashkey
    bool success = lookupBlockTTRecursive(1, accumulated_key, hashkey_sequence, env, tt_pattern, start_id, zone_table);
    statistic.lookup_timer_.stopAndAddAccumulatedTime();
    if (success) { ++statistic.num_hit_; }
    return success;
}

bool RZoneTTHandler::lookupBlockTTRecursive(int start, GSHashKey& accumulated_key, const std::vector<GSHashKey>& hashkey_sequence, const Environment& env, RZoneTTPattern& tt_pattern, int start_id, const TreeRZoneData& zone_table)
{
    unsigned int index = block_tt_.lookup(accumulated_key);
    ++block_tt_.getStatistic().num_traverse_;
    if (index == std::numeric_limits<unsigned int>::max()) { return false; }
    if (start_id > block_tt_.getEntry(index).getData().tt_max_id_) { return false; }
    for (const auto& pattern : block_tt_.getEntry(index).getData().patterns_) {
        ++block_tt_.getStatistic().num_compare_;
        if (start_id > pattern.timestamp_) { break; }
        if (!rzone_handler_->isRZonePatternMatch(env, pattern, zone_table)) { continue; }
        tt_pattern = pattern;
        return true;
    }

    for (size_t i = start; i < hashkey_sequence.size(); ++i) {
        accumulated_key ^= hashkey_sequence[i];
        if (lookupBlockTTRecursive(start + 1, accumulated_key, hashkey_sequence, env, tt_pattern, start_id, zone_table)) { return true; }
        accumulated_key ^= hashkey_sequence[i];
    }

    return false;
}

} // namespace gamesolver

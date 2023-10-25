#pragma once

#include "gs_bitboard.h"
#include "gs_mcts.h"
#include "rzone_tt_pattern.h"

namespace gamesolver {

class RZoneHandler {
public:
    RZoneHandler() {}
    virtual ~RZoneHandler() = default;

    virtual GSBitboard getWinnerRZoneBitboard(const Environment& env) = 0;
    virtual GSBitboard getWinnerRZoneBitboard(const Environment& env, const GSBitboard& child_bitboard, const Action& win_action) = 0;
    virtual bool isRelevantMove(const Environment& env, const GSBitboard& rzone_bitboard, const Action& action) = 0;
    virtual GSBitboard getLoserRZoneBitboard(const Environment& env, const GSBitboard& union_bitboard, const minizero::env::Player& player) = 0;
    virtual ZonePattern extractZonePattern(const Environment& env, const GSBitboard& rzone_bitboard) = 0;
    virtual RZoneTTPattern extractRZoneTTPattern(const Environment& env, GSMCTSNode* node, int winner_aciton_id = -1) = 0;
    virtual bool isRZonePatternMatch(const Environment& env, const RZoneTTPattern& rzone_pattern, const TreeRZoneData& zone_table) = 0;
};

} // namespace gamesolver

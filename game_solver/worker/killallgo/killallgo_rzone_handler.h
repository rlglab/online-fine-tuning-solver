#pragma once

#include "killallgo_knowledge_handler.h"
#include "rzone_tt_handler.h"
#include <memory>

namespace gamesolver {

#if KILLALLGO
class KillallGoRZoneHandler : public RZoneHandler {
public:
    KillallGoRZoneHandler(std::shared_ptr<KillallGoKnowledgeHandler> knowledge_handler)
        : knowledge_handler_(knowledge_handler) {}

    GSBitboard getWinnerRZoneBitboard(const minizero::env::killallgo::KillAllGoEnv& env) override;
    GSBitboard getWinnerRZoneBitboard(const minizero::env::killallgo::KillAllGoEnv& env, const GSBitboard& child_bitboard, const Action& win_action) override;
    bool isRelevantMove(const minizero::env::killallgo::KillAllGoEnv& env, const GSBitboard& rzone_bitboard, const Action& action) override;
    GSBitboard getLoserRZoneBitboard(const minizero::env::killallgo::KillAllGoEnv& env, const GSBitboard& union_bitboard, const minizero::env::Player& player) override;
    ZonePattern extractZonePattern(const minizero::env::killallgo::KillAllGoEnv& env, const GSBitboard& rzone_bitboard) override;
    RZoneTTPattern extractRZoneTTPattern(const minizero::env::killallgo::KillAllGoEnv& env, GSMCTSNode* node, int winner_aciton_id = -1) override;
    bool isRZonePatternMatch(const minizero::env::killallgo::KillAllGoEnv& env, const RZoneTTPattern& tt_pattern, const TreeRZoneData& zone_table) override;

private:
    GSBitboard getMoveRZone(const minizero::env::killallgo::KillAllGoEnv& env, const minizero::env::go::GoBitboard& bitboard_rzone, minizero::env::go::GoBitboard bitboard_own_influence);
    GSBitboard getMoveInfluenceInRZ(const minizero::env::killallgo::KillAllGoEnv& env, const minizero::env::go::GoBitboard& rzone_bitboard, const Action& action);
    GSBitboard getLegalizedRZone(const minizero::env::killallgo::KillAllGoEnv& env, minizero::env::go::GoBitboard bitboard, const minizero::env::Player& player);
    GSBitboard getSuicidalRZone(const minizero::env::killallgo::KillAllGoEnv& env, minizero::env::go::GoBitboard bitboard, const minizero::env::Player& player);
    bool matchRZonePatternKoPosition(const minizero::env::killallgo::KillAllGoEnv& env, const int16_t& ko_position);

    std::shared_ptr<KillallGoKnowledgeHandler> knowledge_handler_;
};
#endif

} // namespace gamesolver

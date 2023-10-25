#pragma once

#include "hex_knowledge_handler.h"
#include "rzone_tt_handler.h"
#include <memory>

namespace gamesolver {

#if HEX
class HexRZoneHandler : public RZoneHandler {
public:
    HexRZoneHandler(std::shared_ptr<HexKnowledgeHandler> knowledge_handler)
        : knowledge_handler_(knowledge_handler) {}

    GSBitboard getWinnerRZoneBitboard(const minizero::env::hex::HexEnv& env) override;
    GSBitboard getWinnerRZoneBitboard(const minizero::env::hex::HexEnv& env, const GSBitboard& child_bitboard, const Action& win_action) override;
    bool isRelevantMove(const minizero::env::hex::HexEnv& env, const GSBitboard& rzone_bitboard, const Action& action) override;
    GSBitboard getLoserRZoneBitboard(const minizero::env::hex::HexEnv& env, const GSBitboard& union_bitboard, const minizero::env::Player& player) override;
    ZonePattern extractZonePattern(const minizero::env::hex::HexEnv& env, const GSBitboard& rzone_bitboard) override;
    RZoneTTPattern extractRZoneTTPattern(const minizero::env::hex::HexEnv& env, GSMCTSNode* node, int winner_aciton_id = -1) override;
    bool isRZonePatternMatch(const minizero::env::hex::HexEnv& env, const RZoneTTPattern& rzone_pattern, const TreeRZoneData& zone_table) override;

private:
    std::shared_ptr<HexKnowledgeHandler> knowledge_handler_;
};
#endif

} // namespace gamesolver

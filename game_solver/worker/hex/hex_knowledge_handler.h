#pragma once

#include "gs_mcts.h"
#include "hex.h"
#include "knowledge_handler.h"
#include <memory>
#include <vector>

namespace gamesolver {

#if HEX
class HexKnowledgeHandler : public KnowledgeHandler {
public:
    minizero::env::GamePair<GSBitboard> getStoneBitboard(const minizero::env::hex::HexEnv& env) const;

    minizero::env::Player getWinner(const minizero::env::hex::HexEnv& env) override;
    std::vector<GSHashKey> getHashKeySequence(const minizero::env::hex::HexEnv& env) override;
    std::vector<GSHashKey> getHashKeySequenceInBitboard(const minizero::env::hex::HexEnv& env, GSBitboard bitboard) override;
    void findGHI(const minizero::env::hex::HexEnv& env, std::vector<minizero::actor::MCTSNode*>& node_path, std::shared_ptr<GSMCTS> mcts) override { return; }
    std::vector<minizero::env::GamePair<GSBitboard>> getAncestorPositions(const minizero::env::hex::HexEnv& env, const std::vector<minizero::actor::MCTSNode*>& node_path) override { return {}; }
};
#endif

} // namespace gamesolver

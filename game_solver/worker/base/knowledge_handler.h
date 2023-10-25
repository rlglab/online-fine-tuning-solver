#pragma once

#include "gs_bitboard.h"
#include "gs_hashkey.h"
#include "gs_mcts.h"
#include <memory>
#include <vector>

namespace gamesolver {

class KnowledgeHandler {
public:
    KnowledgeHandler() {}
    virtual ~KnowledgeHandler() = default;

    virtual minizero::env::Player getWinner(const Environment& env) = 0;
    virtual std::vector<GSHashKey> getHashKeySequence(const Environment& env) = 0; // TODO: rename this
    virtual std::vector<GSHashKey> getHashKeySequenceInBitboard(const Environment& env, GSBitboard bitboard) = 0;
    virtual void findGHI(const Environment& env, std::vector<minizero::actor::MCTSNode*>& node_path, std::shared_ptr<GSMCTS> mcts) = 0;
    virtual std::vector<minizero::env::GamePair<GSBitboard>> getAncestorPositions(const Environment& env, const std::vector<minizero::actor::MCTSNode*>& node_path) = 0;
};

} // namespace gamesolver

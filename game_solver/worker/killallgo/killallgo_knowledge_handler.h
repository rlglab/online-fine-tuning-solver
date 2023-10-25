#pragma once

#include "killallgo.h"
#include "knowledge_handler.h"
#include <memory>
#include <string>
#include <vector>

namespace gamesolver {

#if KILLALLGO
class KillallGoKnowledgeHandler : public KnowledgeHandler {
public:
    GSHashKey getSequenceHashKey(const std::string& sgf_string);
    GSHashKey getSequenceHashKey(const minizero::env::killallgo::KillAllGoEnv& env);
    bool isCaptureMove(const minizero::env::killallgo::KillAllGoEnv& env, const Action& action);
    bool isSuicidalMove(const minizero::env::killallgo::KillAllGoEnv& env, const Action& action);
    bool isEatKoMove(const minizero::env::killallgo::KillAllGoEnv& env, const Action& action);
    int getWinByKoPosition(const minizero::env::killallgo::KillAllGoEnv& env, const int& winner_action_id);
    minizero::env::go::GoBitboard getStoneBitBoardAfterPlay(const minizero::env::killallgo::KillAllGoEnv& env, const Action& action);
    minizero::env::go::GoBitboard getLibertyBitBoardAfterPlay(const minizero::env::killallgo::KillAllGoEnv& env, const Action& action);
    int getNumLibertyAfterPlay(const minizero::env::killallgo::KillAllGoEnv& env, const Action& action);
    GSHashKey getHashKeyAfterPlay(const minizero::env::killallgo::KillAllGoEnv& env, const Action& action);
    minizero::env::GamePair<minizero::env::go::GoBitboard> findSekiBitboard(const minizero::env::killallgo::KillAllGoEnv& env, const Action& action);
    bool isEnclosedSeki(const minizero::env::killallgo::KillAllGoEnv& env, const minizero::env::go::GoArea* area);
    bool enclosedSekiSearch(const minizero::env::killallgo::KillAllGoEnv& env, const minizero::env::go::GoBlock* block, const minizero::env::go::GoArea* area, minizero::env::Player turn);

    minizero::env::Player getWinner(const minizero::env::killallgo::KillAllGoEnv& env) override;
    std::vector<GSHashKey> getHashKeySequence(const minizero::env::killallgo::KillAllGoEnv& env) override;
    std::vector<GSHashKey> getHashKeySequenceInBitboard(const minizero::env::killallgo::KillAllGoEnv& env, GSBitboard bitboard) override;
    void findGHI(const minizero::env::killallgo::KillAllGoEnv& env, std::vector<minizero::actor::MCTSNode*>& node_path, std::shared_ptr<GSMCTS> mcts) override;
    std::vector<minizero::env::GamePair<GSBitboard>> getAncestorPositions(const minizero::env::killallgo::KillAllGoEnv& env, const std::vector<minizero::actor::MCTSNode*>& node_path) override;
};
#endif

} // namespace gamesolver

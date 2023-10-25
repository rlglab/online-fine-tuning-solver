#pragma once

#include "gs_mcts.h"

namespace gamesolver {

class RZoneTTPattern {
public:
    RZoneTTPattern() { reset(); }

    void reset()
    {
        ko_position_ = -1;
        timestamp_ = -1;
        node_ = nullptr;
        turn_ = minizero::env::Player::kPlayerNone;
    }

public:
    int16_t ko_position_;
    int timestamp_;
    GSMCTSNode* node_;
    minizero::env::Player turn_; // TODO: to remove
};

} // namespace gamesolver

#pragma once

#include "gs_mcts.h"
#include <memory>
#include <string>

namespace gamesolver {

class TreeLogger {
public:
    static std::string getTreeString(const Environment& env, const std::shared_ptr<GSMCTS>& mcts, const bool is_editor_log = false);
    static std::string recursiveGetTreeString(const Environment& env, const std::shared_ptr<GSMCTS>& mcts, const GSMCTSNode* node, const bool is_editor_log = false);
    static std::string getColorZoneString(const Environment& env, const std::shared_ptr<GSMCTS>& mcts, const GSMCTSNode* node);
    static std::string getCommentString(const Environment& env, const std::shared_ptr<GSMCTS>& mcts, const GSMCTSNode* node, const bool is_editor_log);
    static std::string getRZoneString(const Environment& env, const std::shared_ptr<GSMCTS>& mcts, const GSMCTSNode* node);
    static std::string getWinColorInformation(const Environment& env, const std::shared_ptr<GSMCTS>& mcts, const GSMCTSNode* node, int win_action_id);
    static std::string getRZoneColorInformation(const Environment& env, const std::shared_ptr<GSMCTS>& mcts, const GSMCTSNode* node);
    static std::string getLossActionColorInformation(const Environment& env, const std::shared_ptr<GSMCTS>& mcts, const GSMCTSNode* node);
    static std::string getColorString(const Environment& env, const std::shared_ptr<GSMCTS>& mcts, const GSMCTSNode* node, const int win_action_id);
    static std::string getRZoneColorString(const GSBitboard& bitboard, const int win_action_id, const int board_size);
    static std::string getWinActionColorString(const GSBitboard& bitboard, const int win_action_id, const int board_size);
    static std::string getLossActionColorString(const Environment& env, const std::shared_ptr<GSMCTS>& mcts, const GSMCTSNode* node);
    static std::string actionIDToEditorPosition(const int action_id, const int board_size);
    static int getWinActionID(const Environment& env, const GSMCTSNode* node);
    static std::string getActionString(const int action_id, const int board_size, const bool is_editor_log);
    static std::string nodeToTreeString(const GSMCTSNode* node);
    static void saveTreeStringToFile(const std::string file_name, const Environment& env, const std::shared_ptr<GSMCTS>& mcts, const bool is_editor_log = false);
    static std::string green() { return "0x00ff40"; }
    static std::string purple() { return "0x9721ff"; }
    static std::string red() { return "0xff0000"; }
    static std::string blue() { return "0x0dc8f0"; }
};

} // namespace gamesolver

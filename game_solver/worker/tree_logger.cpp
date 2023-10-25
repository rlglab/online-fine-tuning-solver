#include "tree_logger.h"
#include "sgf_loader.h"
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>

namespace gamesolver {

using namespace minizero;
using namespace minizero::utils;

std::string TreeLogger::getTreeString(const Environment& env, const std::shared_ptr<GSMCTS>& mcts, const bool is_editor_log)
{
    // sgf prefix
    std::ostringstream oss;
    oss << "(;FF[4]CA[UTF-8]SZ[" << gamesolver::env_board_size << "]";
    for (const auto& action : env.getActionHistory()) {
        std::string action_string = getActionString(action.getActionID(), gamesolver::env_board_size, is_editor_log);
        oss << ";" << playerToChar(action.getPlayer())
            << "[" << action_string << "]";
    }

    // tree nodes
    const GSMCTSNode* root = mcts->getRootNode();
    oss << getColorZoneString(env, mcts, root)
        << getCommentString(env, mcts, root, is_editor_log)
        << recursiveGetTreeString(env, mcts, root, is_editor_log) << ")";
    return oss.str();
}

std::string TreeLogger::recursiveGetTreeString(const Environment& env, const std::shared_ptr<GSMCTS>& mcts, const GSMCTSNode* node, const bool is_editor_log)
{
    std::ostringstream oss;
    int num_children = 0;
    for (int i = 0; i < node->getNumChildren(); ++i) {
        GSMCTSNode* child = node->getChild(i);
        if (!child->displayInTreeLog()) { continue; }
        ++num_children;
    }

    for (int i = 0; i < node->getNumChildren(); ++i) {
        GSMCTSNode* child = node->getChild(i);
        if (!child->displayInTreeLog()) { continue; }
        if (num_children > 1) { oss << "("; }
        oss << ";" << playerToChar(child->getAction().getPlayer())
            << "[" << getActionString(child->getAction().getActionID(), gamesolver::env_board_size, is_editor_log) << "]"
            << getColorZoneString(env, mcts, child)
            << getCommentString(env, mcts, child, is_editor_log)
            << recursiveGetTreeString(env, mcts, child, is_editor_log);
        if (num_children > 1) { oss << ")"; }
    }
    return oss.str();
}

std::string TreeLogger::getColorZoneString(const Environment& env, const std::shared_ptr<GSMCTS>& mcts, const GSMCTSNode* node)
{
    std::ostringstream oss;
    int win_action_id = getWinActionID(env, node);
    std::string r_str = getWinColorInformation(env, mcts, node, win_action_id);
    std::string g_str = getRZoneColorInformation(env, mcts, node);
    std::string b_str = getLossActionColorInformation(env, mcts, node);
    if (r_str != "" || g_str != "" || b_str != "") {
        oss << "RZ"
            << "[" << r_str << "]"
            << "[" << g_str << "]"
            << "[" << b_str << "]";
    }
    return oss.str();
}

std::string TreeLogger::getCommentString(const Environment& env, const std::shared_ptr<GSMCTS>& mcts, const GSMCTSNode* node, const bool is_editor_log)
{
    std::ostringstream oss;
    oss << "C[" << nodeToTreeString(node);
    if (!is_editor_log) {
        oss << "]";
    } else {
        int win_action_id = getWinActionID(env, node);
        oss << "]" << getColorString(env, mcts, node, win_action_id);
    }
    return oss.str();
}

std::string TreeLogger::nodeToTreeString(const GSMCTSNode* node)
{
    std::ostringstream oss;
    std::stringstream ss(node->toString());
    std::string token;
    std::string tree_str;
    bool is_first_token = true;
    while (std::getline(ss, token, ',')) {
        std::size_t found = token.find("equal_loss");
        if (found != std::string::npos) {
            std::size_t action_id_pos = token.find("=") + 2;
            int equal_loss = std::stoi(token.substr(action_id_pos));
            if (equal_loss != -1)
                token.replace(token.begin() + action_id_pos, token.end(), getActionString(equal_loss, gamesolver::env_board_size, false));
        }
        tree_str += (is_first_token) ? "" : "\r\n";
        tree_str += token[0] == ' ' ? token.substr(1) : token;
        is_first_token = false;
    }
    return tree_str;
}

std::string TreeLogger::getColorString(const Environment& env, const std::shared_ptr<GSMCTS>& mcts, const GSMCTSNode* node, const int win_action_id)
{
    std::ostringstream oss;
    int board_size = gamesolver::env_board_size;
    int rzone_index = node->getRZoneDataIndex();
    GSBitboard rzone_bitboard;
    if (rzone_index != -1) {
        rzone_bitboard = mcts->getTreeRZoneData().getData(rzone_index).getRZone();
        oss << getRZoneColorString(rzone_bitboard, win_action_id, board_size);
    }
    oss << getWinActionColorString(rzone_bitboard, win_action_id, board_size);
    oss << getLossActionColorString(env, mcts, node);
    return oss.str();
}

std::string TreeLogger::getWinColorInformation(const Environment& env, const std::shared_ptr<GSMCTS>& mcts, const GSMCTSNode* node, int win_action_id)
{
    if (win_action_id == -1) { return ""; }
    std::ostringstream oss;
    oss << std::hex << (1ull << win_action_id);
    return oss.str();
}

std::string TreeLogger::getRZoneColorInformation(const Environment& env, const std::shared_ptr<GSMCTS>& mcts, const GSMCTSNode* node)
{
    std::ostringstream oss;
    int rzone_index = node->getRZoneDataIndex();
    if (rzone_index == -1) { return ""; }
    int board_size = gamesolver::env_board_size;
    GSBitboard rzone_bitboard = mcts->getTreeRZoneData().getData(rzone_index).getRZone();
    uint64_t value = 0;
    for (int i = 0; i < board_size * board_size; ++i) {
        if (rzone_bitboard.test(i)) { value += 1ull << i; }
    }
    oss << std::hex << value;
    return oss.str();
}

std::string TreeLogger::getLossActionColorInformation(const Environment& env, const std::shared_ptr<GSMCTS>& mcts, const GSMCTSNode* node)
{
    std::ostringstream oss;
    bool has_lose_action = false;
    uint64_t value = 0;
    for (int i = 0; i < node->getNumChildren(); i++) {
        GSMCTSNode* child = node->getChild(i);
        if (child->getSolverStatus() == SolverStatus::kSolverLoss) {
            has_lose_action = true;
            int action_id = child->getAction().getActionID();
            value += (1ull << action_id);
        }
    }
    if (!has_lose_action || !(node->getSolverStatus() == SolverStatus::kSolverUnknown)) { return ""; }
    oss << std::hex << value;
    return oss.str();
}

std::string TreeLogger::getRZoneColorString(const GSBitboard& bitboard, const int win_action_id, const int board_size)
{
    std::ostringstream oss;
    oss << "BG[" << green() << "]"
        << "[";
    for (int i = 0; i < board_size * board_size; i++) {
        std::string action_string = actionIDToEditorPosition(i, board_size);
        if (bitboard.test(i) && i != win_action_id) {
            oss << action_string << ",";
        }
    }
    std::string rzone_color_str = oss.str();
    rzone_color_str.pop_back();
    return rzone_color_str + "]";
}

std::string TreeLogger::getWinActionColorString(const GSBitboard& bitboard, const int win_action_id, const int board_size)
{
    if (win_action_id == -1) { return ""; }
    std::ostringstream oss;
    std::string color = (bitboard.test(win_action_id)) ? red() : purple();
    oss << "BG[" << color << "]"
        << "[" << actionIDToEditorPosition(win_action_id, board_size) << "]";
    return oss.str();
}

std::string TreeLogger::getLossActionColorString(const Environment& env, const std::shared_ptr<GSMCTS>& mcts, const GSMCTSNode* node)
{
    std::ostringstream oss;
    int board_size = gamesolver::env_board_size;
    bool has_lose_action = false;
    bool is_black_turn = false;

    oss << "BG[" << blue() << "][";
    for (int i = 0; i < node->getNumChildren(); i++) {
        GSMCTSNode* child = node->getChild(i);
        is_black_turn = child->getAction().getPlayer() == minizero::env::Player::kPlayer1;
        if (child->getSolverStatus() == SolverStatus::kSolverLoss) {
            has_lose_action = true;
            std::string action_string = actionIDToEditorPosition(child->getAction().getActionID(), board_size);
            oss << action_string << ",";
        }
    }
    if (!has_lose_action || !(node->getSolverStatus() == SolverStatus::kSolverUnknown && is_black_turn)) { return ""; }
    std::string lose_color_str = oss.str();
    lose_color_str.pop_back();
    return lose_color_str + "]";
}

std::string TreeLogger::getRZoneString(const Environment& env, const std::shared_ptr<GSMCTS>& mcts, const GSMCTSNode* node)
{
    std::ostringstream oss;
    int rzone_index = node->getRZoneDataIndex();
    if (rzone_index == -1) { return ""; }
    int board_size = gamesolver::env_board_size;
    GSBitboard rzone_bitboard = mcts->getTreeRZoneData().getData(rzone_index).getRZone();
    for (int i = board_size - 1; i >= 0; --i) {
        for (int j = 0; j < board_size; ++j) {
            oss << (rzone_bitboard.test(i * board_size + j) ? "1 " : "0 ");
        }
        oss << "\n";
    }
    return oss.str();
}

std::string TreeLogger::actionIDToEditorPosition(const int action_id, const int board_size)
{
    if (action_id == board_size * board_size) return "@@"; // pass action
    std::ostringstream oss;
    int x = action_id % board_size;
    int y = action_id / board_size;
    // the column number in editor is from bottom to top
    oss << static_cast<char>('A' + x) << static_cast<char>('A' + y);
    return oss.str();
}

int TreeLogger::getWinActionID(const Environment& env, const GSMCTSNode* node)
{
    int win_location = -1;
    for (int i = 0; i < node->getNumChildren(); i++) {
        GSMCTSNode* child = node->getChild(i);
        if (child->getSolverStatus() == SolverStatus::kSolverWin) {
            win_location = child->getAction().getActionID();
            break;
        }
    }
    return win_location;
}

std::string TreeLogger::getActionString(const int action_id, const int board_size, const bool is_editor_log)
{
    return (is_editor_log) ? actionIDToEditorPosition(action_id, board_size) : SGFLoader::actionIDToSGFString(action_id, board_size);
}

void TreeLogger::saveTreeStringToFile(const std::string file_name, const Environment& env, const std::shared_ptr<GSMCTS>& mcts, const bool is_editor_log)
{
    std::fstream f;
    f.open(file_name, std::ios::out);
    f << getTreeString(env, mcts, is_editor_log) << std::endl;
    f.close();
}

} // namespace gamesolver

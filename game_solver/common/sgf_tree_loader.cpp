#include "sgf_tree_loader.h"
#include <algorithm>
#include <cstring>
#include <deque>
#include <iomanip>
#include <memory>
#include <numeric>
#include <regex>
#include <sstream>
#include <string>
#include <utility>
#include <limits>

namespace gamesolver {

using namespace minizero::env;

SGFTreeLoader::SGFTreeLoader()
{
    reset();
}

void SGFTreeLoader::reset()
{
    SGFLoader::reset();
    current_node_id_ = 0;
    current_node_ = nullptr;
    sgf_tree_nodes_.clear();
}

void SGFTreeLoader::setTreeSize(int tree_size)
{
    sgf_tree_nodes_.clear();
    sgf_tree_nodes_.resize(tree_size);
    resetCurrentNode();
}

bool SGFTreeLoader::loadFromString(const std::string& sgf_content)
{
    reset();
    setTreeSize(calculateTreeSizeFromSgfString(sgf_content));
    sgf_content_ = trimSpace(sgf_content);
    SGFTreeNode* root = getNextNode();
    return (parseSGFTree(sgf_content, 0, root) != std::string::npos);
}

size_t SGFTreeLoader::parseSGFTree(const std::string& sgf_content, size_t start_pos, SGFTreeNode* parent)
{
    SGFTreeNode* node = parent;
    size_t index = start_pos + 1;
    while (index < sgf_content_.size() && sgf_content_[index] != ')') {
        if (sgf_content[index] == '(') {
            index = parseSGFTree(sgf_content, index, node) + 1;
        } else {
            std::pair<std::string, std::string> key_value;
            if ((index = calculateKeyValuePair(sgf_content_, index, key_value)) == std::string::npos) { return std::string::npos; }
            if (key_value.first == "B" || key_value.first == "W") {
                int board_size = tags_.count("SZ") ? std::stoi(tags_["SZ"]) : -1;
                if (board_size == -1) { return std::string::npos; }
                Player player = key_value.first == "B" ? Player::kPlayer1 : Player::kPlayer2;
                SGFTreeNode* child = getNextNode();
                if (!node->child_) {
                    node->child_ = child;
                } else {
                    SGFTreeNode* rightmost_child = node->child_;
                    while (rightmost_child->next_silbing_) { rightmost_child = rightmost_child->next_silbing_; }
                    rightmost_child->next_silbing_ = child;
                }
                child->parent_ = node;
                node = child;
                node->action_ = Action(sgfStringToActionID(key_value.second, board_size), player);
            } else if (key_value.first == "C") {
                node->comment_ = key_value.second;
            } else {
                tags_[key_value.first] = key_value.second;
            }
        }
    }
    return index;
}

void SGFTreeLoader::act(const Action& action)
{
    if (!current_node_) { return; }
    SGFTreeNode* child = current_node_->child_;
    while (child) {
        if (child->action_.getActionID() == action.getActionID()) {
            current_node_ = child;
            return;
        }
        child = child->next_silbing_;
    }
    current_node_ = nullptr;
}

std::string SGFTreeLoader::getVisitOrder(bool show_visit_count)
{
    constexpr int MAX_DISPLAY_COUNT = 10;

    std::ostringstream oss;
    const int board_size = std::stoi(tags_.at("SZ"));
    const int board_size_sq = board_size * board_size;
    std::unique_ptr<int[]> board(new int[board_size_sq]{});

    if (current_node_) {
        using NodeInfo = std::pair<int, SGFTreeNode*>;
        std::vector<NodeInfo> visit_counts;
        static std::regex reg("count = ([0-9]+)"); // match the visit count
        // record all silbing nodes
        for (auto child = current_node_->child_; child; child = child->next_silbing_) {
            // parse child_comment
            int action_id = child->action_.getActionID();
            if (action_id < 0 || action_id >= board_size_sq) { continue; }
            std::smatch sm;
            if (!std::regex_search(child->comment_, sm, reg)) { continue; }
            int vc = std::stoi(sm[1].str()); // get visit count
            visit_counts.push_back({vc, child});
        }
        std::sort(visit_counts.begin(), visit_counts.end(), [](const NodeInfo& lhs, const NodeInfo& rhs) {
            return lhs.first > rhs.first;
        });
        struct {
            int visit_count;
            int order;
        } previous = {std::numeric_limits<int>::max(), 0};
        for (auto v : visit_counts) {
            int visit_count = v.first;
            SGFTreeNode* node = v.second;
            if (visit_count < previous.visit_count) {
                previous.visit_count = visit_count;
                previous.order++;
            }
            if (previous.order > MAX_DISPLAY_COUNT || visit_count == 0) { break; }
            board[node->action_.getActionID()] = (show_visit_count ? visit_count : previous.order);
        }
    }

    for (int row = 0; row < board_size; row++) {
        for (int col = 0; col < board_size; col++) {
            const int board_idx = (board_size - row - 1) * board_size + col;
            if (board[board_idx] > 0) {
                oss << board[board_idx] << ' ';
            } else {
                oss << "\"\" ";
            }
        }
        oss << '\n';
    }
    oss << '\n';

    return oss.str();
}

std::string SGFTreeLoader::getNextMaxNode()
{
    std::ostringstream oss;
    const int board_size = std::stoi(tags_.at("SZ"));
    const int board_size_sq = board_size * board_size;
    std::unique_ptr<int[]> board(new int[board_size_sq]{});

    if (current_node_ && current_node_->parent_) {
        static auto get_visit_count = [](const std::string& comment) -> int {
            static std::regex reg("count = ([0-9]+)"); // match the visit count
            std::smatch sm;
            if (!std::regex_search(comment, sm, reg)) { return -1; }
            return std::stoi(sm[1].str());
        };
        int current_visit_count = get_visit_count(current_node_->comment_);
        if (current_visit_count > 0) { // perform the first search
            bool has_find = false;
            for (auto node = current_node_->next_silbing_; node; node = node->next_silbing_) {
                int action_id = node->action_.getActionID();
                if (action_id < 0 || action_id >= board_size_sq) { continue; }
                // parse child_comment
                int visit_count = get_visit_count(node->comment_);
                if (visit_count == current_visit_count) {
                    has_find = true;
                    board[action_id] = visit_count;
                    break;
                }
            }
            if (has_find == false) { // if the first search failed, perform the second search
                struct {
                    int visit_count;
                    SGFTreeNode* node;
                } next_max_node = {std::numeric_limits<int>::min(), nullptr};
                // record all silbing nodes
                for (auto node = current_node_->parent_->child_; node; node = node->next_silbing_) {
                    int action_id = node->action_.getActionID();
                    if (action_id < 0 || action_id >= board_size_sq) { continue; }
                    // parse child_comment
                    int visit_count = get_visit_count(node->comment_);
                    if (visit_count < current_visit_count && visit_count > next_max_node.visit_count) {
                        next_max_node.visit_count = visit_count;
                        next_max_node.node = node;
                    }
                }
                if (next_max_node.node) {
                    board[next_max_node.node->action_.getActionID()] = next_max_node.visit_count;
                }
            }
        }
    }

    for (int row = 0; row < board_size; row++) {
        for (int col = 0; col < board_size; col++) {
            const int board_idx = (board_size - row - 1) * board_size + col;
            if (board[board_idx] > 0) {
                oss << board[board_idx] << ' ';
            } else {
                oss << "\"\" ";
            }
        }
        oss << '\n';
    }
    oss << '\n';

    return oss.str();
}

std::string SGFTreeLoader::getTTMatchIndexPath()
{
    static constexpr int MATCH_FAILED = -2;
    static const auto get_tt_match_index_value = [](SGFTreeNode* node) -> int {
        if (node == nullptr) { return MATCH_FAILED; }

        static const std::string key = "tt_match_index";
        std::size_t found = node->comment_.find(key);
        if (found == std::string::npos) { return MATCH_FAILED; }

        std::size_t end_pos = node->comment_.find("\r\n", found); // TODO: more general to not \r\n?
        std::size_t start_pos = found + key.length() + 3;
        std::string tt_match_index_str = node->comment_.substr(start_pos, end_pos - start_pos);
        return std::stoi(tt_match_index_str);
    };
    const int board_size = std::stoi(tags_.at("SZ"));
    int match_tt_value = get_tt_match_index_value(current_node_);
    if (match_tt_value == -1 || match_tt_value == MATCH_FAILED) { return ""; }

    std::ostringstream oss;
    std::vector<SGFTreeNode*> match_tt;

    for (auto& node : sgf_tree_nodes_) {
        int tt_value = get_tt_match_index_value(&node);
        if (tt_value == match_tt_value) {
            match_tt.push_back(&node);
        }
    }

    static const auto get_path_string = [&](SGFTreeNode* node) -> std::string {
        std::ostringstream oss;
        oss << "(;FF[4]CA[UTF-8]SZ[" << board_size << "]";
        std::vector<SGFTreeNode*> path;
        SGFTreeNode* root = getRoot();
        for (auto ptr = node; ptr != root; ptr = ptr->parent_)
            path.push_back(ptr);
        for (auto ptr = path.rbegin(); ptr != path.rend(); ptr++) {
            Player player = (*ptr)->action_.getPlayer();
            oss << (player == Player::kPlayer1 ? ";B[" : ";W[") << actionIDToSGFString((*ptr)->action_.getActionID(), board_size) << "]";
        }
        oss << ")";
        return oss.str();
    };
    for (auto& node : match_tt) {
        oss << get_path_string(node) << std::endl;
    }
    return oss.str();
}

int SGFTreeLoader::calculateTreeSizeFromSgfString(const std::string& sgf_content)
{
    int tree_size = 0;
    bool is_in_value = false;
    for (size_t index = 0; index < sgf_content.size(); ++index) {
        if (sgf_content[index] == '[') {
            is_in_value = true;
            continue;
        } else if (sgf_content[index] == ']') {
            is_in_value = false;
            continue;
        }

        if (sgf_content[index] == ';' && !is_in_value) { ++tree_size; }
    }

    return tree_size;
}

} // namespace gamesolver

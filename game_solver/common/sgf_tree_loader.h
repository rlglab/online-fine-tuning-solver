#pragma once

#include "environment.h"
#include "sgf_loader.h"
#include <iostream>
#include <string>
#include <vector>

namespace gamesolver {

class SGFTreeLoader : public minizero::utils::SGFLoader {
public:
    class SGFTreeNode {
    public:
        Action action_;
        SGFTreeNode* parent_;
        SGFTreeNode* child_;
        SGFTreeNode* next_silbing_;
        std::string comment_;

        SGFTreeNode()
        {
            action_ = Action();
            parent_ = nullptr;
            child_ = nullptr;
            next_silbing_ = nullptr;
            comment_.clear();
        }
    };

public:
    SGFTreeLoader();
    void setTreeSize(int tree_size);
    bool loadFromString(const std::string& sgf_content) override;
    size_t parseSGFTree(const std::string& sgf_content, size_t start_pos, SGFTreeNode* node);
    void act(const Action& action);

    inline int getTreeSize() const { return current_node_id_; }
    inline const SGFTreeNode* getRoot() const { return &sgf_tree_nodes_[0]; }
    inline SGFTreeNode* getRoot() { return &sgf_tree_nodes_[0]; }
    inline void resetCurrentNode() { current_node_ = &sgf_tree_nodes_[0]; }
    inline SGFTreeNode* getNextNode() { return &sgf_tree_nodes_[current_node_id_++]; }
    inline std::string getComment() { return (current_node_ ? current_node_->comment_ : "null"); }
    std::string getVisitOrder(bool show_visit_count = false);
    std::string getNextMaxNode();
    std::string getTTMatchIndexPath();
    int calculateTreeSizeFromSgfString(const std::string& sgf_content);

private:
    void reset() override;

private:
    int current_node_id_;
    SGFTreeNode* current_node_;
    std::vector<SGFTreeNode> sgf_tree_nodes_;
};

} // namespace gamesolver

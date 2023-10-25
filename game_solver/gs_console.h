#pragma once

#include "console.h"
#include "gs_actor.h"
#include "proof_cost_network.h"
#include "sgf_tree_loader.h"
#include <string>
#include <vector>
#include <memory>

namespace gamesolver {

class Console : public minizero::console::Console {
public:
    Console();
    virtual ~Console() = default;

protected:
    void initialize() override;
    void cmdGoguiAnalyzeCommands(const std::vector<std::string>& args);
    void cmdBoardSize(const std::vector<std::string>& args);
    void cmdBoardEvaluation(const std::vector<std::string>& args);
    void cmdPolicy(const std::vector<std::string>& args);
    void cmdValue(const std::vector<std::string>& args);
    void cmdLoadTreeSgf(const std::vector<std::string>& args);
    void cmdRZone(const std::vector<std::string>& args);
    void cmdVisitCountOrder(const std::vector<std::string>& args);
    void cmdVisitCount(const std::vector<std::string>& args);
    void cmdNextMaxNode(const std::vector<std::string>& args);
    void cmdTTMatchIndexPath(const std::vector<std::string>& args);
    void cmdClearBoard(const std::vector<std::string>& args);
    void cmdPlay(const std::vector<std::string>& args);
    void cmdSgfString(const std::vector<std::string>& args);

    std::string dboard(const std::vector<float>& values);
    std::shared_ptr<ProofCostNetworkOutput> networkForward();

    SGFTreeLoader sgf_tree_loader_;
};

} // namespace gamesolver

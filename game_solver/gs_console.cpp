#include "gs_console.h"
#include "configuration.h"
#include "gs_configuration.h"
#include "sgf_loader.h"
#include <climits>
#include <iostream>
#include <map>
#include <regex>
#include <sstream>

namespace gamesolver {

using namespace minizero;

Console::Console()
{
    RegisterFunction("gogui-analyze_commands", this, &Console::cmdGoguiAnalyzeCommands);
    RegisterFunction("boardsize", this, &Console::cmdBoardSize);
    RegisterFunction("board_evaluation", this, &Console::cmdBoardEvaluation); // B:0~1, W:0~-1
    RegisterFunction("policy", this, &Console::cmdPolicy);
    RegisterFunction("value", this, &Console::cmdValue);
    RegisterFunction("load_tree_sgf", this, &Console::cmdLoadTreeSgf);
    RegisterFunction("rzone", this, &Console::cmdRZone);
    RegisterFunction("visit_count_order", this, &Console::cmdVisitCountOrder);
    RegisterFunction("visit_count", this, &Console::cmdVisitCount);
    RegisterFunction("next_max_node", this, &Console::cmdNextMaxNode);
    RegisterFunction("tt_match_index_path", this, &Console::cmdTTMatchIndexPath);
    RegisterFunction("clear_board", this, &Console::cmdClearBoard);
    RegisterFunction("play", this, &Console::cmdPlay);
    RegisterFunction("sgf_string", this, &Console::cmdSgfString);
}

void Console::initialize()
{
    if (!network_) {
        network_ = std::make_shared<ProofCostNetwork>();
        std::dynamic_pointer_cast<ProofCostNetwork>(network_)->loadModel(config::nn_file_name, 0);
    }
    if (!actor_) {
        uint64_t tree_node_size = static_cast<uint64_t>(config::actor_num_simulation + 1) * network_->getActionSize();
        actor_ = std::make_shared<GSActor>(tree_node_size);
        actor_->setNetwork(network_);
        actor_->reset();
    }
}

void Console::cmdGoguiAnalyzeCommands(const std::vector<std::string>& args)
{
    if (!checkArgument(args, 1, 1)) { return; }
    std::string registered_cmd = "dboard/board_evaluation/board_evaluation\n";
    registered_cmd += "dboard/policy/policy\n";
    registered_cmd += "value/value/value\n";
    registered_cmd += "cboard/rzone/rzone\n";
    registered_cmd += "sboard/visit_count_order/visit_count_order\n";
    registered_cmd += "sboard/visit_count/visit_count\n";
    registered_cmd += "sboard/next_max_node/next_max_node\n";
    registered_cmd += "string/tt_match_index_path/tt_match_index_path\n";
    registered_cmd += "string/sgf_string/sgf_string\n";
    reply(console::ConsoleResponse::kSuccess, registered_cmd);
}

void Console::cmdBoardSize(const std::vector<std::string>& args)
{
    if (!checkArgument(args, 2, 2)) { return; }
    if (stoi(args[1]) != gamesolver::env_board_size) {
        reply(console::ConsoleResponse::kFail, "board size must be " + std::to_string(gamesolver::env_board_size));
    } else {
        reply(console::ConsoleResponse::kSuccess, "");
    }
}

void Console::cmdBoardEvaluation(const std::vector<std::string>& args)
{
    if (!checkArgument(args, 1, 1)) { return; }
    std::shared_ptr<ProofCostNetworkOutput> pcn_output = networkForward();
    std::vector<float> board_evaluation;
    board_evaluation.reserve(pcn_output->board_evaluation_.size());
    for (size_t i = 0; i < pcn_output->board_evaluation_.size(); i++) {
        board_evaluation[i] = pcn_output->board_evaluation_[i] * 2 - 1;
    }
    reply(console::ConsoleResponse::kSuccess, dboard(board_evaluation));
}

void Console::cmdPolicy(const std::vector<std::string>& args)
{
    if (!checkArgument(args, 1, 1)) { return; }
    std::shared_ptr<ProofCostNetworkOutput> pcn_output = networkForward();
    reply(console::ConsoleResponse::kSuccess, dboard(pcn_output->policy_));
}

void Console::cmdValue(const std::vector<std::string>& args)
{
    if (!checkArgument(args, 1, 1)) { return; }
    std::shared_ptr<ProofCostNetworkOutput> pcn_output = networkForward();
    std::ostringstream oss;
    oss << pcn_output->value_n_;
    reply(console::ConsoleResponse::kSuccess, oss.str());
}

void Console::cmdLoadTreeSgf(const std::vector<std::string>& args)
{
    if (!checkArgument(args, 2, 2)) { return; }

    std::string filename = args[1];
    std::string msg;
    if (sgf_tree_loader_.loadFromFile(filename)) {
        msg = ("Tree size: " + std::to_string(sgf_tree_loader_.getTreeSize()));
    } else {
        msg = "Load file failed.";
    }

    for (const auto& action : actor_->getEnvironment().getActionHistory()) { sgf_tree_loader_.act(action); }
    reply(console::ConsoleResponse::kSuccess, msg);
}

void Console::cmdRZone(const std::vector<std::string>& args)
{
    if (!checkArgument(args, 1, 1)) { return; }

    if (sgf_tree_loader_.getTags().find("SZ") == sgf_tree_loader_.getTags().end()) {
        reply(console::ConsoleResponse::kFail, "Sgf file is not ready.");
        return;
    }

    std::string comment = sgf_tree_loader_.getComment();

    std::ostringstream oss;
    int board_size = std::stoi(sgf_tree_loader_.getTags().at("SZ"));

    static std::regex reg("([RGB]): ([0-9a-z]+)");
    auto begin = std::sregex_iterator(comment.begin(), comment.end(), reg);
    auto end = std::sregex_iterator();

    std::map<std::string, std::string> color_zone;
    for (auto it = begin; it != end; it++) {
        std::smatch cm = *it;
        std::cerr << cm.str() << std::endl;
        if (cm.size() != 3) { continue; }
        color_zone[cm[1]] = cm[2];
    }

    static const std::string R_CODE = "0xff0000";
    static const std::string G_CODE = "0x00ff40";
    static const std::string B_CODE = "0x0dc8f0";
    static const std::string EMPTY = "\"\"";

    uint64_t r_code = 0;
    uint64_t g_code = 0;
    uint64_t b_code = 0;
    std::stringstream(color_zone["R"]) >> std::hex >> r_code;
    std::stringstream(color_zone["G"]) >> std::hex >> g_code;
    std::stringstream(color_zone["B"]) >> std::hex >> b_code;

    for (int row = 0; row < board_size; row++) {
        for (int col = 0; col < board_size; col++) {
            const int bit_idx = (board_size - row - 1) * board_size + col;
            if ((r_code >> bit_idx) & 1) {
                oss << R_CODE << ' ';
            } else if ((g_code >> bit_idx) & 1) {
                oss << G_CODE << ' ';
            } else if ((b_code >> bit_idx) & 1) {
                oss << B_CODE << ' ';
            } else {
                oss << EMPTY << ' ';
            }
        }
        oss << std::endl;
    }
    oss << std::endl;

    reply(console::ConsoleResponse::kSuccess, oss.str());
}

void Console::cmdVisitCountOrder(const std::vector<std::string>& args)
{
    if (!checkArgument(args, 1, 1)) { return; }
    if (sgf_tree_loader_.getTags().find("SZ") == sgf_tree_loader_.getTags().end()) {
        reply(console::ConsoleResponse::kFail, "Sgf file is not ready.");
        return;
    }
    std::string msg = sgf_tree_loader_.getVisitOrder(false);
    reply(console::ConsoleResponse::kSuccess, msg);
}

void Console::cmdVisitCount(const std::vector<std::string>& args)
{
    if (!checkArgument(args, 1, 1)) { return; }
    if (sgf_tree_loader_.getTags().find("SZ") == sgf_tree_loader_.getTags().end()) {
        reply(console::ConsoleResponse::kFail, "Sgf file is not ready.");
        return;
    }
    std::string msg = sgf_tree_loader_.getVisitOrder(true);
    reply(console::ConsoleResponse::kSuccess, msg);
}

void Console::cmdNextMaxNode(const std::vector<std::string>& args)
{
    if (!checkArgument(args, 1, 1)) { return; }
    if (sgf_tree_loader_.getTags().find("SZ") == sgf_tree_loader_.getTags().end()) {
        reply(console::ConsoleResponse::kFail, "Sgf file is not ready.");
        return;
    }
    std::string msg = sgf_tree_loader_.getNextMaxNode();
    reply(console::ConsoleResponse::kSuccess, msg);
}

void Console::cmdTTMatchIndexPath(const std::vector<std::string>& args)
{
    if (!checkArgument(args, 1, 1)) { return; }
    if (sgf_tree_loader_.getTags().find("SZ") == sgf_tree_loader_.getTags().end()) {
        reply(console::ConsoleResponse::kFail, "Sgf file is not ready.");
        return;
    }
    std::string msg = sgf_tree_loader_.getTTMatchIndexPath();
    reply(console::ConsoleResponse::kSuccess, msg);
}

void Console::cmdClearBoard(const std::vector<std::string>& args)
{
    sgf_tree_loader_.resetCurrentNode();
    console::Console::cmdClearBoard(args);
}

void Console::cmdPlay(const std::vector<std::string>& args)
{
    std::vector<std::string> act_args;
    for (unsigned int i = 1; i < args.size(); i++) { act_args.push_back(args[i]); }
    sgf_tree_loader_.act(Action(act_args, gamesolver::env_board_size));
    console::Console::cmdPlay(args);
}

void Console::cmdSgfString(const std::vector<std::string>& args)
{
    if (!checkArgument(args, 1, 1)) { return; }

    std::ostringstream oss;
    oss << "(;FF[4]CA[UTF-8]SZ[" << gamesolver::env_board_size << "]";
    std::string msg;
    const std::vector<Action>& history = actor_->getEnvironment().getActionHistory();
    for (size_t index = 0; index < history.size(); ++index) {
        oss << ";" << (history[index].getPlayer() == env::Player::kPlayer1 ? "B" : "W")
            << "[" << SGFTreeLoader::actionIDToSGFString(history[index].getActionID(), gamesolver::env_board_size) << "]";
    }
    oss << ")";
    reply(console::ConsoleResponse::kSuccess, oss.str());
}

std::string Console::dboard(const std::vector<float>& values)
{
    std::ostringstream oss;
    for (int row = gamesolver::env_board_size - 1; row >= 0; row--) {
        int start_index = row * gamesolver::env_board_size;
        for (int col = 0; col < gamesolver::env_board_size; col++) {
            oss << values[start_index + col] << " ";
        }
        oss << "\n";
    }
    return oss.str();
}

std::shared_ptr<ProofCostNetworkOutput> Console::networkForward()
{
    std::shared_ptr<ProofCostNetwork> pcn = std::static_pointer_cast<ProofCostNetwork>(network_);
    pcn->pushBack(actor_->getEnvironment().getFeatures());
    auto network_output = pcn->forward();
    return std::static_pointer_cast<ProofCostNetworkOutput>(network_output[0]);
}

} // namespace gamesolver

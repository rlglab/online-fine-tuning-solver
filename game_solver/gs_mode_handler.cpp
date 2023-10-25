#include "gs_mode_handler.h"
#include "gs_console.h"
#include "manager.h"
#include "solver_group.h"
#include "trainer_server.h"
#include "tree_logger.h"
#include <string>
#include <memory>

namespace gamesolver {

GSModeHandler::GSModeHandler()
{
    RegisterFunction("worker", this, &GSModeHandler::runWorker);
    RegisterFunction("manager", this, &GSModeHandler::runManager);
    RegisterFunction("solver_test", this, &GSModeHandler::runSolverTest);
    RegisterFunction("benchmarker", this, &GSModeHandler::runBenchmarker);
}

void GSModeHandler::runConsole()
{
    gamesolver::Console console;
    std::string command;
    while (getline(std::cin, command)) {
        if (command == "quit") { break; }
        console.executeCommand(command);
    }
}

void GSModeHandler::runSelfPlay()
{
    gamesolver::GSActorGroup ag;
    ag.run();
}

void GSModeHandler::runZeroServer()
{
    gamesolver::TrainerServer server;
    server.run();
}

void GSModeHandler::runWorker()
{
    gamesolver::SolverGroup sg;
    sg.run();
}

void GSModeHandler::runManager()
{
    std::shared_ptr<ProofCostNetwork> network = std::make_shared<ProofCostNetwork>();
    network->loadModel(minizero::config::nn_file_name, 0);
    uint64_t tree_node_size = static_cast<uint64_t>(minizero::config::actor_num_simulation + 1) * network->getActionSize();

    Manager manager(tree_node_size);
    manager.setNetwork(network);

    SolverJob solver_job;
    solver_job.sgf_ = gamesolver::manager_job_sgf;
    manager.setSolverJob(solver_job);

    boost::posix_time::ptime start_solving = minizero::utils::TimeSystem::getLocalTime();
    manager.solve();
    boost::posix_time::ptime end_solving = minizero::utils::TimeSystem::getLocalTime();

    std::stringstream timess;
    timess << "time: " << ((end_solving - start_solving).total_microseconds() / 1000000.f) << std::endl;
    std::cerr << timess.str();

    // save solution tree
    TreeLogger::saveTreeStringToFile(gamesolver::tree_file_name + ".sgf", manager.getEnvironment(), manager.getMCTS());
    TreeLogger::saveTreeStringToFile(gamesolver::tree_file_name + ".7esgf", manager.getEnvironment(), manager.getMCTS(), true);
}

void GSModeHandler::runSolverTest()
{
    // TODO: fix this (rockmanray)
    // std::shared_ptr<ProofCostNetwork> network = std::make_shared<ProofCostNetwork>();
    // network->loadModel(config::nn_file_name, 0);
    // long long tree_node_size = static_cast<long long>(config::actor_num_simulation + 1) * network->getActionSize();

    // solver::KillAllGoSolver solver(tree_node_size);
    // solver.setNetwork(network);
    // std::string sgf_string;
    // while (getline(cin, sgf_string)) {
    //     cout << sgf_string << endl;
    //     minizero::utils::SGFLoader sgf_loader;
    //     sgf_loader.loadFromString(sgf_string);
    //     if (sgf_loader.getTags().count("SZ") == 0) { return; }

    //     solver.reset();
    //     const std::vector<std::pair<std::vector<std::string>, std::string>>& actions = sgf_loader.getActions();
    //     for (auto act : actions) {
    //         Action action(act.first, stoi(sgf_loader.getTags().at("SZ")));
    //         solver.act(action);
    //     }

    //     // start solver
    //     const Environment& env = solver.getEnvironment();
    //     cout << env.toString() << endl;
    //     solver.solve();
    //     cout << solver.getMCTS()->getRootNode()->toString() << endl;

    //     // save solution tree
    //     TreeLogger::saveTreeStringToFile("tree_editor.sgf", solver.getEnvironment(), solver.getMCTS(), true);
    //     TreeLogger::saveTreeStringToFile("tree_gogui.sgf", solver.getEnvironment(), solver.getMCTS());
    // }
}

void GSModeHandler::runBenchmarker()
{
    // TODO: fix this (rockmanray)
    // std::shared_ptr<solver::ProofCostNetwork> network = std::make_shared<solver::ProofCostNetwork>();
    // network->loadModel(config::nn_file_name, 0);
    // long long tree_node_size = static_cast<long long>(config::actor_num_simulation + 1) * network->getActionSize();
    // solver::KillAllGoSolver solver(tree_node_size);
    // solver.setNetwork(network);
    // std::string dir = "problems/benchmark";
    // std::string filename = "capture_irrelevant_block_2.sgf";
    // minizero::utils::SGFLoader sgf_loader;
    // sgf_loader.loadFromFile(dir + "/" + filename);
    // const std::vector<std::pair<std::vector<std::string>, std::string>>& actions = sgf_loader.getActions();
    // const Environment& env = solver.getEnvironment();
    // if (sgf_loader.getTags().count("SZ") == 0) { return; }
    // for (auto act : actions) {
    //     Action action(act.first, stoi(sgf_loader.getTags().at("SZ")));
    //     solver.act(action);
    // }

    // cout << env.toString() << endl;
    // env::go::GoBitboard child_bitboard;
    // std::vector<int> child_rz_pos{1, 2, 3, 4, 8, 9, 10, 11, 16, 17, 18, 19, 24, 25, 26, 27, 32, 33};
    // for (unsigned int i = 0; i < child_rz_pos.size(); ++i) { child_bitboard.set(child_rz_pos[i]); }
    // cout << "Child RZone:" << endl;
    // for (int i = 6; i >= 0; --i) {
    //     for (int j = 0; j < 7; ++j) {
    //         if (child_bitboard.test(i * 7 + j)) {
    //             cout << "1 ";
    //         } else {
    //             cout << "0 ";
    //         }
    //     }
    //     cout << "\n";
    // }
    // minizero::env::killallgo::KillAllGoAction win_action(37, minizero::env::Player::kPlayer2);
    // env::go::GoBitboard parent_bitboard = solver::knowledge::KillAllGoRZone::getLoserRZoneBitboard(env, child_bitboard, minizero::env::Player::kPlayer1);
    // cout << "Updated RZone:" << endl;
    // for (int i = 6; i >= 0; --i) {
    //     for (int j = 0; j < 7; ++j) {
    //         if (parent_bitboard.test(i * 7 + j)) {
    //             cout << "1 ";
    //         } else {
    //             cout << "0 ";
    //         }
    //     }
    //     cout << "\n";
    // }
}

} // namespace gamesolver

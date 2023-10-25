#include "trainer_server.h"
#include "gs_configuration.h"
#include "utils.h"
#include <string>
#include <unordered_set>
#include <vector>

namespace gamesolver {

void TrainerServer::initialize()
{
    ZeroServer::initialize();

    // tell broker I am a trainer
    std::cout << gamesolver::broker_name << " << solver register trainer" << std::endl;

    // create one thread to handle I/O
    thread_groups_.create_thread(boost::bind(&TrainerServer::handleIO, this));
}

void TrainerServer::handleIO()
{
    std::string command;
    const int buffer_size = 10000000;
    command.reserve(buffer_size);
    while (getline(std::cin, command)) {
        if (command.empty() || command[0] == '%' || command[0] == '#') continue;

        // format: sender >> command
        command = command.substr(command.find(">> ") + std::string(">> ").size());
        std::string command_prefix = ((command.find(" ") == std::string::npos) ? command : command.substr(0, command.find(" ")));
        if (command_prefix != "solver" && command_prefix != "trainer") { continue; }
        syncTrainerCommand(command.substr(command.find(" ") + 1));
    }
}

void TrainerServer::syncTrainerCommand(const std::string& command)
{
    std::string command_prefix = ((command.find(" ") == std::string::npos) ? command : command.substr(0, command.find(" ")));
    if (command_prefix == "solved_sgf") {
        std::vector<std::string> sgfs = minizero::utils::stringToVector(command.substr(command.find(" ") + 1));
        static std::vector<std::string> d;
        for (auto& s : sgfs) { d.push_back(s); }
        if (d.size() >= 1000) {
            std::fstream fout;
            fout.open(minizero::config::zero_training_directory + "/solved_sgf.txt", std::ios::out);
            for (auto& s : d) { fout << s << std::endl; }
            d.clear();
            fout.close();
        }
    }

    std::unordered_set<std::string> command_list({"openings", "stop", "load_model"});
    if (!command_list.count(command_prefix)) { return; }

    boost::lock_guard<boost::mutex> lock(worker_mutex_);
    for (auto& worker : connections_) {
        if (worker->getType() == "sp") { worker->write(command); }
    }
}

} // namespace gamesolver

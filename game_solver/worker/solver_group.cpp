#include "solver_group.h"
#include "utils.h"
#include <memory>
#include <string>
#include <torch/cuda.h>

namespace gamesolver {

using namespace minizero;
using namespace minizero::network;
using namespace minizero::utils;

bool SolverSlaveThread::doCPUJob()
{
    size_t worker_id = getSharedData()->getAvailableActorIndex();
    if (worker_id >= getSharedData()->actors_.size()) { return false; }

    std::shared_ptr<Solver> solver = std::static_pointer_cast<Solver>(getSharedData()->actors_[worker_id]);
    if (solver->isIdle()) { return true; }
    int network_id = worker_id % getSharedData()->networks_.size();
    int network_output_id = solver->getNNEvaluationBatchIndex();
    if (network_output_id >= 0) {
        assert(network_output_id < static_cast<int>(getSharedData()->network_outputs_[network_id].size()));
        solver->afterNNEvaluation(getSharedData()->network_outputs_[network_id][network_output_id]);
    }

    if (!solver->isSearchDone()) { solver->beforeNNEvaluation(); }
    return true;
}

void SolverGroup::initialize()
{
    GSActorGroup::initialize();
    std::cout << "num_solvers " << getSharedData()->actors_.size() << "\n\n";
    running_ = true;
    quit_ = false;
}

void SolverGroup::createActors()
{
    assert(getSharedData()->networks_.size() > 0);
    std::shared_ptr<Network>& network = getSharedData()->networks_[0];
    uint64_t tree_node_size = static_cast<uint64_t>(config::actor_num_simulation + 1) * network->getActionSize();
    for (int i = 0; i < config::actor_num_parallel_games; ++i) {
        getSharedData()->actors_.emplace_back(std::make_shared<Solver>(tree_node_size));
        getSharedData()->actors_.back()->setNetwork(getSharedData()->networks_[i % getSharedData()->networks_.size()]);
        getSharedData()->actors_.back()->reset();
    }
}

void SolverGroup::handleFinishedGame()
{
    std::lock_guard<std::mutex> lock(getSharedData()->mutex_);
    size_t num_idle_workers = 0;
    for (auto actor : getSharedData()->actors_) {
        std::shared_ptr<Solver> solver = std::static_pointer_cast<Solver>(actor);
        if (solver->isIdle() && !job_queue_.empty()) {
            solver->setSolverJob(job_queue_.front());
            job_queue_.pop_front();
        } else if (!solver->isIdle() && solver->isSearchDone()) {
            std::cout << solver->getSolverJob().getJobResultString() << "\n\n"
                      << std::flush;
            solver->reset();
        }

        // count number of idle solvers
        if (solver->isIdle()) { ++num_idle_workers; }
    }

    // If there are no running solvers, sleep 100 ms
    if (num_idle_workers == getSharedData()->actors_.size() && job_queue_.empty()) {
        boost::this_thread::sleep(boost::posix_time::milliseconds(100));
        if (quit_) { exit(0); }
    }
}

void SolverGroup::handleCommand(const std::string& command_prefix, const std::string& command)
{
    if (command_prefix == "+") {
        SolverJob solver_job;
        if (!solver_job.setJob(command.substr(command.find(' ') + 1))) { std::cerr << "set job failed, job string: \"" << command << "\"" << std::endl; }
        job_queue_.push_back(solver_job);
    } else if (command_prefix == "-") {
        uint64_t job_id = std::stoull(command.substr(command.find(' ') + 1));
        for (auto& actor : getSharedData()->actors_) {
            std::shared_ptr<Solver> solver = std::static_pointer_cast<Solver>(actor);
            if (solver->getSolverJob().job_id_ != job_id) { continue; }
            solver->reset();
            break;
        }
    } else if (command_prefix == "quit") {
        quit_ = true;
    } else {
        GSActorGroup::handleCommand(command_prefix, command);
    }
}

} // namespace gamesolver

#include "gs_actor_group.h"
#include "gs_configuration.h"
#include "utils.h"
#include <algorithm>
#include <memory>
#include <string>
#include <torch/cuda.h>
#include <vector>

namespace gamesolver {

using namespace minizero;
using namespace minizero::network;
using namespace minizero::utils;

void GSSlaveThread::doGPUJob()
{
    if (id_ >= static_cast<int>(getSharedData()->networks_.size())) { return; }

    std::shared_ptr<ProofCostNetwork> pcn = std::static_pointer_cast<ProofCostNetwork>(getSharedData()->networks_[id_]);
    if (pcn->getBatchSize() > 0) { getSharedData()->network_outputs_[id_] = pcn->forward(); }
}

void GSActorGroup::createNeuralNetworks()
{
    int num_networks = std::min(static_cast<int>(torch::cuda::device_count()), config::actor_num_parallel_games);
    assert(num_networks > 0);
    getSharedData()->networks_.resize(num_networks);
    getSharedData()->network_outputs_.resize(num_networks);
    for (int gpu_id = 0; gpu_id < num_networks; ++gpu_id) {
        getSharedData()->networks_[gpu_id] = std::make_shared<ProofCostNetwork>();
        std::dynamic_pointer_cast<ProofCostNetwork>(getSharedData()->networks_[gpu_id])->loadModel(config::nn_file_name, gpu_id);
    }
}

void GSActorGroup::createActors()
{
    assert(getSharedData()->networks_.size() > 0);
    std::shared_ptr<Network>& network = getSharedData()->networks_[0];
    uint64_t tree_node_size = static_cast<uint64_t>(config::actor_num_simulation + 1) * network->getActionSize();
    for (int i = 0; i < config::actor_num_parallel_games; ++i) {
        getSharedData()->actors_.emplace_back(std::make_shared<GSActor>(tree_node_size));
        getSharedData()->actors_.back()->setNetwork(getSharedData()->networks_[i % getSharedData()->networks_.size()]);
        getSharedData()->actors_.back()->reset();
    }
}

void GSActorGroup::handleFinishedGame()
{
    for (size_t actor_id = 0; actor_id < getSharedData()->actors_.size(); ++actor_id) {
        std::shared_ptr<GSActor> actor = std::static_pointer_cast<GSActor>(getSharedData()->actors_[actor_id]);
        if (!actor->isSearchDone()) { continue; }
        bool is_endgame = (actor->isResign() || actor->isEnvTerminal());
        if (is_endgame && !actor->isValidOpening()) { actor->reset(); }
    }
    ActorGroup::handleFinishedGame();
}

void GSActorGroup::handleCommand(const std::string& command_prefix, const std::string& command)
{
    if (command_prefix == "openings") {
        std::vector<std::string> sgf_openings = utils::stringToVector(command.substr(command.find(" ") + 1));
        for (size_t actor_id = 0; actor_id < getSharedData()->actors_.size(); ++actor_id) {
            std::shared_ptr<GSActor> actor = std::static_pointer_cast<GSActor>(getSharedData()->actors_[actor_id]);
            actor->setSgfOpenings(sgf_openings);
        }
    } else {
        ActorGroup::handleCommand(command_prefix, command);
    }
}

} // namespace gamesolver

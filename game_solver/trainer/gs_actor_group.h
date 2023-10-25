#pragma once

#include "actor_group.h"
#include "gs_actor.h"
#include "proof_cost_network.h"
#include <memory>
#include <string>

namespace gamesolver {

class GSSlaveThread : public minizero::actor::SlaveThread {
public:
    GSSlaveThread(int id, std::shared_ptr<minizero::utils::BaseSharedData> shared_data)
        : minizero::actor::SlaveThread(id, shared_data) {}

protected:
    void doGPUJob() override;
};

class GSActorGroup : public minizero::actor::ActorGroup {
public:
    GSActorGroup() {}

protected:
    void createNeuralNetworks() override;
    void createActors() override;
    void handleFinishedGame() override;
    void handleCommand(const std::string& command_prefix, const std::string& command) override;

    std::shared_ptr<minizero::utils::BaseSlaveThread> newSlaveThread(int id) override { return std::make_shared<GSSlaveThread>(id, shared_data_); }
};

} // namespace gamesolver

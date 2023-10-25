#pragma once

#include "gs_actor_group.h"
#include "solver.h"
#include <deque>
#include <memory>
#include <string>

namespace gamesolver {

class SolverSlaveThread : public GSSlaveThread {
public:
    SolverSlaveThread(int id, std::shared_ptr<minizero::utils::BaseSharedData> shared_data)
        : GSSlaveThread(id, shared_data) {}

private:
    bool doCPUJob() override;
};

class SolverGroup : public GSActorGroup {
public:
    SolverGroup() {}

    void initialize() override;

private:
    void createActors() override;
    void handleFinishedGame() override;
    void handleCommand(const std::string& command_prefix, const std::string& command) override;

    std::shared_ptr<minizero::utils::BaseSlaveThread> newSlaveThread(int id) override { return std::make_shared<SolverSlaveThread>(id, shared_data_); }

    bool quit_;
    std::deque<SolverJob> job_queue_;
};

} // namespace gamesolver

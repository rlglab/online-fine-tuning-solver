#pragma once

#include "zero_server.h"
#include <boost/thread.hpp>
#include <string>

namespace gamesolver {

class TrainerServer : public minizero::zero::ZeroServer {
public:
    TrainerServer() {}

private:
    void initialize() override;
    void handleIO();
    void syncTrainerCommand(const std::string& command);

    boost::thread_group thread_groups_;
};

} // namespace gamesolver

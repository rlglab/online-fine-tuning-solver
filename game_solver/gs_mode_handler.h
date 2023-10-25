#pragma once

#include "gs_configuration.h"
#include "mode_handler.h"

namespace gamesolver {

class GSModeHandler : public minizero::console::ModeHandler {
public:
    GSModeHandler();

protected:
    void setDefaultConfiguration(minizero::config::ConfigureLoader& cl) override { gamesolver::setConfiguration(cl); }
    void runConsole() override;
    void runSelfPlay() override;
    void runZeroServer() override;
    void runWorker();
    void runManager();
    void runSolverTest();
    void runBenchmarker();
};

} // namespace gamesolver

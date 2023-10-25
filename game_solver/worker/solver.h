#pragma once

#if KILLALLGO
#include "killallgo_solver.h"
typedef gamesolver::KillallGoSolver Solver;
#elif HEX
#include "hex_solver.h"
typedef gamesolver::HexSolver Solver;
#endif

#pragma once

#include "gs_mcts.h"
#include <string>
#include <vector>

namespace gamesolver {

class SolverJob {
public:
    SolverJob() { reset(); }
    SolverJob(const std::string& sgf, float pcn_value, const std::vector<minizero::actor::MCTSNode*>& node_path);

    void reset();
    bool setJob(const std::string& job_string);
    bool setJobResult(const std::string& job_result_string);
    std::string getJobString(bool with_job_id = true) const;
    std::string getJobResultString(bool with_job_id = true) const;

public:
    uint64_t job_id_;

    // job
    std::string sgf_;
    std::vector<minizero::actor::MCTSNode*> node_path_;
    float pcn_value_;

    // job results
    SolverStatus solver_status_;
    GSBitboard rzone_bitboard_;
    int nodes_;
    GHIData ghi_data_;
};

} // namespace gamesolver

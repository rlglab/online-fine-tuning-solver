#include "solver_job.h"
#include "utils.h"
#include <string>
#include <vector>

namespace gamesolver {

using namespace minizero;
using namespace minizero::actor;
using namespace minizero::utils;

SolverJob::SolverJob(const std::string& sgf, float pcn_value, const std::vector<MCTSNode*>& node_path)
{
    reset();
    sgf_ = sgf;
    pcn_value_ = pcn_value;
    node_path_ = node_path;
}

void SolverJob::reset()
{
    job_id_ = -1ULL;
    sgf_ = "(;FF[4]CA[UTF-8]SZ[" + std::to_string(gamesolver::env_board_size) + "]KM[0])";
    pcn_value_ = 0.0f;
    node_path_.clear();
    solver_status_ = SolverStatus::kSolverUnknown;
    rzone_bitboard_.reset();
    ghi_data_.reset();
    nodes_ = 0;
}

bool SolverJob::setJob(const std::string& job_string)
{
    // job format: job_id sgf [other arguments]
    std::vector<std::string> args = stringToVector(job_string);
    if (args.size() < 2) { return false; }

    uint64_t job_id = std::stoull(args[0]);
    const std::string& sgf = args[1];
    utils::SGFLoader sgf_loader;
    if (job_id == -1ULL || !sgf_loader.loadFromString(sgf)) { return false; }
    if (sgf_loader.getTags().count("SZ") == 0) { return false; }

    // set job
    reset();
    job_id_ = job_id;
    sgf_ = sgf;
    if (args.size() >= 3) { pcn_value_ = std::stof(args[2]); }
    return true;
}

bool SolverJob::setJobResult(const std::string& job_result_string)
{
    // job result format: solver_results rzone_bitboard nodes ghi_string
    std::vector<std::string> args = utils::stringToVector(job_result_string);
    if (args.size() < 2) { return false; }

    solver_status_ = static_cast<SolverStatus>(std::stoi(args[0]));
    try {
        rzone_bitboard_ = std::stoull(args[1], nullptr, 16);
    } catch (std::exception&) {
        return false;
    }
    nodes_ = std::stoi(args[2]);
    ghi_data_.parseFromString(args[3].substr(1, args[3].length() - 2));

    return true;
}

std::string SolverJob::getJobString(bool with_job_id /*= true*/) const
{
    // job format: job_id sgf [other arguments]
    std::ostringstream oss;
    if (with_job_id) { oss << job_id_ << " "; }
    oss << sgf_ << " " << pcn_value_;
    return oss.str();
}

std::string SolverJob::getJobResultString(bool with_job_id /*= true*/) const
{
    // job result format: job_id solver_results rzone_bitboard nodes ghi_string
    std::ostringstream oss;
    if (with_job_id) { oss << job_id_ << " "; }
    oss << static_cast<int>(solver_status_) << " "
        << std::hex << rzone_bitboard_.to_ullong() << " " // TODO: avoid using more than 64-bit bitboard
        << std::dec << nodes_ << " "
        << "\"" << ghi_data_.toString() << "\"";
    return oss.str();
}

} // namespace gamesolver

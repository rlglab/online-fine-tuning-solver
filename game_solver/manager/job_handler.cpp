#include "job_handler.h"
#include "gs_configuration.h"
#include <boost/format.hpp>

namespace gamesolver {

JobHandler& JobHandler::instance()
{
    static JobHandler adapter;
    return adapter;
}

void JobHandler::initialize()
{
    if (gamesolver::use_broker) {
        setBrokerName(gamesolver::broker_name);
        setAdapterName(gamesolver::broker_adapter_name);
        connect(gamesolver::broker_host, gamesolver::broker_port);
    }
    num_solvers_ = 0;
    num_loading_ = 0;
}

bool JobHandler::addJob(JobResultDeque* owner, minizero::actor::MCTSNode* node, SolverJob solver_job)
{
    assert(owner && node);
    std::string command = "solve \"" + solver_job.getJobString(false) + '"';
    std::shared_ptr<Job> job = requestJob(command, JobState::JobConfirmed);
    if (job->id() != NullJobID) {
        std::shared_ptr<JobPackage> job_package = std::make_shared<JobPackage>();
        solver_job.job_id_ = job->id();
        job_package->job_ = job;
        job_package->node_ = node;
        job_package->solver_job_ = solver_job;
        job_package->owner_ = owner;
        {
            std::scoped_lock lock(job_map_mutex_);
            id_job_map_.insert({job->id(), job_package});
            node_job_map_.insert({node, job_package});
        }
        return true;
    } else { // TODO: job->id() == NullJobID means broker rejected this job
        assert(false);
        return false;
    }
}

bool JobHandler::removeJob(JobResultDeque* owner, minizero::actor::MCTSNode* node)
{
    std::shared_ptr<JobPackage> remove;
    {
        std::scoped_lock lock(job_map_mutex_);
        auto job_it = node_job_map_.find(node);
        if (job_it != node_job_map_.end()) {
            remove = job_it->second;
            assert(remove->owner_ == owner);
            id_job_map_.erase(remove->job_->id());
            node_job_map_.erase(job_it);
        }
    }
    if (remove) {
        terminateJob(remove->job_, remove->job_->state());
        return true;
    } else {
        return false;
    }
}

void JobHandler::removeJobs(JobResultDeque* owner)
{
    std::vector<std::shared_ptr<JobPackage>> remove_owned;
    {
        std::scoped_lock lock(job_map_mutex_);
        for (const auto& package : id_job_map_) {
            std::shared_ptr<JobPackage> remove = package.second;
            if (remove->owner_ == owner) {
                remove_owned.push_back(remove);
            }
        }
        for (std::shared_ptr<JobPackage> remove : remove_owned) {
            id_job_map_.erase(remove->job_->id());
            node_job_map_.erase(remove->node_);
        }
    }
    for (std::shared_ptr<JobPackage> remove : remove_owned) {
        terminateJob(remove->job_, remove->job_->state());
    }
}

bool JobHandler::onJobCompleted(std::shared_ptr<BrokerAdapter::Job> job)
{
    int id = job->id();
    auto state = job->state();
    int code = job->code();
    std::string output = job->output();
    if (state == JobState::JobCompleted) {
        if (code != 0) {
            log((boost::format("incorrect return %d for job %d") % code % id).str());
            return false;
        }
        std::shared_ptr<JobPackage> job_package;
        {
            std::scoped_lock lock(job_map_mutex_);
            auto id_it = id_job_map_.find(id);
            if (id_it != id_job_map_.end()) {
                job_package = id_it->second;
                id_job_map_.erase(id_it);
                node_job_map_.erase(job_package->node_);
            } else {
                log((boost::format("unexpected job %d") % id).str());
                return true; // do NOT ask broker to redo this job
            }
        }
        SolverJob& solver_job = job_package->solver_job_;
        if (!solver_job.setJobResult(output)) {
            log((boost::format("incorrect result format '%s' for job %d") % output % id).str());
            std::scoped_lock lock(job_map_mutex_);
            id_job_map_.insert({id, job_package});
            node_job_map_.insert({job_package->node_, job_package});
            return false;
        }
        job_package->owner_->pushJobResult(solver_job);
        return true;

    } else if (state == JobState::JobTerminated) {
        if (output != "terminate") {
            log((boost::format("job %d has been terminated with reason \"%s\"") % id % output).str());
        }
        std::scoped_lock lock(job_map_mutex_);
        auto id_it = id_job_map_.find(id);
        if (id_it != id_job_map_.end()) {
            id_job_map_.erase(id_it);
            node_job_map_.erase(id_it->second->node_);
        }
        return true;
    }

    assert(false);
    return false;
}

bool JobHandler::handleExtendedMessage(const std::string& message, const std::string& sender)
{
    if (message.find("solver ") == 0) {
        log("command \"" + message + "\" from " + sender);
        pushCommand(message.substr(7));
        return true;
    }
    return false;
}

void JobHandler::onJobConfirmed(std::shared_ptr<Job> job, bool accepted)
{
    if (accepted) { num_loading_++; }
}

void JobHandler::onStateChanged(const std::string& state, size_t loading, size_t capacity, const std::string& details)
{
    num_loading_ = loading;
    num_solvers_ = capacity;
}

void JobHandler::onNetworkError(const std::string& msg)
{
    num_solvers_ = 0;
    num_loading_ = 0;
    std::exit(1); // TODO: recover from network errors?
}

} // namespace gamesolver

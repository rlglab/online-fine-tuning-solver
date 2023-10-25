#pragma once

#include "broker_adapter.h"
#include "gs_mcts.h"
#include "solver_job.h"
#include <deque>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <string>
#include <vector>
#include <list>

template<class DataType>
class SynchronizedDeque : public std::deque<DataType> {
public:
    virtual bool pop(DataType& data);
    virtual bool push(const DataType& data);
protected:
    std::mutex mutex_;
};

namespace gamesolver {

class JobResultDeque : public SynchronizedDeque<SolverJob> {
public:
    virtual bool popJobResult(SolverJob& job_result) { return pop(job_result); }
    virtual bool pushJobResult(const SolverJob& job_result) { return push(job_result); }
};

class CommandDeque : public SynchronizedDeque<std::string> {
public:
    virtual bool popCommand(std::string& command) { return pop(command); }
    virtual bool pushCommand(const std::string& command) { return push(command); }
};

class JobHandler : public chat::BrokerAdapter, public CommandDeque {
public:
    JobHandler() : chat::BrokerAdapter() { initialize(); }
    static JobHandler& instance();

    bool addJob(JobResultDeque* owner, minizero::actor::MCTSNode* node, SolverJob solver_job);
    bool removeJob(JobResultDeque* owner, minizero::actor::MCTSNode* node);
    void removeJobs(JobResultDeque* owner);

    inline int getNumJobs() const { return id_job_map_.size(); }
    inline int getNumSolvers() const { return num_solvers_; }
    inline bool hasIdleSolvers() const { return num_loading_ < num_solvers_; }

private:
    void initialize();

private: // override from chat::BrokerAdapter
    bool onJobCompleted(std::shared_ptr<BrokerAdapter::Job> job) override;
    void onJobConfirmed(std::shared_ptr<Job> job, bool accepted) override;
    void onStateChanged(const std::string& state, size_t loading, size_t capacity, const std::string& details) override;
    void onNetworkError(const std::string& msg) override;
    std::list<std::string> listSubscribedItems() const override { return {"state"}; }
    bool handleExtendedMessage(const std::string& message, const std::string& sender) override;

private:
    struct JobPackage {
        std::shared_ptr<BrokerAdapter::Job> job_;
        minizero::actor::MCTSNode* node_;
        SolverJob solver_job_;
        JobResultDeque* owner_;
    };

private:
    int num_solvers_ = 0;
    int num_loading_ = 0;
    std::mutex job_map_mutex_;
    std::unordered_map<BrokerAdapter::JobID, std::shared_ptr<JobPackage>> id_job_map_;
    std::unordered_map<minizero::actor::MCTSNode*, std::shared_ptr<JobPackage>> node_job_map_;
};

} // namespace gamesolver

template<class DataType>
bool SynchronizedDeque<DataType>::pop(DataType& data)
{
    if (std::deque<DataType>::size()) { // this size() call is for efficiency, not thread-safe
        std::scoped_lock lock(mutex_);
        if (std::deque<DataType>::size()) {
            data = std::deque<DataType>::front();
            std::deque<DataType>::pop_front();
            return true;
        }
    }
    return false;
}

template<class DataType>
bool SynchronizedDeque<DataType>::push(const DataType& data)
{
    std::scoped_lock lock(mutex_);
    std::deque<DataType>::push_back(data);
    return true;
}

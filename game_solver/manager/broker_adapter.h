#pragma once
#include <boost/asio.hpp>
#include <condition_variable>
#include <ctime>
#include <functional>
#include <iostream>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <utility>

namespace chat {

using boost::asio::ip::tcp;
using boost::system::error_code;

class BrokerAdapter {
public:
    typedef size_t JobID;
    static constexpr JobID NullJobID = -1ull;

    enum JobState {
        JobUnconfirmed = 0,
        JobConfirmed = 1,
        JobAssigned = 2,
        JobCompleted = 3,
        JobTerminated = 4
    };

    class Job {
        friend class BrokerAdapter;

    public:
        JobID id() const { return id_; }
        JobState state() const { return state_; }
        std::string command() const { return command_; }
        int code() const { return code_; }
        std::string output(bool decode = true) const { return decode ? decodeOutput(output_) : output_; }

        std::string toString() const;

    private:
        JobID id_ = NullJobID;
        JobState state_ = JobUnconfirmed;
        std::string command_ = {};
        int code_ = -1;
        std::string output_ = {};

        static std::string decodeOutput(const std::string& encode);
    };

public:
    BrokerAdapter(boost::asio::io_context& io_context, const std::string& name = {}, const std::string& broker = "broker");

    BrokerAdapter(const std::string& name = {}, const std::string& broker = "broker");

    virtual ~BrokerAdapter();

    std::shared_ptr<Job> requestJob(const std::string& command, JobState state = JobState::JobConfirmed, time_t timeout = 0);

    std::shared_ptr<Job> requestJob(const std::string& command, const std::string& options, JobState state = JobState::JobConfirmed, time_t timeout = 0);

    std::shared_ptr<Job> terminateJob(std::shared_ptr<Job> job, JobState state = JobState::JobTerminated, time_t timeout = 0);

    std::shared_ptr<Job> waitJobUntil(std::shared_ptr<Job> job, JobState state = JobState::JobCompleted, time_t timeout = 0);

protected:
    virtual bool onJobCompleted(std::shared_ptr<Job> job) { return true; }

    virtual void onJobConfirmed(std::shared_ptr<Job> job, bool accepted) {}

    virtual void onJobAssigned(std::shared_ptr<Job> job, const std::string& worker) {}

    virtual void onStateChanged(const std::string& state, size_t loading, size_t capacity, const std::string& details) {}

    virtual void onCapacityChanged(size_t capacity, const std::string& details) {}

    virtual void onNetworkError(const std::string& msg) {}

protected:
    virtual void handleInput(const std::string& input);

    virtual bool handleExtendedMessage(const std::string& message, const std::string& sender) { return false; }

    virtual void handleReadError(error_code ec, size_t n);

    virtual void handleWriteError(const std::string& data, error_code ec, size_t n);

    virtual void handleConnectError(error_code ec, const tcp::endpoint& remote);

    virtual void handleHandshakeError(const std::string& msg);

protected:
    virtual std::string stringifyRequest(const std::string& command, const std::string& options) const;

    virtual std::list<std::string> listSubscribedItems() const;

    void setAdapterName(const std::string& name) { name_ = name; }

    void setBrokerName(const std::string& broker) { broker_ = broker; }

public:
    bool connect(const std::string& host, int port, time_t timeout = 10000);

    void disconnect();

    void connectAsync(const std::string& host, int port);

    void outputAsync(const std::string& command, bool to_broker = true);

protected:
    void readAsync();

    void writeAsync(const std::string& data);

    void notifyAllWaits();

public:
    void log(const std::string& msg) const;

private:
    static boost::asio::io_context& io_context();

private:
    boost::asio::io_context& io_context_;
    std::unique_ptr<std::thread> thread_;
    tcp::socket socket_;
    std::string buffer_;

    std::string name_;
    std::string broker_;

private:
    std::list<std::shared_ptr<Job>> unconfirmed_;
    std::unordered_map<JobID, std::shared_ptr<Job>> accepted_;

private:
    std::mutex unconfirmed_mutex_;
    std::mutex wait_cv_mutex_;
    std::condition_variable wait_cv_;
};

std::ostream& operator<<(std::ostream& out, const BrokerAdapter::JobState& state);

std::ostream& operator<<(std::ostream& out, const BrokerAdapter::Job& job);

} // namespace chat

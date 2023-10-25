#include "broker_adapter.h"
#include "gs_configuration.h"
#include "time_system.h"
#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/format.hpp>
#include <iomanip>
#include <boost/regex.hpp>

class Logger {
public:
    void log(const std::string& msg) const
    {
        if (gamesolver::broker_logging == false) return;
        std::stringstream ss;
        ss << minizero::utils::TimeSystem::getTimeString("Y-m-d H:i:s.f");
        ss << ' ' << msg;
        std::cerr << ss.str() << std::flush;
    }

    class OstreamAdapter : public std::stringstream {
    public:
        OstreamAdapter(Logger& logref) : std::stringstream(), logref(logref) {}
        OstreamAdapter(OstreamAdapter&& out) : std::stringstream(std::move(out)), logref(out.logref) {}
        OstreamAdapter(const OstreamAdapter&) = delete;
        ~OstreamAdapter() { logref.log(str()); }

    private:
        Logger& logref;
    };

    template <typename type>
    OstreamAdapter operator<<(const type& t)
    {
        OstreamAdapter out(*this);
        out << t;
        return out;
    }
} _log;

namespace chat {

using boost::asio::ip::tcp;
using boost::system::error_code;

using Job = BrokerAdapter::Job;
using JobState = BrokerAdapter::JobState;

std::string Job::toString() const
{
    std::stringstream out;
    switch (state_) {
        default:
            out << boost::format("%llu:%d {%s} %d {%s}") % id_ % state_ % command_ % code_ % output_;
            break;
        case JobState::JobUnconfirmed:
            out << boost::format("? {%s}") % command_;
            break;
        case JobState::JobConfirmed:
            if (id_ != NullJobID)
                out << boost::format("%llu {%s}") % id_ % command_;
            else
                out << boost::format("X {%s}") % command_;
            break;
        case JobState::JobAssigned:
            out << boost::format("%llu {%s} at %s") % id_ % command_ % output_;
            break;
        case JobState::JobCompleted:
            out << boost::format("%llu {%s} %d {%s}") % id_ % command_ % code_ % output_;
            break;
    }
    return out.str();
}

std::string Job::decodeOutput(const std::string& encode)
{
    std::string decode = encode;
    boost::replace_all(decode, "\\n", "\n");
    boost::replace_all(decode, "\\t", "\t");
    boost::replace_all(decode, "\\\\", "\\");
    return decode;
}

std::ostream& operator<<(std::ostream& out, const JobState& state)
{
    const char* names[] = {"unconfirmed", "confirmed", "assigned", "completed", "terminated", "unknown"};
    return out << names[std::min<int>(state, 5)];
}

std::ostream& operator<<(std::ostream& out, const Job& job) { return out << job.toString(); }

BrokerAdapter::BrokerAdapter(boost::asio::io_context& io_context, const std::string& name, const std::string& broker) : io_context_(io_context), socket_(io_context), name_(name), broker_(broker) {}

BrokerAdapter::BrokerAdapter(const std::string& name, const std::string& broker) : BrokerAdapter(io_context(), name, broker) {}

BrokerAdapter::~BrokerAdapter() { disconnect(); }

std::shared_ptr<Job> BrokerAdapter::requestJob(const std::string& command, JobState state, time_t timeout)
{
    return requestJob(command, {}, state, timeout);
}

std::shared_ptr<Job> BrokerAdapter::requestJob(const std::string& command, const std::string& options, JobState state, time_t timeout)
{
    std::shared_ptr<Job> job = std::make_shared<Job>();
    job->state_ = JobState::JobUnconfirmed;
    job->command_ = command;
    std::string request = stringifyRequest(command, options);
    {
        std::scoped_lock lock(unconfirmed_mutex_);
        unconfirmed_.push_back(job);
    }
    outputAsync(request);
    _log << request << " has been sent" << std::endl;
    return (state > job->state()) ? waitJobUntil(job, state, timeout) : job;
}

std::shared_ptr<Job> BrokerAdapter::terminateJob(std::shared_ptr<Job> job, JobState state, time_t timeout)
{
    outputAsync("terminate " + std::to_string(job->id()));
    _log << "terminate " << job->id() << " has been sent" << std::endl;
    return (state > job->state()) ? waitJobUntil(job, state, timeout) : job;
}

std::shared_ptr<Job> BrokerAdapter::waitJobUntil(std::shared_ptr<Job> job, JobState state, time_t timeout)
{
    if (job->state_ < state) {
        std::unique_lock lock(wait_cv_mutex_);
        if (timeout) {
            _log << "wait for request '" << *job << "' becomes " << state << " with at most " << timeout << "ms" << std::endl;
            auto until = std::chrono::system_clock::now() + std::chrono::milliseconds(timeout);
            wait_cv_.wait_until(lock, until, [job, state] { return job->state_ >= state; });
        } else {
            _log << "wait for request '" << *job << "' becomes " << state << std::endl;
            wait_cv_.wait(lock, [job, state] { return job->state_ >= state; });
        }
        if (job->state_ >= state) {
            _log << "stop waiting, '" << *job << "' has become " << job->state_ << std::endl;
        } else {
            _log << "timed out, '" << *job << "' has not become " << state << std::endl;
        }
        return job;
    } else {
        _log << "no need to wait, '" << *job << "' is already " << job->state_ << std::endl;
        return job;
    }
}

boost::regex _regex_message_from("^(\\S+) >> (.+)$");
boost::regex _regex_confirm_request("^(accept|reject) request ([0-9]+)? ?(\\{(.+)\\})?$");
boost::regex _regex_response("^response ([0-9]+) (.+) \\{(.*)\\}$");
boost::regex _regex_notify_assign("^notify assign request ([0-9]+) to (\\S+)$");
boost::regex _regex_notify_state("^notify state (idle|busy|full)( ([0-9]+)/([0-9]+)( (.+))?)?$");
boost::regex _regex_notify_capacity("^notify capacity ([0-9]+) ?(.*)$");
boost::regex _regex_confirm_terminate("^(accept|confirm|reject) terminate ([0-9]+)$");
boost::regex _regex_confirm_protocol("^(accept|reject) protocol (.+)$");

void BrokerAdapter::handleInput(const std::string& input)
{
    boost::smatch match;
    if (boost::regex_match(input, match, _regex_message_from)) {
        // ^(\\S+) >> (.+)$
        std::string sender = match[1].str();
        std::string message = match[2].str();

        if (sender == broker_) {
            if (boost::regex_match(message, match, _regex_confirm_request)) {
                // ^(accept|reject) request ([0-9]+)? ?(\\{(.+)\\})?$
                bool accepted = (match[1].str() == "accept");
                JobID id = accepted ? std::stoull(match[2].str()) : NullJobID;
                std::string command = match[4].str();

                std::function<bool(std::shared_ptr<Job>)> match_request = [](auto) { return false; };
                if (command.size()) { // a new job (requested without id) has been accepted or rejected
                    match_request = [&](std::shared_ptr<Job> job) { return job->command_ == command && job->id_ == NullJobID; };
                } else if (id != NullJobID) { // an accepted job with rejected result has been accepted again
                    match_request = [=](std::shared_ptr<Job> job) { return job->id_ == id; };
                }

                std::shared_ptr<Job> job;
                {
                    std::scoped_lock lock(unconfirmed_mutex_);
                    auto it = std::find_if(unconfirmed_.begin(), unconfirmed_.end(), match_request);
                    if (it != unconfirmed_.end()) {
                        job = *it;
                        unconfirmed_.erase(it);
                    }
                }
                if (job) {
                    job->id_ = id;
                    job->state_ = JobState::JobConfirmed;
                    if (accepted) {
                        accepted_.insert({id, job});
                    }
                    onJobConfirmed(job, accepted);
                    _log << boost::format("confirm %sed request %llu {%s}") % match[1] % id % command << std::endl;

                    notifyAllWaits();

                } else {
                    _log << boost::format("ignore the confirmation of nonexistent request %llu {%s}") % id % command << std::endl;
                }

            } else if (boost::regex_match(message, match, _regex_response)) {
                // ^response ([0-9]+) (.+) \\{(.*)\\}$
                JobID id = std::stoull(match[1].str());
                std::string code = match[2].str();
                std::string output = match[3].str();

                std::shared_ptr<Job> job;
                auto it = accepted_.find(id);
                if (it != accepted_.end()) {
                    job = it->second;
                    accepted_.erase(it);
                }
                if (job) {
                    try {
                        job->code_ = std::stol(code);
                        job->output_ = output;
                        job->state_ = JobState::JobCompleted;
                    } catch (std::invalid_argument&) {
                        job->code_ = -1;
                        job->output_ = code;
                        job->state_ = JobState::JobTerminated;
                    }
                    bool accept = onJobCompleted(job);
                    if (!accept) {
                        std::scoped_lock lock(unconfirmed_mutex_);
                        job->state_ = JobState::JobUnconfirmed;
                        unconfirmed_.push_back(job);
                    }
                    std::string confirm = accept ? "accept" : "reject";
                    outputAsync(confirm + " response " + std::to_string(id));
                    _log << boost::format("%s response %llu %s {%s}") % confirm % id % code % output << std::endl;

                    notifyAllWaits();

                } else {
                    _log << boost::format("ignore the response of nonexistent request %llu") % id << std::endl;
                }

            } else if (boost::regex_match(message, match, _regex_notify_state)) {
                // ^notify state (idle|busy|full)( ([0-9]+)/([0-9]+)( (.+))?)?$
                std::string state = match[1].str();
                size_t loading = 0, capacity = 0;
                if (match[2].str().size()) {
                    loading = std::stoull(match[3].str());
                    capacity = std::stoull(match[4].str());
                }
                std::string details = match[6].str();
                onStateChanged(state, loading, capacity, details);
                _log << boost::format("confirm %s state %s %d/%d %s") % broker_ % state % loading % capacity % details << std::endl;

            } else if (boost::regex_match(message, match, _regex_notify_assign)) {
                // ^notify assign request ([0-9]+) to (\\S+)$
                JobID id = std::stoull(match[1].str());
                std::string worker = match[2].str();

                std::shared_ptr<Job> job;
                auto it = accepted_.find(id);
                if (it != accepted_.end()) {
                    job = it->second;
                }
                if (job) {
                    job->output_ = worker;
                    job->state_ = JobState::JobAssigned;
                    onJobAssigned(job, worker);
                    _log << boost::format("confirm request %llu assigned to worker %s") % id % worker << std::endl;

                    notifyAllWaits();

                } else {
                    _log << boost::format("ignore the confirmation of nonexistent request %llu assigned to worker %s") % id % worker << std::endl;
                }

            } else if (boost::regex_match(message, match, _regex_notify_capacity)) {
                // ^notify capacity ([0-9]+) ?(.*)$
                size_t capacity = std::stoull(match[1].str());
                std::string details = match[2].str();
                onCapacityChanged(capacity, details);
                _log << boost::format("confirm capacity %llu with '%s'") % capacity % details << std::endl;

            } else if (boost::regex_match(message, match, _regex_confirm_terminate)) {
                // ^(accept|confirm|reject) terminate ([0-9]+)$
                bool accepted = (match[1].str() != "reject");
                JobID id = accepted ? std::stoull(match[2].str()) : NullJobID;

                std::shared_ptr<Job> job;
                auto it = accepted_.find(id);
                if (it != accepted_.end()) {
                    job = it->second;
                    accepted_.erase(it);
                }
                if (job) {
                    job->code_ = -1;
                    job->output_ = "terminate";
                    job->state_ = JobState::JobTerminated;
                    bool accept = onJobCompleted(job);
                    if (!accept) {
                        _log << "job terminated by client can not be enqueued again" << std::endl;
                    }
                    _log << boost::format("confirm %sed terminate request %llu") % match[1] % id << std::endl;

                    notifyAllWaits();
                }

            } else if (boost::regex_match(message, match, _regex_confirm_protocol)) {
                // ^(accept|reject) protocol (.+)$
                bool accepted = (match[1].str() == "accept");
                if (accepted) {
                    _log << boost::format("handshake with %s successfully") % broker_ << std::endl;

                    for (const std::string& item : listSubscribedItems()) {
                        outputAsync("subscribe " + item);
                    }

                    notifyAllWaits();
                } else {
                    handleHandshakeError(message);
                }

            } else /* message does not match any watched formats */ {
                if (!handleExtendedMessage(message, sender)) {
                    _log << boost::format("ignore message '%s' from %s") % message % sender << std::endl;
                }
            }

        } else /* if (sender != broker_) */ {
            if (!handleExtendedMessage(message, sender)) {
                _log << boost::format("ignore message '%s' from %s") % message % sender << std::endl;
            }
        }

    } else if (input.size() && input[0] == '%') { // reply from system
        std::string message = input.substr(2);
        if (message.find("name") == 0) {
            std::string name = message.substr(message.find(' ') + 1);
            name_ = name;
            _log << boost::format("confirm client name %s from chat system") % name << std::endl;
            _log << boost::format("send handshake to %s...") % broker_ << std::endl;
            outputAsync("use protocol 0");

        } else if (message.find("failed name") == 0) {
            std::string name = name_;
            unsigned idx = 1;
            try {
                idx = std::stoul(name.substr(name.find_last_of('-') + 1)) + 1;
            } catch (std::exception&) {
            }
            name = name.substr(0, name.find_last_of('-')) + '-' + std::to_string(idx);
            _log << boost::format("adapter name %s has been taken, try %s...") % name_ % name << std::endl;
            outputAsync(name.size() ? "name " + name : "name", false);

        } else if (message.find("failed protocol") == 0) {
            handleHandshakeError("unsupported chat protocol");
        } else if (message.find("failed chat") == 0) {
            handleHandshakeError("broker '" + broker_ + "' is down");
        }

    } else if (input.size() && input[0] == '#') { // notification from system
        std::string message = input.substr(2);
        _log << boost::format("ignore notification '%s'") % message << std::endl;
    }
}

void BrokerAdapter::handleReadError(error_code ec, size_t n)
{
    if (ec == boost::asio::error::eof || ec == boost::asio::error::operation_aborted) {
        _log << "socket input stream closed: " << ec << std::endl;
    } else {
        _log << "unexpected socket read error: " << ec << std::endl;
    }
    std::stringstream ss;
    ss << "read error: " << ec;
    onNetworkError(ss.str());
}
void BrokerAdapter::handleWriteError(const std::string& data, error_code ec, size_t n)
{
    _log << "unexpected socket write error: " << ec << std::endl;
    std::stringstream ss;
    ss << "write error: " << ec << ", " << data;
    onNetworkError(ss.str());
}
void BrokerAdapter::handleConnectError(error_code ec, const tcp::endpoint& remote)
{
    _log << "unexpected socket connect error: " << ec << std::endl;
    std::stringstream ss;
    ss << "connect error: " << ec << " " << remote.address() << ":" << remote.port();
    onNetworkError(ss.str());
}
void BrokerAdapter::handleHandshakeError(const std::string& msg)
{
    _log << "unexpected chat handshake error: " << msg << std::endl;
    std::stringstream ss;
    ss << "handshake error: " << msg;
    onNetworkError(ss.str());
}

std::string BrokerAdapter::stringifyRequest(const std::string& command, const std::string& options) const
{
    std::string request;
    request.reserve(command.size() + options.size() + 20);
    request += "request {";
    request += command;
    request += '}';
    if (options.size()) {
        request += " with ";
        request += options;
    }
    return request;
}

std::list<std::string> BrokerAdapter::listSubscribedItems() const
{
    return {"state", "capacity", "assign"};
}

bool BrokerAdapter::connect(const std::string& host, int port, time_t timeout)
{
    if (thread_) return true;

    connectAsync(host, port);
    std::unique_lock lock(wait_cv_mutex_);
    if (timeout) {
        _log << "wait for the connection with at most " << timeout << "ms" << std::endl;
        auto until = std::chrono::system_clock::now() + std::chrono::milliseconds(timeout);
        if (wait_cv_.wait_until(lock, until, [] { return true; }))
            return true;
        _log << "connection timed out" << std::endl;
        handleHandshakeError("timeout");
        return false;
    } else {
        _log << "wait for the connection" << std::endl;
        wait_cv_.wait(lock, [] { return true; });
        return true;
    }
}

void BrokerAdapter::disconnect()
{
    // TODO: terminate called without an active exception
    if (!thread_) return;
    _log << "disconnecting..." << std::endl;

    boost::asio::post(io_context_, [this]() { socket_.close(); });
    try {
        buffer_.clear();
        thread_->join(); // TODO: io_context_ should not join itself
        thread_.reset();
    } catch (std::exception&) {
    }
    _log << "disconnected" << std::endl;
}

void BrokerAdapter::connectAsync(const std::string& host, int port)
{
    if (thread_) return;
    _log << boost::format("connecting to %s %d...") % host % port << std::endl;

    tcp::resolver resolver(io_context_);
    auto endpoints = resolver.resolve(host, std::to_string(port));

    boost::asio::async_connect(socket_, endpoints,
                               [this](error_code ec, tcp::endpoint remote) {
                                   if (!ec) {
                                       _log << "connected to " << remote.address().to_string() << ':' << remote.port() << std::endl;
                                       boost::asio::socket_base::keep_alive option(true);
                                       socket_.set_option(option);
                                       _log << "send chat room handshake messages" << std::endl;
                                       outputAsync("protocol 0", false);
                                       outputAsync(name_.size() ? "name " + name_ : "name", false);
                                       // start reading from socket input
                                       readAsync();
                                   } else {
                                       handleConnectError(ec, remote);
                                   }
                               });

    thread_ = std::make_unique<std::thread>([this]() { io_context_.run(); });
}

void BrokerAdapter::outputAsync(const std::string& command, bool to_broker)
{
    std::string output;
    output.reserve(command.length() + broker_.length() + 10);
    if (to_broker) {
        output += broker_;
        output += " << ";
    }
    output += command;
    output += '\n';
    writeAsync(output);
    _log << "output '" << command << (to_broker ? "' to broker" : "'") << std::endl;
}

void BrokerAdapter::readAsync()
{
    boost::asio::async_read_until(socket_, boost::asio::dynamic_buffer(buffer_), "\n",
                                  [this](error_code ec, size_t n) {
                                      if (!ec) {
                                          std::string input(buffer_.substr(0, n - 1));
                                          handleInput(input);
                                          buffer_.erase(0, n);
                                          readAsync();
                                      } else {
                                          handleReadError(ec, n);
                                      }
                                  });
}

void BrokerAdapter::writeAsync(const std::string& data) // TODO: check if async_write needs a lock
{
    auto buffer = std::make_shared<std::string>(data);
    boost::asio::async_write(socket_, boost::asio::buffer(*buffer),
                             [this, buffer](error_code ec, size_t n) {
                                 if (!ec) {
                                 } else {
                                     handleWriteError(*buffer, ec, n);
                                 }
                             });
}

void BrokerAdapter::notifyAllWaits()
{
    {
        std::scoped_lock lock(wait_cv_mutex_);
    }
    wait_cv_.notify_all();
}

void BrokerAdapter::log(const std::string& msg) const
{
    _log << msg << std::endl;
}

boost::asio::io_context& BrokerAdapter::io_context()
{
    static boost::asio::io_context io_context;
    return io_context; // reference of a lazy-initialized io_context
}

} // namespace chat

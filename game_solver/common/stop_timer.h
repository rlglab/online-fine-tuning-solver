#pragma once

#include "gs_configuration.h"
#include "time_system.h"

namespace gamesolver {

class StopTimer {
public:
    inline void reset()
    {
        start_count_ = 0;
        stop_count_ = 0;
        accumulated_ptime_ = boost::posix_time::time_duration(0, 0, 0);
    }
    inline void start()
    {
        if (!gamesolver::use_timer_in_tt) { return; }
        start_ptime_ = minizero::utils::TimeSystem::getLocalTime();
        ++start_count_;
    }
    inline void stop()
    {
        if (!gamesolver::use_timer_in_tt) { return; }
        end_ptime_ = minizero::utils::TimeSystem::getLocalTime();
        ++stop_count_;
    }
    inline boost::posix_time::time_duration getElapsedPTime() const { return end_ptime_ - start_ptime_; }
    inline void addAccumulatedPtime() { accumulated_ptime_ += getElapsedPTime(); }
    inline void stopAndAddAccumulatedTime()
    {
        if (!gamesolver::use_timer_in_tt) { return; }
        stop();
        addAccumulatedPtime();
    }
    inline boost::posix_time::time_duration getAccumulatedPTime() const { return accumulated_ptime_; }
    inline uint64_t getStartCount() const { return start_count_; }
    inline uint64_t getStopCount() const { return stop_count_; }

private:
    uint64_t start_count_;
    uint64_t stop_count_;
    boost::posix_time::ptime start_ptime_, end_ptime_;
    boost::posix_time::time_duration accumulated_ptime_;
};

} // namespace gamesolver

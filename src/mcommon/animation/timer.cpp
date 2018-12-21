#include <chrono>     // Timing
#include <iomanip>
#include "mcommon/logger.h"

#include "timer.h"

namespace COMMON_NAMESPACE
{

struct TimedSection
{
    uint64_t elapsed = 0;
    uint64_t count;
};

timer globalTimer;

uint64_t timer_get_time_now()
{
    return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

void timer_start(timer& timer)
{
    timer_restart(timer);
}

void timer_restart(timer& timer)
{
    timer.startTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

uint64_t timer_get_elapsed(const timer& timer)
{
    auto now = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    return now - timer.startTime;
}

double timer_get_elapsed_seconds(const timer& timer)
{
    return timer_to_seconds(timer_get_elapsed(timer));
}

double timer_to_seconds(uint64_t value)
{
    return double(value / 1000000.0);
}

double timer_to_ms(uint64_t value)
{
    return double(value / 1000.0);
}

void profile_begin(const char* pszText)
{
}

void profile_marker(const char* pszText)
{
}

void profile_end()
{
}

TimerBlock::TimerBlock(const std::string& timer)
    :strTimer(timer)
{
    timer_start(blockTimer);
}

TimerBlock::~TimerBlock()
{
    elapsed = timer_get_elapsed(blockTimer);
    LOG(INFO) << "[timer] " << strTimer << " : " << elapsed / 1000 <<  " msec";
}

} // mcommon
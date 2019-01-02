#pragma once

#include "common_namespace.h"
#include <cstdint>
#include <string>

namespace COMMON_NAMESPACE
{

enum class TimerSample : uint32_t
{
    None,
    Restart
};

struct timer
{
    int64_t startTime;
};
extern timer globalTimer;

uint64_t timer_get_time_now();
void timer_restart(timer& timer);
void timer_start(timer& timer);
uint64_t timer_get_elapsed(const timer& timer);
double timer_get_elapsed_seconds(const timer& timer);
double timer_to_seconds(uint64_t value);
double timer_to_ms(uint64_t value);

void profile_begin(const char* pszText);
void profile_marker(const char* pszText);
void profile_end();

class TimerBlock
{
public:
    std::string strTimer;
    timer blockTimer;
    uint64_t elapsed;

    TimerBlock(const std::string& timer);
    ~TimerBlock();
};

#define TIME_SCOPE(name) TimerBlock name##_timer_block(#name);

} // namespace COMMON_NAMESPACE

#include "timer.h"

#include <chrono>

namespace Zep
{
Timer::Timer()
{
    Restart();
}

Timer& Timer::GlobalTimer()
{
    static Timer timer;
    return timer;
}

float Timer::GetTime() const
{
    auto time = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    return float(time / 1000000.0);
}

float Timer::GetDelta() const
{
    auto now = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    auto diff = now - m_startTime;
    return float(diff / 1000000.0);
}

void Timer::Restart()
{
    m_startTime = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}
}

#pragma once

#include <cstdint>

enum class TimerSample
{
    None,
    Restart
};

class Timer
{
public:
    Timer();
    static Timer& GlobalTimer();

    void Restart();

    float GetTime() const;
    float GetDelta() const;

private:
    int64_t m_startTime;
};
#pragma once
#include <chrono>

using TimePoint = std::chrono::time_point<std::chrono::high_resolution_clock>;
inline float time_to_float_seconds(const TimePoint& pt)
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(pt.time_since_epoch()).count() / 1000.0f;
}

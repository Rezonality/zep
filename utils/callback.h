#pragma once

#include <functional>
#include "animation/timer.h"

namespace Mgfx
{

enum class SchedulerState
{
    Off = 0,
    Started = 1,
    Triggered = 2
};

struct scheduler
{
    std::function<void()> callback;
    timer timer;
    float timeout = 1.0f;
    SchedulerState state = SchedulerState::Off;
};

void scheduler_start(scheduler& sched, float seconds, std::function<void()> fnCallback);
void scheduler_stop(scheduler& sched);
void scheduler_update(scheduler& sched);

} // Mgfx

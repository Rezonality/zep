#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <vector>

#include <zep/mcommon/logger.h>
#include "thread_utils.h"

#include <concurrentqueue/concurrentqueue.h>

//#include <ctti/type_id.hpp>

#include "time_utils.h"

#undef ERROR

struct ITimeConsumer
{
    virtual void Tick() = 0;
};

class TimeProvider
{
public:
    TimeProvider();
    static TimeProvider& Instance();

    void Free();

    TimePoint Now() const;
    TimePoint StartTime() const;

    void ResetStartTime();
    void RegisterConsumer(ITimeConsumer* pConsumer);
    void UnRegisterConsumer(ITimeConsumer* pConsumer);
    void StartThread();
    void EndThread();

    void SetTempo(double bpm, double quantum);
    void SetBeat(double beat);
    void SetFrame(uint32_t frame);

    uint32_t GetFrame() const;
    double GetBeat() const;
    double GetTempo() const;
    double GetQuantum() const;
    std::chrono::microseconds GetTimePerBeat() const;

    double GetBeatAtTime(TimePoint time);

private:
    TimePoint m_startTime;
    std::unordered_set<ITimeConsumer*> m_consumers;
    audio_spin_mutex m_spin_mutex;

    std::atomic_bool m_quitTimer = false;
    std::thread m_tickThread;
    std::atomic<double> m_quantum = 4;
    std::atomic<double> m_tempo = 120;
    std::atomic<double> m_beat = 0;
    std::atomic<uint32_t> m_frame = 0;

    TimePoint m_lastTime; // Should probably be atomic, but compile error on GCC?
    std::atomic<double> m_lastBeat;
    std::chrono::microseconds m_timePerBeat = std::chrono::microseconds(1000000 / 120);
};


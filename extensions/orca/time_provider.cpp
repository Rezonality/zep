#include <algorithm>

#include "time_provider.h"

using namespace std::chrono;

TimeProvider& TimeProvider::Instance()
{
    static TimeProvider provider;
    return provider;
}

TimeProvider::TimeProvider()
{
    m_startTime = Now();
    SetTempo(120.0, 4.0);
}

void TimeProvider::Free()
{
    EndThread();
}

TimePoint TimeProvider::Now() const
{
    return std::chrono::high_resolution_clock::now();
}

TimePoint TimeProvider::StartTime() const
{
    return m_startTime;
}

void TimeProvider::ResetStartTime()
{
    m_startTime = Now();
}

void TimeProvider::RegisterConsumer(ITimeConsumer* pConsumer)
{
    LOCK_GUARD(m_spin_mutex, TP_Lock);
    m_consumers.insert(pConsumer);
}

void TimeProvider::UnRegisterConsumer(ITimeConsumer* pConsumer)
{
    LOCK_GUARD(m_spin_mutex, TP_Lock);
    m_consumers.erase(pConsumer);
}

void TimeProvider::StartThread()
{
    auto lastTime = TimeProvider::Instance().Now();

    // A thread which wakes up and ticks
    m_quitTimer = false;
    m_tickThread = std::thread([&]() {
        for (;;)
        {
            // Beat at regular intervals, no matter how long our operation takes
            auto startTime = TimeProvider::Instance().Now();
            auto nextTime = startTime + m_timePerBeat;
            auto beat = m_beat.load();
            auto frame = m_frame.load();

            {
                LOCK_GUARD(m_spin_mutex, TP_Lock);

                m_lastTime = startTime;
                m_lastBeat = beat;

                for (auto& consumer : m_consumers)
                {
                    consumer->Tick();
                }

                m_frame.store(frame + 1);
                m_beat.store(beat + 1);
            }

            if (m_quitTimer.load())
            {
                break;
            }

            std::this_thread::sleep_until(nextTime);
        }
    });
}

void TimeProvider::EndThread()
{
    m_quitTimer = true;
    if (m_tickThread.joinable())
    {
        m_tickThread.join();
    }
}

double TimeProvider::GetBeatAtTime(TimePoint time)
{
    auto d = duration_cast<microseconds>(time - m_lastTime).count();
    auto beat = m_lastBeat.load();

    auto beatsPerTime = (double)d / (double)duration_cast<microseconds>(GetTimePerBeat()).count();
    return beat + beatsPerTime;
}

// TODO:
// Current m_beat is actualy an integer value which increments every tick.
// The ticks are designed to be very regular, but should the tick event
// broadcast the time and beat?
double TimeProvider::GetBeat() const
{
    return m_beat;
}

double TimeProvider::GetTempo() const
{
    return m_tempo;
}

double TimeProvider::GetQuantum() const
{
    return m_quantum;
}

void TimeProvider::SetTempo(double tempo, double quantum)
{
    LOCK_GUARD(m_spin_mutex, TP_Lock);
    m_quantum = quantum;
    m_tempo = tempo;
    m_timePerBeat = microseconds((uint64_t)(60000000.0 / tempo));
}

void TimeProvider::SetBeat(double beat)
{
    LOCK_GUARD(m_spin_mutex, TP_Lock);
    m_beat = beat;
}

void TimeProvider::SetFrame(uint32_t frame)
{
    LOCK_GUARD(m_spin_mutex, TP_Lock);
    m_frame.store(frame);
}

uint32_t TimeProvider::GetFrame() const
{
    return m_frame.load();
}

std::chrono::microseconds TimeProvider::GetTimePerBeat() const
{
    return m_timePerBeat;
}

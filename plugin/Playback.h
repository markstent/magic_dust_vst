#pragma once

#include <algorithm>
#include <array>
#include <bitset>
#include <cmath>
#include <cstdint>
#include <vector>

#include "md/types.hpp"

namespace md
{

struct PlayEvent
{
    int tick;
    std::uint8_t channel;
    std::uint8_t note;
    std::uint8_t velocity; // 0 for off
    bool on;
};

// Sorted by tick, offs before ons at the same tick. An off at lengthTicks fires before the loop
// restarts.
struct PlaybackPattern
{
    std::vector<PlayEvent> events;
    int lengthTicks;
};

PlaybackPattern makePattern(const Idea& idea);

struct ScheduledEvent
{
    int sampleOffset;
    PlayEvent event;
};

class SoundingNotes
{
  public:
    void apply(const PlayEvent& e) { held_.at(e.channel % 16).set(e.note % 128, e.on); }

    // NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward): a callback invoked many times
    template <class F> void releaseAll(F&& emitOff)
    {
        std::uint8_t channel = 0;
        for (std::bitset<128>& notes : held_)
        {
            for (std::uint8_t note = 0; note < 128; ++note)
            {
                if (notes.test(note))
                {
                    emitOff(channel, note);
                }
            }
            notes.reset();
            ++channel;
        }
    }

  private:
    std::array<std::bitset<128>, 16> held_;
};

constexpr double kPpqTicks = 960.0;

// Calls emit for every event of the looping pattern that falls in the block starting at startPpq.
// NOLINTBEGIN(bugprone-easily-swappable-parameters,cppcoreguidelines-missing-std-forward)
template <class F>
void schedule(const PlaybackPattern& p, double startPpq, double bpm, double sampleRate,
              int numSamples, F&& emit)
{
    if (p.events.empty() || p.lengthTicks <= 0 || numSamples <= 0)
    {
        return;
    }
    const double loopPpq = p.lengthTicks / kPpqTicks;
    const double ppqPerSample = bpm / 60.0 / sampleRate;
    const double endPpq = startPpq + (numSamples * ppqPerSample);
    // One pass back, so an off at the very end of the previous pass still fires at a block start.
    const auto firstPass =
        std::max(0LL, static_cast<long long>(std::floor(startPpq / loopPpq)) - 1);
    for (auto pass = firstPass; static_cast<double>(pass) * loopPpq < endPpq; ++pass)
    {
        for (const PlayEvent& e : p.events)
        {
            const double eventPpq = (static_cast<double>(pass) * loopPpq) + (e.tick / kPpqTicks);
            if (eventPpq >= endPpq)
            {
                break;
            }
            if (eventPpq < startPpq)
            {
                continue;
            }
            const double offset = std::floor((eventPpq - startPpq) / ppqPerSample);
            emit(ScheduledEvent{.sampleOffset =
                                    static_cast<int>(std::clamp(offset, 0.0, numSamples - 1.0)),
                                .event = e});
        }
    }
}
// NOLINTEND(bugprone-easily-swappable-parameters,cppcoreguidelines-missing-std-forward)

class InternalClock
{
  public:
    void start()
    {
        ppq_ = 0.0;
        running_ = true;
    }
    void stop() { running_ = false; }
    [[nodiscard]] bool running() const { return running_; }
    [[nodiscard]] double ppq() const { return ppq_; }
    void advance(int numSamples, double bpm, double sampleRate)
    {
        if (running_)
        {
            ppq_ += numSamples * bpm / 60.0 / sampleRate;
        }
    }

  private:
    double ppq_ = 0.0;
    bool running_ = false;
};

} // namespace md

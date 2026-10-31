#pragma once

#include <algorithm>
#include <array>
#include <bitset>
#include <cmath>
#include <cstdint>
#include <vector>

#include "md/ranges.hpp"
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

// Events must be sorted by tick, offs before ons at the same tick (makePattern does this). An off
// at lengthTicks fires before the loop restarts.
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
    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access): the indices are
    // reduced below the sizes, and .at() could throw on the audio thread.
    void apply(const PlayEvent& e) { held_[e.channel % 16][e.note % 128] = e.on; }
    [[nodiscard]] bool holds(const PlayEvent& e) const
    {
        return held_[e.channel % 16][e.note % 128];
    }

    // NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward): a callback invoked many times
    template <class F> void releaseAll(F&& emitOff)
    {
        std::uint8_t channel = 0;
        for (std::bitset<128>& notes : held_)
        {
            for (std::uint8_t note = 0; note < 128; ++note)
            {
                if (notes[note])
                {
                    emitOff(channel, note);
                }
            }
            notes.reset();
            ++channel;
        }
    }

    // NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward): a callback invoked many times
    template <class F> void releaseChannel(std::uint8_t channel, F&& emitOff)
    {
        std::bitset<128>& notes = held_[channel % 16];
        for (std::uint8_t note = 0; note < 128; ++note)
        {
            if (notes[note])
            {
                emitOff(channel, note);
            }
        }
        notes.reset();
    }

    // NOLINTEND(cppcoreguidelines-pro-bounds-avoid-unchecked-container-access)

  private:
    std::array<std::bitset<128>, 16> held_;
};

// The one expression for how far the beat position moves in a block, so a block's end and the next
// block's start are the same double.
inline double ppqForSamples(int numSamples, double bpm, double sampleRate)
{
    return numSamples * bpm / 60.0 / sampleRate;
}

// Calls emit for every event of the looping pattern that falls in the block starting at startPpq.
template <class F>
// NOLINTBEGIN(bugprone-easily-swappable-parameters,cppcoreguidelines-missing-std-forward): the
// signature is fixed by the spec and emit is a callback invoked many times, never moved.
void schedule(const PlaybackPattern& p, double startPpq, double bpm, double sampleRate,
              int numSamples, F&& emit)
// NOLINTEND(bugprone-easily-swappable-parameters,cppcoreguidelines-missing-std-forward)
{
    if (p.events.empty() || p.lengthTicks <= 0 || numSamples <= 0)
    {
        return;
    }
    const double loopPpq = p.lengthTicks / static_cast<double>(kTicksPerQuarter);
    const double endPpq = startPpq + ppqForSamples(numSamples, bpm, sampleRate);
    const auto firstPass = static_cast<long long>(std::floor(startPpq / loopPpq));
    for (auto pass = firstPass; static_cast<double>(pass) * loopPpq < endPpq; ++pass)
    {
        for (const PlayEvent& e : p.events)
        {
            const double eventPpq = (static_cast<double>(pass) * loopPpq) +
                                    (e.tick / static_cast<double>(kTicksPerQuarter));
            // An off at lengthTicks belongs to the block that ends the pass, so its edges are
            // (start, end]; every other event uses [start, end).
            const bool passEnd = e.tick >= p.lengthTicks;
            if (passEnd ? eventPpq > endPpq : eventPpq >= endPpq)
            {
                break;
            }
            if (passEnd ? eventPpq <= startPpq : eventPpq < startPpq)
            {
                continue;
            }
            const double offset = std::floor((eventPpq - startPpq) * 60.0 / bpm * sampleRate);
            emit(ScheduledEvent{.sampleOffset =
                                    static_cast<int>(std::clamp(offset, 0.0, numSamples - 1.0)),
                                .event = e});
        }
    }
}

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
            ppq_ += ppqForSamples(numSamples, bpm, sampleRate);
        }
    }

  private:
    double ppq_ = 0.0;
    bool running_ = false;
};

} // namespace md

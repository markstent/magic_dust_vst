#include "internal.hpp"

#include "md/pcg32.hpp"
#include "md/ranges.hpp"
#include "md/theory.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <span>
#include <vector>

namespace md::detail
{
namespace
{
enum class Role : std::uint8_t
{
    root,
    octave,
    fifth,
};

struct Slot
{
    int step;
    int length;
    Role role;
};

constexpr Slot hit(int step, int length, Role role)
{
    return {.step = step, .length = length, .role = role};
}

constexpr Role R = Role::root;
constexpr Role O = Role::octave;
constexpr Role F = Role::fifth;

constexpr std::array kRolling = std::to_array<Slot>({
    hit(2, 1, R),
    hit(3, 1, R),
    hit(6, 1, R),
    hit(7, 1, R),
    hit(10, 1, R),
    hit(11, 1, R),
    hit(14, 1, R),
    hit(15, 1, R),
});
constexpr std::array kOffbeat =
    std::to_array<Slot>({hit(2, 2, R), hit(6, 2, R), hit(10, 2, R), hit(14, 2, R)});
constexpr std::array kBounce = std::to_array<Slot>({
    hit(2, 1, R),
    hit(3, 1, O),
    hit(6, 1, R),
    hit(7, 1, O),
    hit(10, 1, R),
    hit(11, 1, O),
    hit(14, 1, R),
    hit(15, 1, O),
});
constexpr std::array kGarage =
    std::to_array<Slot>({hit(3, 1, R), hit(6, 2, R), hit(11, 1, R), hit(14, 2, F)});
constexpr std::array kSustained = std::to_array<Slot>({hit(0, 16, R)});

std::span<const Slot> slotsFor(BassPattern pattern)
{
    switch (pattern)
    {
    case BassPattern::rollingOffbeat:
        return kRolling;
    case BassPattern::octaveBounce:
        return kBounce;
    case BassPattern::garageSkip:
        return kGarage;
    case BassPattern::sustained:
        return kSustained;
    default:
        return kOffbeat; // offbeat and pedal
    }
}

// The chord root in 33 to 44.
int rootNote(int rootPc) { return 33 + ((rootPc + 12 - (33 % 12)) % 12); }

int pitchFor(Role role, int root)
{
    switch (role)
    {
    case Role::octave:
        // Up an octave when that stays in range, otherwise down, otherwise (roots with pitch class
        // 1 to 3 have no octave pair in range) the fifth.
        if (root + 12 <= kBassHigh)
        {
            return root + 12;
        }
        return root - 12 >= kBassLow ? root - 12 : pitchFor(Role::fifth, root);
    case Role::fifth:
        return root + 7 <= kBassHigh ? root + 7 : root - 5;
    case Role::root:
        break;
    }
    return root;
}

// Sparse to busy.
int densityRank(BassPattern pattern)
{
    switch (pattern)
    {
    case BassPattern::sustained:
        return 0;
    case BassPattern::pedal:
        return 1;
    case BassPattern::offbeat:
        return 2;
    case BassPattern::callResponse:
        return 3;
    case BassPattern::garageSkip:
        return 4;
    case BassPattern::octaveBounce:
        return 5;
    case BassPattern::riff:
        return 6;
    case BassPattern::rollingOffbeat:
        break;
    }
    return 7;
}

bool variesPitch(BassPattern pattern)
{
    return pattern == BassPattern::rollingOffbeat || pattern == BassPattern::offbeat ||
           pattern == BassPattern::garageSkip || pattern == BassPattern::callResponse;
}

// The chord's fifth (5 or b5 above its root) in range, up from the root when that fits.
int chordFifth(const ChordSymbol& chord, int root)
{
    const std::vector<int> intervals = chordIntervals(chord.quality);
    const auto it = std::ranges::find_if(intervals, [](int i) { return i == 6 || i == 7; });
    const int fifth = it == intervals.end() ? 0 : *it;
    return root + fifth <= kBassHigh ? root + fifth : root + fifth - 12;
}

// Root 60%, octave 25%, fifth 15%.
int variedPitch(const ChordSymbol& chord, int root, Pcg32& rng)
{
    const std::uint32_t roll = rng.below(100);
    int pitch = root;
    if (roll >= 85)
    {
        pitch = chordFifth(chord, root);
    }
    else if (roll >= 60)
    {
        pitch = root + 12 <= kBassHigh ? root + 12 : root - 12;
    }
    return pitch >= kBassLow && pitch <= kBassHigh ? pitch : root;
}

bool inScale(const std::vector<int>& scale, int pitch)
{
    return std::ranges::find(scale, pitch % 12) != scale.end();
}

// The nearest scale pitch in direction dir (1 or -1), at most 2 semitones from the target.
int scaleStep(const std::vector<int>& scale, int target, int dir)
{
    for (int step = 1; step <= 2; ++step)
    {
        if (inScale(scale, target + (dir * step)))
        {
            return target + (dir * step);
        }
    }
    return target;
}

// Half step below 40% (only when in the scale, else the scale step below), scale step above 30%,
// scale step below 30%. A pitch outside the range mirrors to the other side.
int approachPitch(const std::vector<int>& scale, int target, Pcg32& rng)
{
    const std::uint32_t roll = rng.below(100);
    const int dir = roll >= 40 && roll < 70 ? 1 : -1;
    const bool half = roll < 40 && inScale(scale, target - 1);
    const auto pitchTo = [&](int d)
    { return half && d < 0 ? target - 1 : scaleStep(scale, target, d); };
    const int pitch = pitchTo(dir);
    return pitch >= kBassLow && pitch <= kBassHigh ? pitch : pitchTo(-dir);
}

// Replaces the hit on the last swung 16th before each chord change (and before the loop seam) with
// a 16th approach note to the first bass note at the change. Events must be sorted by start.
void addApproachNotes(const Idea& idea, std::uint64_t bassSeed, std::vector<NoteEvent>& events)
{
    Pcg32 rng(bassSeed, kBassApproach);
    const std::vector<int> scale = scalePitchClasses(idea.key);
    const int end = idea.settings.bars * kTicksPerBar;
    const int swing = swingPermille(idea.settings.groove);
    std::vector<int> changes;
    for (std::size_t i = 1; i < idea.progression.size(); ++i)
    {
        changes.push_back(idea.progression.at(i).startBar * kTicksPerBar);
    }
    changes.push_back(end);
    std::vector<NoteEvent> approaches;
    for (const int change : changes)
    {
        const auto target = change == end
                                ? events.begin()
                                : std::ranges::find_if(events, [change](const NoteEvent& e)
                                                       { return e.start >= change; });
        if (target == events.end())
        {
            continue;
        }
        const int pitch = approachPitch(scale, target->pitch, rng);
        approaches.push_back({.pitch = pitch,
                              .start = swung(change - kTicksPerSixteenth, swing),
                              .length = kTicksPerSixteenth,
                              .velocity = 85});
    }
    std::erase_if(
        events, [&](const NoteEvent& e)
        { return std::ranges::find(approaches, e.start, &NoteEvent::start) != approaches.end(); });
    events.insert(events.end(), approaches.begin(), approaches.end());
    std::ranges::stable_sort(events, {}, &NoteEvent::start);
}
} // namespace

BassPattern bassPatternFor(const StyleEntry& entry, int energy, Pcg32& rng)
{
    std::vector<BassPattern> allowed = entry.bassPatterns;
    std::ranges::stable_sort(allowed, {}, densityRank);
    const int n = static_cast<int>(allowed.size());
    std::vector<int> weights;
    for (int k = 0; k < n; ++k)
    {
        // 1 + 4 * max(0, 1 - 2 * |k / (n - 1) - energy / 100|), scaled by 100 * (n - 1).
        const int scale = 100 * std::max(n - 1, 1);
        const int distance = std::abs((100 * k) - (energy * (n - 1)));
        weights.push_back(scale + (4 * std::max(0, scale - (2 * distance))));
    }
    const BassPattern chosen = allowed.at(rng.pickWeighted(weights));
    // The riff and call-and-response patterns arrive in a later phase.
    return chosen == BassPattern::riff || chosen == BassPattern::callResponse ? BassPattern::offbeat
                                                                              : chosen;
}

std::vector<NoteEvent> bassEvents(const Idea& idea, BassPattern pattern, std::uint64_t bassSeed)
{
    Pcg32 pitchRng(bassSeed, kBassPitch);
    const PlacedChord* previous = nullptr;
    const int swing = swingPermille(idea.settings.groove);
    std::vector<NoteEvent> events;
    for (int bar = 0; bar < idea.settings.bars; ++bar)
    {
        const PlacedChord& chord = chordAt(idea.progression, bar * kTicksPerBar);
        const int rootPc =
            pattern == BassPattern::pedal ? idea.key.root : chordRootPc(idea.key, chord.symbol);
        const int root = rootNote(rootPc);
        for (const Slot& slot : slotsFor(pattern))
        {
            const bool varies =
                variesPitch(pattern) && slot.role == Role::root && &chord == previous;
            previous = &chord;
            const NoteEvent note{
                .pitch =
                    varies ? variedPitch(chord.symbol, root, pitchRng) : pitchFor(slot.role, root),
                .start = swung((bar * kTicksPerBar) + (slot.step * kTicksPerSixteenth), swing),
                .length = slot.length * kTicksPerSixteenth,
                .velocity = 100,
            };
            events.push_back(note);
        }
    }
    if (idea.settings.energy > 50 && pattern != BassPattern::sustained &&
        pattern != BassPattern::pedal)
    {
        addApproachNotes(idea, bassSeed, events);
    }
    clampMonophonic(events, idea.settings.bars * kTicksPerBar);
    return events;
}
} // namespace md::detail

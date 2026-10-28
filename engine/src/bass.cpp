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
           pattern == BassPattern::garageSkip;
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
    const auto approachTick = [swing](int change)
    { return swung(change - kTicksPerSixteenth, swing); };
    std::vector<NoteEvent> approaches;
    for (std::size_t i = 0; i < changes.size(); ++i)
    {
        const int change = changes.at(i);
        const auto target = change == end
                                ? events.begin()
                                : std::ranges::find_if(events, [change](const NoteEvent& e)
                                                       { return e.start >= change; });
        // A change with no bass note before the next change's approach has nothing to lead into.
        if (target == events.end() ||
            target->start >= approachTick(changes.at((i + 1) % changes.size())))
        {
            continue;
        }
        const int pitch = approachPitch(scale, target->pitch, rng);
        approaches.push_back({.pitch = pitch,
                              .start = approachTick(change),
                              .length = kTicksPerSixteenth,
                              .velocity = 85});
    }
    std::erase_if(
        events, [&](const NoteEvent& e)
        { return std::ranges::find(approaches, e.start, &NoteEvent::start) != approaches.end(); });
    events.insert(events.end(), approaches.begin(), approaches.end());
    std::ranges::stable_sort(events, {}, &NoteEvent::start);
}
// Each chord span's first note is its root and the rest vary (slot 2) on the patterns that do.
std::vector<NoteEvent> slotEvents(const Idea& idea, BassPattern pattern, std::uint64_t bassSeed)
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
    return events;
}

constexpr int kRiffSpanSteps = 2 * 16;
constexpr std::array kRiffSteps = std::to_array({1, 2, 3, 5, 6, 7, 9, 10, 11, 13, 14, 15});

struct Motif
{
    std::vector<int> steps;   // 16th steps within the 2-bar span, sorted
    std::vector<int> degrees; // scale-degree offsets from the chord root, one per onset
};

// 4 to 6 onsets at least 2 steps apart, each with a scale-degree offset of -3 to 3.
Motif drawMotif(Pcg32& rng)
{
    std::vector<int> pool;
    for (int bar = 0; bar < 2; ++bar)
    {
        for (const int step : kRiffSteps)
        {
            pool.push_back((16 * bar) + step);
        }
    }
    Motif motif;
    const int count = 4 + static_cast<int>(rng.below(3));
    for (int n = 0; n < count; ++n)
    {
        const int step = pool.at(rng.below(static_cast<std::uint32_t>(pool.size())));
        motif.steps.push_back(step);
        std::erase_if(pool, [step](int s) { return std::abs(s - step) <= 1; });
    }
    std::ranges::sort(motif.steps);
    for (int n = 0; n < count; ++n)
    {
        motif.degrees.push_back(static_cast<int>(rng.below(7)) - 3);
    }
    return motif;
}

struct Onset
{
    int step; // 16th step in the idea
    int degree;
};

// A riff note's place: the chord root under it and its swung start tick.
struct Spot
{
    int root;
    int start;
};

// The pitch |onset.degree| scale steps up (positive) or down (negative) from the root.
int scaleDegree(const std::vector<int>& scale, int root, const Onset& onset)
{
    const int dir = onset.degree < 0 ? -1 : 1;
    int pitch = root;
    for (int left = std::abs(onset.degree); left > 0; --left)
    {
        pitch += dir;
        while (!inScale(scale, pitch))
        {
            pitch += dir;
        }
    }
    return pitch;
}

// The root always passes. Others must be in the scale, within 7 semitones of the root, in the bass
// range and not a semitone from a tone of the chord sounding at the note's start.
bool riffPitchPasses(const Idea& idea, const std::vector<int>& scale, const Spot& spot, int pitch)
{
    if (pitch == spot.root)
    {
        return true;
    }
    if (!inScale(scale, pitch) || std::abs(pitch - spot.root) > 7 || pitch < kBassLow ||
        pitch > kBassHigh)
    {
        return false;
    }
    const std::vector<int> sounding = soundingPcsAt(idea, spot.start);
    const int pc = pitch % 12;
    return std::ranges::find(sounding, pc) != sounding.end() ||
           std::ranges::none_of(
               sounding,
               [pc](int tone) { return (tone - pc + 12) % 12 == 1 || (pc - tone + 12) % 12 == 1; });
}

// The nearest passing pitch to the candidate, ties down.
int nearestPassing(const Idea& idea, const std::vector<int>& scale, const Spot& spot, int candidate)
{
    for (int d = 0;; ++d)
    {
        for (const int pitch : {candidate - d, candidate + d})
        {
            if (riffPitchPasses(idea, scale, spot, pitch))
            {
                return pitch;
            }
        }
    }
}

// One 2-bar rhythm and set of degree offsets (slot 4), repeated in every 2-bar span. Each chord
// span applies the offsets to its own root, with the span's first onset on the root.
std::vector<NoteEvent> riffEvents(const Idea& idea, std::uint64_t bassSeed)
{
    Pcg32 rng(bassSeed, kBassRiff);
    const Motif motif = drawMotif(rng);
    const std::vector<int> scale = scalePitchClasses(idea.key);
    const int swing = swingPermille(idea.settings.groove);
    const int totalSteps = idea.settings.bars * 16;
    std::vector<Onset> onsets;
    for (int span = 0; span * kRiffSpanSteps < totalSteps; ++span)
    {
        for (std::size_t k = 0; k < motif.steps.size(); ++k)
        {
            const int step = (span * kRiffSpanSteps) + motif.steps.at(k);
            if (step < totalSteps)
            {
                onsets.push_back({.step = step, .degree = motif.degrees.at(k)});
            }
        }
    }
    std::vector<NoteEvent> events;
    const PlacedChord* previous = nullptr;
    for (std::size_t k = 0; k < onsets.size(); ++k)
    {
        const Onset& onset = onsets.at(k);
        const int start = swung(onset.step * kTicksPerSixteenth, swing);
        const PlacedChord& chord = chordAt(idea.progression, start);
        const int root = rootNote(chordRootPc(idea.key, chord.symbol));
        const int pitch = &chord == previous
                              ? nearestPassing(idea, scale, {.root = root, .start = start},
                                               scaleDegree(scale, root, onset))
                              : root;
        previous = &chord;
        const int nextStep = k + 1 < onsets.size() ? onsets.at(k + 1).step : totalSteps;
        events.push_back(
            {.pitch = pitch,
             .start = start,
             .length = std::clamp(nextStep - onset.step - 1, 1, 3) * kTicksPerSixteenth,
             .velocity = 100});
    }
    return events;
}

// Per bar, 1 + round(2 * energy / 100) notes (slot 5) on non-downbeat 16ths where no chord note
// starts, the 16th right after a chord note ends weighing 3 and the others 1. Each lasts 1 or 2
// sixteenths and stops at the next chord note start.
std::vector<NoteEvent> callResponseEvents(const Idea& idea, std::uint64_t bassSeed)
{
    Pcg32 rng(bassSeed, kBassCall);
    Pcg32 pitchRng(bassSeed, kBassPitch);
    const std::vector<NoteEvent>& chordNotes = idea.lanes.at(0).events;
    const int swing = swingPermille(idea.settings.groove);
    const int totalSteps = idea.settings.bars * 16;
    std::vector<int> afterChordNote;
    for (const NoteEvent& c : chordNotes)
    {
        int step = 0;
        while (step < totalSteps && swung(step * kTicksPerSixteenth, swing) < c.start + c.length)
        {
            ++step;
        }
        afterChordNote.push_back(step);
    }
    const int count = 1 + (((2 * idea.settings.energy) + 50) / 100);
    std::vector<NoteEvent> events;
    const PlacedChord* previous = nullptr;
    for (int bar = 0; bar < idea.settings.bars; ++bar)
    {
        std::vector<int> candidates;
        std::vector<int> weights;
        for (int s = 1; s < 16; ++s)
        {
            const int step = (bar * 16) + s;
            const int start = swung(step * kTicksPerSixteenth, swing);
            if (s % 4 != 0 &&
                std::ranges::find(chordNotes, start, &NoteEvent::start) == chordNotes.end())
            {
                candidates.push_back(step);
                weights.push_back(
                    std::ranges::find(afterChordNote, step) != afterChordNote.end() ? 3 : 1);
            }
        }
        std::vector<int> picked;
        for (std::size_t n = 0; n < std::min<std::size_t>(count, candidates.size()); ++n)
        {
            const auto i = static_cast<std::ptrdiff_t>(rng.pickWeighted(weights));
            picked.push_back(candidates.at(static_cast<std::size_t>(i)));
            candidates.erase(candidates.begin() + i);
            weights.erase(weights.begin() + i);
        }
        std::ranges::sort(picked);
        for (const int step : picked)
        {
            const int start = swung(step * kTicksPerSixteenth, swing);
            const PlacedChord& chord = chordAt(idea.progression, start);
            const int root = rootNote(chordRootPc(idea.key, chord.symbol));
            const auto next = std::ranges::find_if(chordNotes, [start](const NoteEvent& c)
                                                   { return c.start > start; });
            const int length = (1 + static_cast<int>(rng.below(2))) * kTicksPerSixteenth;
            events.push_back(
                {.pitch = &chord == previous ? variedPitch(chord.symbol, root, pitchRng) : root,
                 .start = start,
                 .length =
                     next == chordNotes.end() ? length : std::min(length, next->start - start),
                 .velocity = 100});
            previous = &chord;
        }
    }
    return events;
}

std::vector<NoteEvent> patternEvents(const Idea& idea, BassPattern pattern, std::uint64_t bassSeed)
{
    switch (pattern)
    {
    case BassPattern::riff:
        return riffEvents(idea, bassSeed);
    case BassPattern::callResponse:
        return callResponseEvents(idea, bassSeed);
    default:
        return slotEvents(idea, pattern, bassSeed);
    }
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
    return allowed.at(rng.pickWeighted(weights));
}

std::vector<NoteEvent> bassEvents(const Idea& idea, BassPattern pattern, std::uint64_t bassSeed)
{
    std::vector<NoteEvent> events = patternEvents(idea, pattern, bassSeed);
    if (idea.settings.energy > 50 && pattern != BassPattern::sustained &&
        pattern != BassPattern::pedal)
    {
        addApproachNotes(idea, bassSeed, events);
    }
    clampMonophonic(events, idea.settings.bars * kTicksPerBar);
    return events;
}
} // namespace md::detail

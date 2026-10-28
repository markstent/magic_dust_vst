#include "md/validate.hpp"

#include "internal.hpp"
#include "md/names.hpp"
#include "md/ranges.hpp"
#include "md/theory.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <iterator>
#include <span>
#include <string>
#include <string_view>

namespace md
{
namespace
{
using Lines = std::vector<std::string>;

bool contains(const std::vector<int>& values, int value)
{
    return std::ranges::find(values, value) != values.end();
}

std::string describe(std::string_view lane, const NoteEvent& e)
{
    return std::string(lane) + " note " + std::to_string(e.pitch) + " at tick " +
           std::to_string(e.start);
}

void checkVoicings(const Idea& idea, Lines& lines)
{
    for (std::size_t i = 0; i < idea.progression.size(); ++i)
    {
        const Voicing& v = idea.progression.at(i).voicing;
        if (idea.progression.at(i).bars < 1)
        {
            lines.push_back("chord " + std::to_string(i + 1) + " has fewer than 1 bar");
        }
        const std::string name = "chord " + std::to_string(i + 1) + " voicing";
        if (v.empty() || v.front() < kChordLow || v.back() > kChordHigh)
        {
            lines.push_back(name + " is outside MIDI 48 to 79");
        }
        else if (v.back() < kChordTopLow || v.back() > kChordTopHigh)
        {
            lines.push_back(name + " has its top note outside MIDI 62 to 77");
        }
        else if (detail::isMuddy(v))
        {
            lines.push_back(name + " is muddy");
        }
        const Voicing& previous = idea.progression.at(i == 0 ? 0 : i - 1).voicing;
        if (i == 0 || idea.parallel || v.empty() || previous.empty())
        {
            continue;
        }
        detail::forEachVoicePair(v, previous,
                                 [&](std::size_t voice, int move)
                                 {
                                     if (move > kMaxVoiceMove)
                                     {
                                         lines.push_back(name + " moves voice " +
                                                         std::to_string(voice + 1) +
                                                         " more than 7 semitones");
                                     }
                                 });
    }
}

// Start ticks of the bass notes that lead into a chord change: the note on the last swung 16th
// before it, within 2 semitones of the first bass note at or after it. The idea's end counts as a
// change into bar 1.
std::vector<int> approachStarts(const Idea& idea)
{
    auto bass = idea.lanes.at(static_cast<std::size_t>(LaneId::bass)).events;
    std::ranges::stable_sort(bass, {}, &NoteEvent::start);
    const int end = idea.settings.bars * kTicksPerBar;
    const int swing = swingPermille(idea.settings.groove);
    std::vector<int> changes;
    for (std::size_t i = 1; i < idea.progression.size(); ++i)
    {
        changes.push_back(idea.progression.at(i).startBar * kTicksPerBar);
    }
    changes.push_back(end);
    std::vector<int> starts;
    for (const int change : changes)
    {
        const int tick = detail::swung(change - kTicksPerSixteenth, swing);
        const auto note = std::ranges::find(bass, tick, &NoteEvent::start);
        const auto target = change == end ? bass.begin()
                                          : std::ranges::find_if(bass, [change](const NoteEvent& e)
                                                                 { return e.start >= change; });
        if (note != bass.end() && target != bass.end() &&
            std::abs(note->pitch - target->pitch) <= 2)
        {
            starts.push_back(tick);
        }
    }
    return starts;
}

void checkLane(const Idea& idea, LaneId id, std::string_view name, int low, int high, Lines& lines,
               const std::vector<int>& approaches = {})
{
    const auto& events = idea.lanes.at(static_cast<std::size_t>(id)).events;
    const int end = idea.settings.bars * kTicksPerBar;
    auto scale = scalePitchClasses(idea.key);
    if (id == LaneId::chords && idea.borrowedFrom)
    {
        const auto extra = scalePitchClasses({.root = idea.key.root, .mode = *idea.borrowedFrom});
        scale.insert(scale.end(), extra.begin(), extra.end());
    }
    for (const NoteEvent& e : events)
    {
        const PlacedChord* chord = detail::findChord(idea.progression, e.start);
        if (e.pitch < low || e.pitch > high)
        {
            lines.push_back(describe(name, e) + " is outside MIDI " + std::to_string(low) + " to " +
                            std::to_string(high));
        }
        else if (id == LaneId::chords && chord != nullptr &&
                 std::ranges::find(chord->voicing, e.pitch) == chord->voicing.end())
        {
            lines.push_back(describe(name, e) + " is not in the voicing of its chord");
        }
        if (e.start < 0)
        {
            lines.push_back(describe(name, e) + " starts before the idea");
        }
        if (e.length < 1)
        {
            lines.push_back(describe(name, e) + " has no length");
        }
        if (e.start + e.length > end)
        {
            lines.push_back(describe(name, e) + " ends after the idea");
        }
        const int pc = e.pitch % 12;
        bool inKey = contains(scale, pc);
        if (id != LaneId::chords && !inKey && chord != nullptr)
        {
            inKey = contains(chordPitchClasses(idea.key, chord->symbol), pc) ||
                    (idea.parallel && contains(detail::soundingPitchClasses(idea, *chord), pc));
        }
        if (!inKey && (id != LaneId::chords || !idea.parallel) && !contains(approaches, e.start))
        {
            lines.push_back(describe(name, e) + " is out of key");
        }
    }
}

// The melody rules that need its neighbours or its chord.
void checkMelody(const Idea& idea, Lines& lines)
{
    const auto& events = idea.lanes.at(static_cast<std::size_t>(LaneId::melody)).events;
    for (std::size_t i = 0; i < events.size(); ++i)
    {
        const NoteEvent& e = events.at(i);
        if (i > 0)
        {
            const int leap = std::abs(e.pitch - events.at(i - 1).pitch);
            if (leap > kMaxMelodyLeap && leap != 12)
            {
                lines.push_back(describe("melody", e) + " leaps " + std::to_string(leap) +
                                " semitones");
            }
        }
        const PlacedChord* chord = detail::findChord(idea.progression, e.start);
        if (e.start % kTicksPerQuarter == 0 && chord != nullptr &&
            !contains(detail::allowedPitchClasses(detail::soundingPitchClasses(idea, *chord)),
                      e.pitch % 12))
        {
            lines.push_back(describe("melody", e) + " clashes with its chord on a strong beat");
        }
    }
}

void checkProgressionRules(const Idea& idea, Lines& lines)
{
    int expectedBar = 0;
    for (std::size_t i = 0; i < idea.progression.size(); ++i)
    {
        if (idea.progression.at(i).startBar != expectedBar)
        {
            lines.push_back("chord " + std::to_string(i + 1) + " starts at bar " +
                            std::to_string(idea.progression.at(i).startBar) + ", expected bar " +
                            std::to_string(expectedBar));
            break;
        }
        expectedBar += idea.progression.at(i).bars;
    }
    if (idea.parallel)
    {
        return;
    }
    std::vector<ChordSymbol> symbols;
    std::ranges::transform(idea.progression, std::back_inserter(symbols), &PlacedChord::symbol);
    const ProgressionFit fit = checkProgression(idea.key.mode, symbols);
    if (!fit.ok)
    {
        lines.emplace_back(
            "progression is not in the mode and borrows from no single related mode");
    }
    else if (fit.borrowedFrom != idea.borrowedFrom)
    {
        lines.emplace_back("borrowedFrom does not match the progression");
    }
}

// The bass keeps off the kick and away from a semitone clash with the chord sounding at its start.
void checkBassRules(const Idea& idea, const std::vector<int>& approaches, Lines& lines)
{
    const Lane& lane = idea.lanes.at(static_cast<std::size_t>(LaneId::bass));
    const bool sustained = lane.pattern == bassPatternName(BassPattern::sustained);
    const bool pedal = lane.pattern == bassPatternName(BassPattern::pedal);
    for (const NoteEvent& e : lane.events)
    {
        const int step = (e.start % kTicksPerBar) / kTicksPerSixteenth;
        const bool allowed = sustained && step == 0;
        if (step % 4 == 0 && !allowed)
        {
            lines.push_back(describe("bass", e) + " starts on a downbeat");
        }
        const auto sounding = detail::soundingPcsAt(idea, e.start);
        const int pc = e.pitch % 12;
        const bool clash =
            !contains(sounding, pc) &&
            std::ranges::any_of(
                sounding, [pc](int tone)
                { return (tone - pc + 12) % 12 == 1 || (pc - tone + 12) % 12 == 1; });
        if (clash && !pedal && !contains(approaches, e.start))
        {
            lines.push_back(describe("bass", e) + " clashes with the sounding chord");
        }
    }
}

void checkBassOverlap(const Idea& idea, Lines& lines)
{
    auto bass = idea.lanes.at(static_cast<std::size_t>(LaneId::bass)).events;
    std::ranges::stable_sort(bass, {}, &NoteEvent::start);
    for (std::size_t i = 1; i < bass.size(); ++i)
    {
        if (bass.at(i - 1).start + bass.at(i - 1).length > bass.at(i).start)
        {
            lines.push_back(describe("bass", bass.at(i)) + " overlaps the previous note");
        }
    }
}
} // namespace

namespace detail
{
const PlacedChord* findChord(std::span<const PlacedChord> progression, int tick)
{
    const auto it =
        std::ranges::find_if(progression,
                             [tick](const PlacedChord& chord)
                             {
                                 return tick >= chord.startBar * kTicksPerBar &&
                                        tick < (chord.startBar + chord.bars) * kTicksPerBar;
                             });
    return it == progression.end() ? nullptr : &*it;
}

bool isMuddy(const Voicing& voicing)
{
    for (std::size_t i = 1; i < voicing.size(); ++i)
    {
        if (voicing.at(i - 1) < kMudBelow && voicing.at(i) - voicing.at(i - 1) < kMudMinInterval)
        {
            return true;
        }
    }
    return false;
}
} // namespace detail

std::vector<std::string> validate(const Idea& idea)
{
    Lines lines;
    if (idea.settings.bars != 4 && idea.settings.bars != 8 && idea.settings.bars != 16)
    {
        return {"settings.bars is " + std::to_string(idea.settings.bars) + ", expected 4, 8 or 16"};
    }
    int bars = 0;
    for (const PlacedChord& chord : idea.progression)
    {
        bars += chord.bars;
    }
    if (bars != idea.settings.bars)
    {
        lines.push_back("progression covers " + std::to_string(bars) + " bars, expected " +
                        std::to_string(idea.settings.bars));
    }
    checkProgressionRules(idea, lines);
    checkVoicings(idea, lines);
    checkLane(idea, LaneId::chords, "chords", kChordLow, kChordHigh, lines);
    const std::vector<int> approaches = approachStarts(idea);
    checkLane(idea, LaneId::bass, "bass", kBassLow, kBassHigh, lines, approaches);
    checkBassRules(idea, approaches, lines);
    checkLane(idea, LaneId::melody, "melody", kMelodyLow, kMelodyHigh, lines);
    checkBassOverlap(idea, lines);
    checkMelody(idea, lines);
    return lines;
}
} // namespace md

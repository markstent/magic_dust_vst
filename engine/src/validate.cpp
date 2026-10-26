#include "md/validate.hpp"

#include "internal.hpp"
#include "md/ranges.hpp"
#include "md/theory.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
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
        if (i == 0 || idea.parallel)
        {
            continue;
        }
        const Voicing& previous = idea.progression.at(i - 1).voicing;
        for (std::size_t voice = 0; voice < std::min(v.size(), previous.size()); ++voice)
        {
            if (std::abs(v.at(voice) - previous.at(voice)) > kMaxVoiceMove)
            {
                lines.push_back(name + " moves voice " + std::to_string(voice + 1) +
                                " more than 7 semitones");
            }
        }
    }
}

void checkLane(const Idea& idea, LaneId id, std::string_view name, int low, int high, Lines& lines)
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
        if (e.pitch < low || e.pitch > high)
        {
            lines.push_back(describe(name, e) + " is outside MIDI " + std::to_string(low) + " to " +
                            std::to_string(high));
        }
        if (e.start + e.length > end)
        {
            lines.push_back(describe(name, e) + " ends after the idea");
        }
        const int pc = e.pitch % 12;
        bool inKey = contains(scale, pc);
        if (id != LaneId::chords && !inKey)
        {
            const PlacedChord* chord = detail::findChord(idea.progression, e.start);
            inKey = chord != nullptr && contains(chordPitchClasses(idea.key, chord->symbol), pc);
        }
        if (!inKey && (id != LaneId::chords || !idea.parallel))
        {
            lines.push_back(describe(name, e) + " is out of key");
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
    checkVoicings(idea, lines);
    checkLane(idea, LaneId::chords, "chords", kChordLow, kChordHigh, lines);
    checkLane(idea, LaneId::bass, "bass", kBassLow, kBassHigh, lines);
    checkLane(idea, LaneId::melody, "melody", kMelodyLow, kMelodyHigh, lines);
    checkBassOverlap(idea, lines);
    return lines;
}
} // namespace md

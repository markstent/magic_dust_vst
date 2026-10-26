#include "internal.hpp"

#include "md/ranges.hpp"
#include "md/theory.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdlib>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <tuple>

namespace md::detail
{
namespace
{
constexpr int kFirstChordTop = 70;
constexpr int kLeapPenalty = 1'000'000;
constexpr int kFifth = 7;

struct TopRange
{
    int low;
    int high;
};

bool fits(const Voicing& v, TopRange top)
{
    return v.front() >= kChordLow && v.back() <= kChordHigh && v.back() >= top.low &&
           v.back() <= top.high && !isMuddy(v);
}

// Every rotation of the close-position stack in every octave that fits the ranges. A triad gets
// its root doubled an octave up; when wide, any of its tones may be doubled an octave up or down,
// which rescues chords whose stacked voicings are all muddy or cannot follow the previous chord.
std::vector<Voicing> candidates(const Key& key, const ChordSymbol& chord, TopRange top, bool wide)
{
    auto intervals = chordIntervals(chord.quality);
    for (const int drop : {kFifth, 0})
    {
        if (intervals.size() > 4)
        {
            std::erase(intervals, drop);
        }
    }
    // Folded into one octave above the root, so a 9th sits between the root and the 3rd.
    std::ranges::transform(intervals, intervals.begin(),
                           [](int interval) { return interval % 12; });
    std::ranges::sort(intervals);
    const int root = chordRootPc(key, chord);
    std::vector<int> pcs;
    std::ranges::transform(intervals, std::back_inserter(pcs),
                           [root](int interval) { return (root + interval) % 12; });
    std::vector<Voicing> found;
    for (std::size_t rotation = 0; rotation < pcs.size(); ++rotation)
    {
        for (int lowest = kChordLow; lowest <= kChordHigh; ++lowest)
        {
            if (lowest % 12 != pcs.at(rotation))
            {
                continue;
            }
            Voicing v{lowest};
            for (std::size_t i = 1; i < pcs.size(); ++i)
            {
                int next = v.back() + 1;
                while (next % 12 != pcs.at((rotation + i) % pcs.size()))
                {
                    ++next;
                }
                v.push_back(next);
            }
            std::vector<Voicing> stacks{v};
            if (pcs.size() == 3)
            {
                stacks.clear();
                const int rootNote =
                    *std::ranges::find(v, root, [](int pitch) { return pitch % 12; });
                std::vector<int> doubled{rootNote + 12};
                for (const int note : wide ? v : Voicing{})
                {
                    doubled.insert(doubled.end(), {note - 12, note + 12});
                }
                for (const int note : doubled)
                {
                    stacks.push_back(v);
                    stacks.back().push_back(note);
                }
            }
            for (Voicing& stacked : stacks)
            {
                std::ranges::sort(stacked);
                if (fits(stacked, top))
                {
                    found.push_back(stacked);
                }
            }
        }
    }
    return found;
}

int cost(const Voicing& v, const Voicing* previous)
{
    if (previous == nullptr)
    {
        return std::abs(v.back() - kFirstChordTop);
    }
    int total = std::abs(v.back() - previous->back());
    for (std::size_t i = 0; i < v.size(); ++i)
    {
        const int move = std::abs(v.at(i) - previous->at(i));
        total += (2 * move) + (move > kMaxVoiceMove ? kLeapPenalty : 0);
    }
    return total;
}

// Lowest cost, then lowest top note, then lowest notes.
Voicing cheapest(const std::vector<Voicing>& options, const Voicing* previous)
{
    return *std::ranges::min_element(options,
                                     [previous](const Voicing& a, const Voicing& b)
                                     {
                                         return std::tuple(cost(a, previous), a.back(), a) <
                                                std::tuple(cost(b, previous), b.back(), b);
                                     });
}

// The first voicing moved by the root movement, in the octave that moves least and fits.
std::optional<Voicing> parallelVoicing(const Voicing& shape, int rootMove, TopRange top)
{
    const int move = ((rootMove + 6) % 12) - 6;
    std::optional<Voicing> best;
    int bestDistance = 0;
    for (int octave = -3; octave <= 3; ++octave)
    {
        const int shift = move + (12 * octave);
        Voicing v = shape;
        for (int& pitch : v)
        {
            pitch += shift;
        }
        const bool better = !best || std::abs(shift) < bestDistance ||
                            (std::abs(shift) == bestDistance && shift < 0);
        if (better && fits(v, top))
        {
            best = v;
            bestDistance = std::abs(shift);
        }
    }
    return best;
}
} // namespace

std::vector<PlacedChord> placeProgression(const Progression& progression, int bars)
{
    std::vector<PlacedChord> placed;
    const std::size_t count = progression.chords.size();
    for (std::size_t i = 0; placed.empty() || placed.back().startBar + placed.back().bars < bars;
         ++i)
    {
        const int startBar = placed.empty() ? 0 : placed.back().startBar + placed.back().bars;
        const PlacedChord chord{
            .symbol = progression.chords.at(i % count),
            .feel = "",
            .startBar = startBar,
            .bars = std::min(progression.barsPerChord.at(i % count), bars - startBar),
            .voicing = {},
        };
        placed.push_back(chord);
    }
    for (std::size_t slot = 0; slot < placed.size(); ++slot)
    {
        placed.at(slot).feel = feelWord(placed.at(slot).symbol, slot, placed.size());
    }
    return placed;
}

void voiceChords(const StyleEntry& entry, const Key& key, std::vector<PlacedChord>& chords)
{
    const TopRange top{
        .low = std::max(kChordTopLow, entry.topNoteLow),
        .high = std::min(kChordTopHigh, entry.topNoteHigh),
    };
    for (std::size_t i = 0; i < chords.size(); ++i)
    {
        std::optional<Voicing> chosen;
        if (entry.parallel && i > 0)
        {
            const int rootMove =
                chordRootPc(key, chords.at(i).symbol) - chordRootPc(key, chords.front().symbol);
            chosen = parallelVoicing(chords.front().voicing, (rootMove + 12) % 12, top);
        }
        if (!chosen)
        {
            const Voicing* previous = i > 0 ? &chords.at(i - 1).voicing : nullptr;
            const ChordSymbol& symbol = chords.at(i).symbol;
            auto options = candidates(key, symbol, top, false);
            const bool leaps =
                !options.empty() && cost(cheapest(options, previous), previous) >= kLeapPenalty;
            if (options.empty() || leaps)
            {
                const auto wider = candidates(key, symbol, top, true);
                options = wider.empty() ? options : wider;
            }
            if (options.empty())
            {
                throw std::logic_error("no voicing fits " + chordName(key, chords.at(i).symbol) +
                                       " with a top note in " + std::to_string(top.low) + " to " +
                                       std::to_string(top.high));
            }
            chosen = cheapest(options, previous);
        }
        chords.at(i).voicing = *chosen;
    }
}

std::vector<NoteEvent> chordEvents(const std::vector<PlacedChord>& chords, const RhythmCell& cell,
                                   const Settings& settings)
{
    const int swing = swingPermille(settings.groove);
    const int ideaEnd = settings.bars * kTicksPerBar;
    struct Onset
    {
        int start;
        int length;
    };
    auto hits = cell.hits;
    std::ranges::stable_sort(hits, {}, &Hit::step);
    const int cellTicks = cell.bars * kTicksPerBar;
    std::vector<NoteEvent> events;
    for (const PlacedChord& chord : chords)
    {
        const int chordStart = chord.startBar * kTicksPerBar;
        const int chordEnd = std::min(chordStart + (chord.bars * kTicksPerBar), ideaEnd);
        std::vector<Onset> onsets;
        for (int tile = chordStart; tile < chordEnd; tile += cellTicks)
        {
            for (const Hit& hit : hits)
            {
                const int nominal = tile + (hit.step * kTicksPerSixteenth);
                if (nominal < chordEnd)
                {
                    const Onset onset{
                        .start = swung(nominal, swing),
                        .length = hit.length * kTicksPerSixteenth,
                    };
                    onsets.push_back(onset);
                }
            }
        }
        for (std::size_t i = 0; i < onsets.size(); ++i)
        {
            const int limit = i + 1 < onsets.size() ? onsets.at(i + 1).start : chordEnd;
            const int start = onsets.at(i).start;
            const int length = std::min(onsets.at(i).length, std::min(limit, chordEnd) - start);
            if (length < 1)
            {
                continue;
            }
            for (const int pitch : chord.voicing)
            {
                const NoteEvent note{
                    .pitch = pitch,
                    .start = start,
                    .length = length,
                    .velocity = 0,
                };
                events.push_back(note);
            }
        }
    }
    return events;
}
} // namespace md::detail

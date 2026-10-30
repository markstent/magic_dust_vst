#include "internal.hpp"

#include "md/pcg32.hpp"
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
constexpr int kMaxVariedHit = 2 * kTicksPerSixteenth;
constexpr std::array kLengthPercents = std::to_array({70, 85, 100});
constexpr int kMinGhostGap = 2;

struct TopRange
{
    int low;
    int high;
};

TopRange topRangeOf(const StyleEntry& entry)
{
    return {
        .low = std::max(kChordTopLow, entry.topNoteLow),
        .high = std::min(kChordTopHigh, entry.topNoteHigh),
    };
}

bool fits(const Voicing& v, TopRange top)
{
    return v.front() >= kChordLow && v.back() <= kChordHigh && v.back() >= top.low &&
           v.back() <= top.high && !isMuddy(v);
}

bool hasDuplicate(const Voicing& sorted)
{
    return std::ranges::adjacent_find(sorted) != sorted.end();
}

// The voicings of one close-position stack in the entry's shape that fit the ranges.
std::vector<Voicing> shaped(const Voicing& stack, VoicingStyle style, int root, TopRange top)
{
    std::vector<Voicing> found;
    Voicing v = stack;
    if (style == VoicingStyle::drop2 && stack.size() == 4)
    {
        v.at(2) -= 12;
        std::ranges::sort(v);
    }
    else if (style == VoicingStyle::organStab)
    {
        v.push_back(stack.front() + 12);
        std::ranges::sort(v);
        if (hasDuplicate(v) || !fits(v, top))
        {
            v = stack;
        }
    }
    else if (style == VoicingStyle::pianoSpread)
    {
        // Chords of five or more tones have no root in their stack; the bass note is added to it.
        const auto rootNote = std::ranges::find(v, root, [](int pitch) { return pitch % 12; });
        if (rootNote != v.end())
        {
            v.erase(rootNote);
        }
        for (int low = kChordLow; low <= v.front() - kFifth; ++low)
        {
            if (low % 12 == root)
            {
                Voicing spread = v;
                spread.insert(spread.begin(), low);
                if (fits(spread, top))
                {
                    found.push_back(spread);
                }
            }
        }
        return found;
    }
    if (!hasDuplicate(v) && fits(v, top))
    {
        found.push_back(v);
    }
    return found;
}

// Every rotation of the close-position stack in every octave, shaped and fitted to the ranges. A
// triad gets its root doubled an octave up; when wide, any of its tones may be doubled an octave up
// or down, which rescues chords whose stacked voicings are all muddy or cannot follow the previous
// chord.
std::vector<Voicing> candidates(const Key& key, const ChordSymbol& chord, TopRange top, bool wide,
                                VoicingStyle style)
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
                const auto shapes = shaped(stacked, style, root, top);
                found.insert(found.end(), shapes.begin(), shapes.end());
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
    forEachVoicePair(v, *previous, [&total](std::size_t, int move)
                     { total += (2 * move) + (move > kMaxVoiceMove ? kLeapPenalty : 0); });
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

// The cheapest voicing in the shape, or none when no candidate fits or, if strict, when every one
// leaps from the previous chord.
std::optional<Voicing> choose(const Key& key, const ChordSymbol& symbol, TopRange top,
                              const Voicing* previous, VoicingStyle style, bool strict)
{
    auto options = candidates(key, symbol, top, false, style);
    const auto leaps = [&] { return cost(cheapest(options, previous), previous) >= kLeapPenalty; };
    if (options.empty() || leaps())
    {
        const auto wider = candidates(key, symbol, top, true, style);
        options = wider.empty() ? options : wider;
    }
    if (options.empty() || (strict && leaps()))
    {
        return std::nullopt;
    }
    return cheapest(options, previous);
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

// Voices every chord in the shape, with close position for a chord the shape cannot voice. A strict
// pass gives up (false) when a chord can only leap; the plain close pass throws instead.
bool voiceAll(const StyleEntry& entry, VoicingStyle style, bool strict, const Key& key,
              std::vector<PlacedChord>& chords)
{
    const TopRange top = topRangeOf(entry);
    for (std::size_t i = 0; i < chords.size(); ++i)
    {
        std::optional<Voicing> chosen;
        if (entry.parallel && i > 0)
        {
            const int rootMove =
                chordRootPc(key, chords.at(i).symbol) - chordRootPc(key, chords.front().symbol);
            chosen = parallelVoicing(chords.front().voicing, (rootMove + 12) % 12, top);
        }
        const Voicing* previous = i > 0 ? &chords.at(i - 1).voicing : nullptr;
        const ChordSymbol& symbol = chords.at(i).symbol;
        if (!chosen)
        {
            chosen = choose(key, symbol, top, previous, style, strict);
        }
        if (!chosen && style != VoicingStyle::close)
        {
            chosen = choose(key, symbol, top, previous, VoicingStyle::close, strict);
        }
        if (!chosen && strict)
        {
            return false;
        }
        if (!chosen)
        {
            throw std::logic_error("no voicing fits " + chordName(key, symbol) +
                                   " with a top note in " + std::to_string(top.low) + " to " +
                                   std::to_string(top.high));
        }
        chords.at(i).voicing = *chosen;
    }
    return true;
}

bool movesWithin(const Voicing& a, const Voicing& b)
{
    bool within = true;
    forEachVoicePair(a, b, [&within](std::size_t, int move)
                     { within = within && move <= kMaxVoiceMove; });
    return within;
}

// True when the span covers bar 3 or 4 of a 4-bar group (0-based bars 2 and 3 of each 4). The
// voicing is one per span, so a span that reaches those bars is inverted whole.
bool coversLateBar(const PlacedChord& chord)
{
    return chord.bars >= 3 || (chord.startBar % 4) + chord.bars > 2;
}

// The voicing with its lowest note moved up an octave, kept sorted.
Voicing nextInversion(const Voicing& v)
{
    Voicing inverted(v.begin() + 1, v.end());
    inverted.push_back(v.front() + 12);
    std::ranges::sort(inverted);
    return inverted;
}

// At most one ghost a bar and bars / 2 in all, in bars that are not turnarounds. Each is the bar's
// voicing on an odd 16th at least kMinGhostGap steps from every hit, clear of any held hit.
void addGhosts(std::vector<NoteEvent>& events, const std::vector<PlacedChord>& chords,
               const Settings& settings, std::uint64_t chordSeed)
{
    Pcg32 rng(chordSeed, kChordGhost);
    const int swing = swingPermille(settings.groove);
    std::vector<int> bars;
    for (int bar = 0; bar < settings.bars; ++bar)
    {
        if (!isTurnaroundBar(settings.bars, bar) && !isBreathBar(settings.bars, bar))
        {
            bars.push_back(bar);
        }
    }
    std::vector<NoteEvent> ghosts;
    // A bar with no clear odd step is skipped, so the idea may hold fewer ghosts than the cap.
    for (int n = 0; n < settings.bars / 2 && !bars.empty(); ++n)
    {
        const auto pick =
            static_cast<std::ptrdiff_t>(rng.below(static_cast<std::uint32_t>(bars.size())));
        const int bar = bars.at(static_cast<std::size_t>(pick));
        bars.erase(bars.begin() + pick);
        std::vector<int> starts;
        for (int step = 1; step < kStepsPerBar; step += 2)
        {
            const int start = swung((bar * kTicksPerBar) + (step * kTicksPerSixteenth), swing);
            const bool clear = std::ranges::all_of(
                events,
                [&](const NoteEvent& e)
                {
                    return std::abs(stepOf(e.start) - ((bar * kStepsPerBar) + step)) >=
                               kMinGhostGap &&
                           (e.start >= start || e.start + e.length <= start);
                });
            if (clear)
            {
                starts.push_back(start);
            }
        }
        if (starts.empty())
        {
            continue;
        }
        const int start = starts.at(rng.below(static_cast<std::uint32_t>(starts.size())));
        for (const int pitch : chordAt(chords, bar * kTicksPerBar).voicing)
        {
            ghosts.push_back(
                {.pitch = pitch,
                 .start = start,
                 .length = std::min(kTicksPerSixteenth, ((bar + 1) * kTicksPerBar) - start),
                 .velocity = kGhostVelocity});
        }
    }
    events.insert(events.end(), ghosts.begin(), ghosts.end());
    std::ranges::stable_sort(events, {}, &NoteEvent::start);
}

std::optional<Mode> borrowedFor(const Idea& idea)
{
    std::vector<ChordSymbol> symbols;
    std::ranges::transform(idea.progression, std::back_inserter(symbols), &PlacedChord::symbol);
    return checkProgression(idea.key.mode, symbols).borrowedFrom;
}
} // namespace

void invertVoicings(const StyleEntry& entry, std::vector<PlacedChord>& chords,
                    std::uint64_t chordSeed)
{
    if (entry.parallel)
    {
        return;
    }
    Pcg32 rng(chordSeed, kChordInversion);
    const TopRange top = topRangeOf(entry);
    for (std::size_t i = 1; i < chords.size(); ++i)
    {
        if (chords.at(i).symbol != chords.at(i - 1).symbol || !coversLateBar(chords.at(i)) ||
            rng.below(2) != 0)
        {
            continue;
        }
        const Voicing inverted = nextInversion(chords.at(i).voicing);
        if (!hasDuplicate(inverted) && fits(inverted, top) &&
            movesWithin(inverted, chords.at(i - 1).voicing) &&
            (i + 1 == chords.size() || movesWithin(inverted, chords.at(i + 1).voicing)))
        {
            chords.at(i).voicing = inverted;
        }
    }
}

std::vector<PlacedChord> placeProgression(const Progression& progression, int bars)
{
    std::vector<PlacedChord> placed;
    const std::size_t count = progression.chords.size();
    if (count == 0 || progression.barsPerChord.size() != count ||
        std::ranges::any_of(progression.barsPerChord, [](int bars) { return bars < 1; }))
    {
        throw std::logic_error("progression needs chords and at least one bar for each");
    }
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
    const bool hasShape = entry.voicingStyle != VoicingStyle::close;
    if (!hasShape || !voiceAll(entry, entry.voicingStyle, true, key, chords))
    {
        voiceAll(entry, VoicingStyle::close, false, key, chords);
    }
}

std::vector<Mode> fittingModes(const StyleEntry& entry, const Progression& progression)
{
    std::vector<Mode> fitting;
    for (const Mode mode : entry.modes)
    {
        if (entry.parallel || checkProgression(mode, progression.chords).ok)
        {
            fitting.push_back(mode);
        }
    }
    std::ranges::sort(fitting);
    return fitting;
}

void placeChords(Idea& idea, const Progression& progression)
{
    idea.progression = placeProgression(progression, idea.settings.bars);
    idea.borrowedFrom = borrowedFor(idea);
    for (PlacedChord& chord : idea.progression)
    {
        chord.symbol =
            applyFlavour(idea.key, idea.borrowedFrom, chord.symbol, idea.settings.flavour);
    }
    idea.borrowedFrom = borrowedFor(idea);
}

std::vector<NoteEvent> chordEvents(const std::vector<PlacedChord>& chords, const RhythmCell& cell,
                                   const Settings& settings, const StyleEntry& entry,
                                   std::uint64_t chordSeed)
{
    const int swing = swingPermille(settings.groove);
    const int ideaEnd = settings.bars * kTicksPerBar;
    struct Onset
    {
        int start;
        int length;
    };
    auto sortedHits = [](std::vector<Hit> cellHits)
    {
        std::ranges::stable_sort(cellHits, {}, &Hit::step);
        return cellHits;
    };
    const auto hits = sortedHits(cell.hits);
    // Bar 1 of each other cell of the entry that has a hit there.
    std::vector<std::vector<Hit>> turnarounds;
    for (const RhythmCell& other : entry.chordRhythms)
    {
        auto first = sortedHits(other.hits);
        std::erase_if(first, [](const Hit& hit) { return hit.step >= kStepsPerBar; });
        if (other.id != cell.id && !first.empty())
        {
            turnarounds.push_back(std::move(first));
        }
    }
    Pcg32 turnaroundRng(chordSeed, kChordTurnaround);
    const bool turns = settings.energy >= kMidEnergy && !turnarounds.empty();
    const bool high = settings.energy > kHighEnergy;
    Pcg32 lengthRng(chordSeed, kChordLength);
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
        for (int bar = chord.startBar; bar * kTicksPerBar < chordEnd; ++bar)
        {
            const auto inBar = [bar](const Onset& onset)
            { return onset.start / kTicksPerBar == bar; };
            if (turns && isTurnaroundBar(settings.bars, bar))
            {
                const auto& turn = turnarounds.at(
                    turnaroundRng.below(static_cast<std::uint32_t>(turnarounds.size())));
                std::erase_if(onsets, inBar);
                for (const Hit& hit : turn)
                {
                    onsets.push_back(
                        {.start =
                             swung((bar * kTicksPerBar) + (hit.step * kTicksPerSixteenth), swing),
                         .length = hit.length * kTicksPerSixteenth});
                }
                std::ranges::stable_sort(onsets, {}, &Onset::start);
            }
            else if (high && isBreathBar(settings.bars, bar) &&
                     std::ranges::count_if(onsets, inBar) > 1)
            {
                onsets.erase(std::ranges::find_if(onsets.rbegin(), onsets.rend(), inBar).base() -
                             1);
            }
        }
        for (std::size_t i = 0; i < onsets.size(); ++i)
        {
            const int limit = i + 1 < onsets.size() ? onsets.at(i + 1).start : chordEnd;
            const int start = onsets.at(i).start;
            int length = std::min(onsets.at(i).length, std::min(limit, chordEnd) - start);
            if (length < 1)
            {
                continue;
            }
            if (high && onsets.at(i).length <= kMaxVariedHit)
            {
                const int percent = kLengthPercents.at(lengthRng.below(kLengthPercents.size()));
                length = std::min(length,
                                  std::max(kMinChordLength, onsets.at(i).length * percent / 100));
            }
            for (const int pitch : chord.voicing)
            {
                const NoteEvent note{
                    .pitch = pitch,
                    .start = start,
                    .length = length,
                    .velocity = 100,
                };
                events.push_back(note);
            }
        }
    }
    if (high)
    {
        addGhosts(events, chords, settings, chordSeed);
    }
    return events;
}
} // namespace md::detail

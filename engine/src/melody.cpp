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
#include <span>
#include <stdexcept>
#include <tuple>

namespace md::detail
{
namespace
{
constexpr int kSpan = kMelodyHigh - kMelodyLow + 1;
constexpr int kBlockBars = 2;
constexpr int kBlockSteps = kBlockBars * kStepsPerBar;
constexpr int kCentre = 72;
constexpr int kBlocksPerPhrase = 4; // A A A B

// Which pitches of kMelodyLow to kMelodyHigh a note may take.
using Pool = std::array<bool, kSpan>;

struct Onset
{
    int step; // sixteenths from bar 0
    int length;
};

int hitsPerTwoBars(const RhythmCell& cell)
{
    return static_cast<int>(cell.hits.size()) * 2 / cell.bars;
}

// Among the cells (skipping some indices), a random one with the hit count nearest the target.
// Throws std::out_of_range when no cell is left to pick.
std::size_t pickCell(const std::vector<RhythmCell>& cells, int target,
                     std::span<const std::size_t> skip, Pcg32& rng)
{
    int bestDistance = -1;
    std::vector<std::size_t> nearest;
    for (std::size_t i = 0; i < cells.size(); ++i)
    {
        const int distance = std::abs(hitsPerTwoBars(cells.at(i)) - target);
        if (std::ranges::find(skip, i) != skip.end() ||
            (bestDistance >= 0 && distance > bestDistance))
        {
            continue;
        }
        if (bestDistance < 0 || distance < bestDistance)
        {
            bestDistance = distance;
            nearest.clear();
        }
        nearest.push_back(i);
    }
    return nearest.at(rng.below(static_cast<std::uint32_t>(nearest.size())));
}

bool hasAnswer(const StyleEntry& entry, const Idea& idea)
{
    return idea.settings.energy >= kMidEnergy && entry.melodyRhythms.size() > 1;
}

// True for a step in the last block of a 4-bar phrase.
bool inAnswerBlock(int step)
{
    return (step / kBlockSteps) % kBlocksPerPhrase == kBlocksPerPhrase - 1;
}

std::vector<Onset> rhythm(const StyleEntry& entry, const Idea& idea, std::uint64_t seed)
{
    const int target = 3 + ((7 * idea.settings.energy) / 100);
    Pcg32 rngA(seed, kMelodyCellA);
    Pcg32 rngB(seed, kMelodyCellB);
    const std::size_t a = pickCell(entry.melodyRhythms, target, {}, rngA);
    const std::size_t b =
        entry.melodyRhythms.size() > 1 ? pickCell(entry.melodyRhythms, target, {&a, 1}, rngB) : a;
    // From kMidEnergy up the answer phrase, the last 2 bars of each 4-bar phrase in an idea of 8
    // bars or more, uses a cell other than A and B when the entry has a third, else B.
    const std::array<std::size_t, 2> used = {a, b};
    Pcg32 rngAnswer(seed, kMelodyAnswerCell);
    const std::size_t answer =
        !hasAnswer(entry, idea)
            ? b
            : pickCell(entry.melodyRhythms, target,
                       entry.melodyRhythms.size() > 2 ? std::span<const std::size_t>(used)
                                                      : std::span<const std::size_t>(&a, 1),
                       rngAnswer);
    std::vector<Onset> onsets;
    for (int block = 0; block < idea.settings.bars / kBlockBars; ++block)
    {
        const bool lastOfPhrase = block % kBlocksPerPhrase == kBlocksPerPhrase - 1;
        const RhythmCell& cell = entry.melodyRhythms.at(lastOfPhrase ? answer : a);
        auto hits = cell.hits;
        std::ranges::stable_sort(hits, {}, &Hit::step);
        for (int tile = 0; tile < std::max(1, kBlockBars / cell.bars); ++tile)
        {
            for (const Hit& hit : hits)
            {
                const int step =
                    (block * kBlockSteps) + (tile * cell.bars * kStepsPerBar) + hit.step;
                if (step < (block + 1) * kBlockSteps)
                {
                    onsets.push_back({.step = step, .length = hit.length});
                }
            }
        }
    }
    return onsets;
}

Pool poolOf(const std::vector<int>& pitchClasses)
{
    Pool pool{};
    for (int i = 0; i < kSpan; ++i)
    {
        pool.at(static_cast<std::size_t>(i)) =
            std::ranges::find(pitchClasses, (kMelodyLow + i) % 12) != pitchClasses.end();
    }
    return pool;
}

bool any(const Pool& pool)
{
    return std::ranges::any_of(pool, [](bool allowed) { return allowed; });
}

Pool intersect(const Pool& a, const Pool& b)
{
    Pool both{};
    for (std::size_t i = 0; i < both.size(); ++i)
    {
        both.at(i) = a.at(i) && b.at(i);
    }
    return both;
}

bool allows(const Pool& pool, int pitch)
{
    return pitch >= kMelodyLow && pitch <= kMelodyHigh &&
           pool.at(static_cast<std::size_t>(pitch - kMelodyLow));
}

// Keeps only the pitches from which the rest of the line can still follow with leaps of at most
// kMaxMelodyLeap. Without this a sparse pool near the range edge could leave no legal next note.
std::vector<Pool> reachable(std::vector<Pool> pools)
{
    for (std::size_t i = pools.size(); i-- > 1;)
    {
        for (int pitch = kMelodyLow; pitch <= kMelodyHigh; ++pitch)
        {
            const auto index = static_cast<std::size_t>(pitch - kMelodyLow);
            bool follows = false;
            for (int next = std::max(kMelodyLow, pitch - kMaxMelodyLeap);
                 next <= std::min(kMelodyHigh, pitch + kMaxMelodyLeap) && !follows; ++next)
            {
                follows = pools.at(i).at(static_cast<std::size_t>(next - kMelodyLow));
            }
            pools.at(i - 1).at(index) = pools.at(i - 1).at(index) && follows;
        }
    }
    return pools;
}

// The pitch of the pool nearest the target and within a leap of previous (when there is one);
// ties go to the pitch nearer the previous note, then the lower one.
int choose(const Pool& pool, int target, std::optional<int> previous)
{
    std::optional<std::tuple<int, int, int>> bestKey;
    int best = 0;
    for (int pitch = kMelodyLow; pitch <= kMelodyHigh; ++pitch)
    {
        if (!allows(pool, pitch) || (previous && std::abs(pitch - *previous) > kMaxMelodyLeap))
        {
            continue;
        }
        const auto key =
            std::tuple(std::abs(pitch - target), previous ? std::abs(pitch - *previous) : 0, pitch);
        if (!bestKey || key < *bestKey)
        {
            bestKey = key;
            best = pitch;
        }
    }
    if (!bestKey)
    {
        throw std::logic_error("melody has no note within a leap");
    }
    return best;
}

const PlacedChord& homeChord(const Idea& idea)
{
    const auto it = std::ranges::find(idea.progression, 1,
                                      [](const PlacedChord& chord) { return chord.symbol.degree; });
    return it == idea.progression.end() ? idea.progression.front() : *it;
}

// The breath: in each breath bar the last note goes, when its block keeps a note and the leap over
// the gap stays legal.
void dropBreathNote(int bars, std::vector<NoteEvent>& events)
{
    for (int bar = 0; bar < bars; ++bar)
    {
        if (!isBreathBar(bars, bar))
        {
            continue;
        }
        const auto inBar = [bar](const NoteEvent& e) { return e.start / kTicksPerBar == bar; };
        const auto found = std::ranges::find_if(events.rbegin(), events.rend(), inBar);
        const int blockStart = (bar / kBlockBars) * kBlockBars * kTicksPerBar;
        const auto inBlock = [blockStart](const NoteEvent& e)
        { return e.start >= blockStart && e.start < blockStart + (kBlockBars * kTicksPerBar); };
        if (found == events.rend() || std::ranges::count_if(events, inBlock) < 2)
        {
            continue;
        }
        const auto last = std::prev(found.base());
        const bool bridged = last != events.begin() && std::next(last) != events.end();
        if (bridged && !melodyLeapOk(std::prev(last)->pitch, std::next(last)->pitch))
        {
            continue;
        }
        events.erase(last);
    }
}
} // namespace

std::vector<int> pluckPitches(const std::vector<int>& allowedPcs, Pcg32& rng)
{
    const Pool pool = poolOf(allowedPcs);
    std::vector<int> pitches;
    for (int pitch = kMelodyLow; pitch <= kMelodyHigh; ++pitch)
    {
        if (allows(pool, pitch))
        {
            pitches.push_back(pitch);
        }
    }
    if (pitches.empty())
    {
        throw std::logic_error("the home chord has no allowed tone for the pluck");
    }
    std::ranges::sort(
        pitches, [](int a, int b)
        { return std::tuple(std::abs(a - kCentre), a) < std::tuple(std::abs(b - kCentre), b); });
    pitches.resize(std::min<std::size_t>(pitches.size(), 1 + rng.below(3)));
    return pitches;
}

std::vector<NoteEvent> melodyEvents(const StyleEntry& entry, const Idea& idea,
                                    std::uint64_t melodySeed)
{
    const bool pluck = entry.melodyModes.front() == MelodyMode::pluck;
    const std::vector<Onset> onsets = rhythm(entry, idea, melodySeed);
    if (onsets.empty())
    {
        throw std::logic_error("melody rhythm has no onsets");
    }
    const std::size_t last = onsets.size() - 1;
    const std::vector<int> homePcs = soundingPitchClasses(idea, homeChord(idea));
    const Pool home = poolOf(homePcs);
    const Pool scale = poolOf(scalePitchClasses(idea.key));

    std::vector<Pool> allowed;
    std::vector<Pool> pools;
    for (std::size_t i = 0; i < onsets.size(); ++i)
    {
        const int step = onsets.at(i).step;
        const PlacedChord& chord = chordAt(idea.progression, step * kTicksPerSixteenth);
        allowed.push_back(poolOf(allowedPitchClasses(soundingPitchClasses(idea, chord))));
        if (pluck)
        {
            pools.push_back(allowed.back());
        }
        else if (i == last)
        {
            const Pool homeTones = intersect(allowed.back(), home);
            pools.push_back(any(homeTones) ? homeTones : allowed.back());
        }
        else
        {
            pools.push_back(step % 4 == 0 ? allowed.back() : scale);
        }
    }
    if (std::ranges::any_of(pools, [](const Pool& pool) { return !any(pool); }))
    {
        throw std::logic_error("melody has an onset with no pitch to play");
    }
    pools = reachable(std::move(pools));

    std::vector<int> scalePitches;
    for (int pitch = kMelodyLow; pitch <= kMelodyHigh; ++pitch)
    {
        if (allows(scale, pitch))
        {
            scalePitches.push_back(pitch);
        }
    }
    Pcg32 walk(melodySeed, kMelodyWalk);
    Pcg32 answerWalk(melodySeed, kMelodyAnswerWalk);
    const bool answers = hasAnswer(entry, idea);
    Pcg32 pluckRng(melodySeed, kMelodyPluck);
    const std::vector<int> pluckTones =
        pluck ? pluckPitches(allowedPitchClasses(homePcs), pluckRng) : std::vector<int>{};
    // The line starts from the allowed tone of the first chord nearest the centre.
    int target = choose(allowed.front(), kCentre, std::nullopt);

    const int swing = swingPermille(idea.settings.groove);
    std::vector<NoteEvent> events;
    std::optional<int> previous;
    for (std::size_t i = 0; i < onsets.size(); ++i)
    {
        if (pluck)
        {
            target = pluckTones.at(i % pluckTones.size());
        }
        else if (previous)
        {
            const auto nearest =
                std::ranges::min_element(scalePitches,
                                         [&](int a, int b)
                                         {
                                             return std::tuple(std::abs(a - *previous), a) <
                                                    std::tuple(std::abs(b - *previous), b);
                                         });
            const int index = static_cast<int>(nearest - scalePitches.begin());
            Pcg32& rng = answers && inAnswerBlock(onsets.at(i).step) ? answerWalk : walk;
            const int steps = 1 + static_cast<int>(rng.below(2));
            const int moved = index + (rng.below(2) == 0 ? -steps : steps);
            target = scalePitches.at(static_cast<std::size_t>(
                std::clamp(moved, 0, static_cast<int>(scalePitches.size()) - 1)));
        }
        const int pitch = choose(pools.at(i), target, previous);
        previous = pitch;
        const NoteEvent note{
            .pitch = pitch,
            .start = swung(onsets.at(i).step * kTicksPerSixteenth, swing),
            .length = onsets.at(i).length * kTicksPerSixteenth,
            .velocity = 100,
        };
        events.push_back(note);
    }
    clampMonophonic(events, idea.settings.bars * kTicksPerBar);
    if (idea.settings.energy > kHighEnergy)
    {
        dropBreathNote(idea.settings.bars, events);
    }
    return events;
}
} // namespace md::detail

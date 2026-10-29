#include "md/generate.hpp"

#include "internal.hpp"
#include "md/bank.hpp"
#include "md/names.hpp"
#include "md/ranges.hpp"
#include "md/theory.hpp"
#include "md/validate.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <set>
#include <vector>

using namespace md;

namespace
{
const Bank& bank()
{
    static const Bank loaded = loadBundledBank();
    return loaded;
}

constexpr std::array<Style, 6> kStyles = {
    Style::ukFunky, Style::mkDub, Style::soulful, Style::bassline, Style::festival, Style::nyGroove,
};
constexpr int kSeedsPerStyle = 60;
constexpr int kLow = 29;  // the top of the low tier
constexpr int kMid = 30;  // the bottom of the mid tier
constexpr int kTop = 60;  // the top of the mid tier
constexpr int kHigh = 61; // the bottom of the high tier

struct Sample
{
    Idea idea; // generated at energy 0
    const StyleEntry* entry;
    int bars;
};

const std::vector<Sample>& samples()
{
    static const std::vector<Sample> all = []
    {
        constexpr std::array<int, 3> barCounts = {4, 8, 16};
        std::vector<Sample> out;
        for (const Style style : kStyles)
        {
            for (int i = 0; i < kSeedsPerStyle; ++i)
            {
                Settings settings;
                settings.style = style;
                settings.keyRoot = i % 12;
                settings.mood = (i * 13) % 101;
                settings.energy = 0;
                settings.groove = static_cast<Groove>(i % 4);
                settings.bars = barCounts.at(static_cast<std::size_t>(i % 3));
                Idea idea = generate(bank(), settings, static_cast<std::uint32_t>(500 + (i * 37)));
                const auto it = std::ranges::find(bank().entries, idea.entryId, &StyleEntry::id);
                REQUIRE(it != bank().entries.end());
                out.push_back({.idea = std::move(idea), .entry = &*it, .bars = settings.bars});
            }
        }
        return out;
    }();
    return all;
}

Idea atEnergy(const Idea& idea, int energy)
{
    Idea copy = idea;
    copy.settings.energy = energy;
    return copy;
}

std::vector<int> barsOf(std::initializer_list<int> bars) { return bars; }

std::vector<int> turnaroundBars(int bars)
{
    return bars == 16 ? barsOf({7, 15}) : barsOf({bars - 1});
}

std::vector<int> breathBars(int bars)
{
    if (bars == 4)
    {
        return {};
    }
    return bars == 8 ? barsOf({3}) : barsOf({3, 11});
}

bool contains(const std::vector<int>& values, int value)
{
    return std::ranges::find(values, value) != values.end();
}

// The bars the answer phrase covers: the second half of each 4-bar phrase, in ideas of 8 or more.
std::vector<int> answerBars(int bars)
{
    if (bars == 4)
    {
        return {};
    }
    return bars == 8 ? barsOf({6, 7}) : barsOf({6, 7, 14, 15});
}

int barOf(const NoteEvent& e) { return e.start / kTicksPerBar; }

std::vector<NoteEvent> chordsAt(const Sample& s, int energy)
{
    const Idea idea = atEnergy(s.idea, energy);
    const auto cell =
        std::ranges::find(s.entry->chordRhythms, idea.lanes.at(0).pattern, &RhythmCell::id);
    REQUIRE(cell != s.entry->chordRhythms.end());
    return detail::chordEvents(idea.progression, *cell, idea.settings, *s.entry,
                               idea.lanes.at(0).seed);
}

std::set<std::pair<int, int>> pitchStarts(const std::vector<NoteEvent>& events, int bar)
{
    std::set<std::pair<int, int>> out;
    for (const NoteEvent& e : events)
    {
        if (barOf(e) == bar)
        {
            out.insert({e.start, e.pitch});
        }
    }
    return out;
}

std::set<int> starts(const std::vector<NoteEvent>& events, int fromBar, int toBar)
{
    std::set<int> out;
    for (const NoteEvent& e : events)
    {
        if (barOf(e) >= fromBar && barOf(e) <= toBar)
        {
            out.insert(e.start);
        }
    }
    return out;
}

// The starts of a cell's hits over [firstBar, firstBar + bars), tiled, swung as the lanes do.
struct Span
{
    int firstBar;
    int bars;
};

std::set<int> cellStarts(const RhythmCell& cell, Groove groove, Span span)
{
    const auto [firstBar, bars] = span;
    const int swing = swingPermille(groove);
    std::set<int> out;
    for (int tile = 0; tile < bars; tile += cell.bars)
    {
        for (const Hit& hit : cell.hits)
        {
            if ((tile * kStepsPerBar) + hit.step < bars * kStepsPerBar)
            {
                out.insert(detail::swung(
                    ((firstBar + tile) * kTicksPerBar) + (hit.step * kTicksPerSixteenth), swing));
            }
        }
    }
    return out;
}

// The bar-1 starts of every chord cell of the entry except the one named.
bool matchesOtherCell(const Sample& s, const std::string& main, int bar,
                      const std::set<int>& actual)
{
    for (const RhythmCell& cell : s.entry->chordRhythms)
    {
        RhythmCell first = cell;
        first.bars = 1;
        std::erase_if(first.hits, [](const Hit& h) { return h.step >= kStepsPerBar; });
        if (cell.id != main && !first.hits.empty() &&
            cellStarts(first, s.idea.settings.groove, {.firstBar = bar, .bars = 1}) == actual)
        {
            return true;
        }
    }
    return false;
}
} // namespace

TEST_CASE("chords: no turnaround or breath below energy 30, and none changes the other bars",
          "[variety][chords]")
{
    int turned = 0;
    for (const Sample& s : samples())
    {
        const auto low = chordsAt(s, kLow);
        const auto zero = chordsAt(s, 0);
        REQUIRE(low == zero);

        const auto mid = chordsAt(s, kMid);
        for (int bar = 0; bar < s.bars; ++bar)
        {
            if (contains(turnaroundBars(s.bars), bar))
            {
                continue;
            }
            REQUIRE(pitchStarts(mid, bar) == pitchStarts(low, bar));
        }
        for (const int bar : turnaroundBars(s.bars))
        {
            if (pitchStarts(mid, bar) == pitchStarts(low, bar))
            {
                continue;
            }
            ++turned;
            const std::string main = s.idea.lanes.at(0).pattern;
            REQUIRE(matchesOtherCell(s, main, bar, starts(mid, bar, bar)));
        }
    }
    REQUIRE(turned > 0);
}

TEST_CASE("chords: the breath drops the last hit of bar 4 of each 8, above energy 60 only",
          "[variety][chords]")
{
    int breathed = 0;
    for (const Sample& s : samples())
    {
        const auto top = chordsAt(s, kTop);
        const auto high = chordsAt(s, kHigh);
        for (int bar = 0; bar < s.bars; ++bar)
        {
            if (!contains(breathBars(s.bars), bar))
            {
                REQUIRE(pitchStarts(high, bar) == pitchStarts(top, bar));
                continue;
            }
            const auto before = starts(top, bar, bar);
            const auto after = starts(high, bar, bar);
            if (before.size() < 2)
            {
                REQUIRE(after == before);
                continue;
            }
            ++breathed;
            auto expected = before;
            expected.erase(std::prev(expected.end()));
            REQUIRE(after == expected);
        }
    }
    REQUIRE(breathed > 0);
}

TEST_CASE("melody: the answer phrase uses another of the entry's cells over the last half-phrase",
          "[variety][melody]")
{
    int answered = 0;
    for (const Sample& s : samples())
    {
        const Idea low = atEnergy(s.idea, kLow);
        const Idea mid = atEnergy(s.idea, kMid);
        const auto lowEvents = detail::melodyEvents(*s.entry, low, low.lanes.at(2).seed);
        const auto midEvents = detail::melodyEvents(*s.entry, mid, mid.lanes.at(2).seed);
        const Groove groove = s.idea.settings.groove;
        const bool changes = s.bars > 4 && s.entry->melodyRhythms.size() > 1;
        if (!changes)
        {
            REQUIRE(midEvents == lowEvents);
            continue;
        }
        // Pitches after an answer block (bars 9 to 14 of a 16-bar idea, which only a loaded project
        // has) may drift, as the pitch walk chains from the previous note; only the notes before
        // the first answer are compared.
        const int firstAnswer = answerBars(s.bars).front();
        REQUIRE(starts(midEvents, 0, firstAnswer - 1) == starts(lowEvents, 0, firstAnswer - 1));
        for (const NoteEvent& e : midEvents)
        {
            if (barOf(e) < firstAnswer)
            {
                REQUIRE(std::ranges::find(lowEvents, e) != lowEvents.end());
            }
        }
        const auto first = starts(midEvents, 0, 1);
        for (int block = 3; block * 2 < s.bars; block += 4)
        {
            const int bar = block * 2;
            const auto actual = starts(midEvents, bar, bar + 1);
            const bool other = std::ranges::any_of(
                s.entry->melodyRhythms,
                [&](const RhythmCell& cell)
                {
                    const auto cellSet = cellStarts(cell, groove, {.firstBar = bar, .bars = 2});
                    return cellSet == actual &&
                           cellStarts(cell, groove, {.firstBar = 0, .bars = 2}) != first;
                });
            REQUIRE(other);
            ++answered;
        }
    }
    REQUIRE(answered > 0);
}

TEST_CASE("melody: the breath drops only the last note of bar 4 of each 8, above energy 60",
          "[variety][melody]")
{
    int breathed = 0;
    for (const Sample& s : samples())
    {
        const Idea top = atEnergy(s.idea, kTop);
        const Idea high = atEnergy(s.idea, kHigh);
        const auto topEvents = detail::melodyEvents(*s.entry, top, top.lanes.at(2).seed);
        const auto highEvents = detail::melodyEvents(*s.entry, high, high.lanes.at(2).seed);
        REQUIRE(highEvents.size() <= topEvents.size());
        REQUIRE(topEvents.size() - highEvents.size() <= breathBars(s.bars).size());
        std::vector<NoteEvent> missing;
        std::ranges::set_difference(topEvents, highEvents, std::back_inserter(missing),
                                    [](const NoteEvent& a, const NoteEvent& b)
                                    { return a.start < b.start; });
        for (const NoteEvent& gone : missing)
        {
            ++breathed;
            REQUIRE(contains(breathBars(s.bars), barOf(gone)));
            const bool last =
                std::ranges::none_of(topEvents, [&](const NoteEvent& e)
                                     { return barOf(e) == barOf(gone) && e.start > gone.start; });
            REQUIRE(last);
        }
        for (const NoteEvent& e : highEvents)
        {
            const auto same = std::ranges::find(topEvents, e.start, &NoteEvent::start);
            REQUIRE(same != topEvents.end());
            REQUIRE(same->pitch == e.pitch);
        }
    }
    REQUIRE(breathed > 0);
}

TEST_CASE("bass: the fill obeys its steps, spares sustained and pedal, and only comes above 60",
          "[variety][bass]")
{
    constexpr std::array<int, 5> kFillSteps = {9, 10, 11, 13, 14};
    int filled = 0;
    for (const Sample& s : samples())
    {
        const std::uint64_t seed = s.idea.lanes.at(1).seed;
        const Idea top = atEnergy(s.idea, kTop);
        const Idea high = atEnergy(s.idea, kHigh);
        const int lastBar = (s.bars - 1) * kTicksPerBar;
        const int window = lastBar + (8 * kTicksPerSixteenth);
        const int swing = swingPermille(s.idea.settings.groove);
        for (const BassPattern pattern : s.entry->bassPatterns)
        {
            const auto base = detail::bassEvents(top, pattern, seed);
            const auto with = detail::bassEvents(high, pattern, seed);
            if (pattern == BassPattern::sustained || pattern == BassPattern::pedal)
            {
                REQUIRE(with == base);
                continue;
            }
            const auto earlier = [&](const std::vector<NoteEvent>& events)
            {
                std::vector<NoteEvent> out;
                std::ranges::copy_if(events, std::back_inserter(out),
                                     [&](const NoteEvent& e) { return e.start < window; });
                return out;
            };
            const auto beforeFill = earlier(base);
            const auto afterFill = earlier(with);
            REQUIRE(afterFill.size() == beforeFill.size());
            for (std::size_t i = 0; i < afterFill.size(); ++i)
            {
                REQUIRE(afterFill.at(i).start == beforeFill.at(i).start);
                REQUIRE(afterFill.at(i).pitch == beforeFill.at(i).pitch);
            }
            if (with == base)
            {
                continue;
            }
            ++filled;
            std::vector<NoteEvent> fill;
            std::ranges::copy_if(with, std::back_inserter(fill),
                                 [&](const NoteEvent& e) { return e.start >= window; });
            const int approach = detail::swung(lastBar + (15 * kTicksPerSixteenth), swing);
            REQUIRE(fill.back().start == approach);
            const int approachPitch = fill.back().pitch;
            fill.pop_back();
            REQUIRE(fill.back().pitch != approachPitch);
            REQUIRE(fill.size() >= 3);
            REQUIRE(fill.size() <= 4);
            for (const NoteEvent& e : fill)
            {
                REQUIRE(e.length <= kTicksPerSixteenth);
                REQUIRE(e.length >= kTicksPerSixteenth / 2);
                REQUIRE(std::ranges::any_of(kFillSteps,
                                            [&](int step)
                                            {
                                                return detail::swung(
                                                           lastBar + (step * kTicksPerSixteenth),
                                                           swing) == e.start;
                                            }));
            }
        }
    }
    REQUIRE(filled > 0);
}

TEST_CASE("every idea validates at the tier edges, in 4, 8 and 16 bars", "[variety]")
{
    for (const Sample& s : samples())
    {
        for (const int energy : {kLow, kMid, kTop, kHigh, 100})
        {
            Settings settings = s.idea.settings;
            settings.energy = energy;
            const Idea idea = generate(bank(), settings, s.idea.seed);
            REQUIRE(validate(idea).empty());
        }
    }
}

TEST_CASE("the chord turnaround draws from its own slot: the low tier is unchanged",
          "[variety][seeds]")
{
    for (const Sample& s : samples())
    {
        REQUIRE(chordsAt(s, 0) == chordsAt(s, kLow));
    }
}

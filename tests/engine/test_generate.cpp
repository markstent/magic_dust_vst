#include "md/generate.hpp"

#include "md/bank.hpp"
#include "md/midifile.hpp"
#include "md/ranges.hpp"
#include "md/theory.hpp"
#include "md/validate.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iterator>
#include <set>
#include <stdexcept>
#include <string>
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
constexpr int kSeedsPerStyle = 200;

Settings settingsFor(Style style, int i)
{
    constexpr std::array<int, 3> bars = {4, 8, 16};
    Settings settings;
    settings.style = style;
    settings.keyRoot = i % 12;
    settings.mood = (i * 13) % 101;
    settings.energy = (i * 7) % 101;
    settings.groove = static_cast<Groove>(i % 4);
    settings.bars = bars.at(static_cast<std::size_t>(i % 3));
    return settings;
}

std::vector<Idea> sampleIdeas(int energyOverride = -1)
{
    std::vector<Idea> ideas;
    for (const Style style : kStyles)
    {
        for (int i = 0; i < kSeedsPerStyle; ++i)
        {
            Settings settings = settingsFor(style, i);
            if (energyOverride >= 0)
            {
                settings.energy = energyOverride;
            }
            ideas.push_back(
                generate(bank(), settings, static_cast<std::uint32_t>(1000 + (i * 37))));
        }
    }
    return ideas;
}

const std::vector<Idea>& allIdeas()
{
    static const std::vector<Idea> ideas = sampleIdeas();
    return ideas;
}

const StyleEntry& entryOf(const Idea& idea)
{
    const auto it = std::ranges::find(bank().entries, idea.entryId, &StyleEntry::id);
    REQUIRE(it != bank().entries.end());
    return *it;
}

const PlacedChord& chordAt(const Idea& idea, int tick)
{
    for (const PlacedChord& chord : idea.progression)
    {
        if (tick >= chord.startBar * kTicksPerBar &&
            tick < (chord.startBar + chord.bars) * kTicksPerBar)
        {
            return chord;
        }
    }
    FAIL("no chord at tick " << tick);
    return idea.progression.front();
}

const PlacedChord& homeChord(const Idea& idea)
{
    const auto it = std::ranges::find(idea.progression, 1,
                                      [](const PlacedChord& c) { return c.symbol.degree; });
    return it == idea.progression.end() ? idea.progression.front() : *it;
}

bool contains(const std::vector<int>& values, int value)
{
    return std::ranges::find(values, value) != values.end();
}

// The pitch classes that sound: the symbol's, or the voicing's for parallel entries.
std::vector<int> soundingPcs(const Idea& idea, const PlacedChord& chord)
{
    if (!idea.parallel)
    {
        return chordPitchClasses(idea.key, chord.symbol);
    }
    std::set<int> pcs;
    for (const int pitch : chord.voicing)
    {
        pcs.insert(pitch % 12);
    }
    return {pcs.begin(), pcs.end()};
}

std::vector<int> allowedPcs(const Idea& idea, const PlacedChord& chord)
{
    const auto pcs = soundingPcs(idea, chord);
    std::vector<int> allowed;
    for (const int pc : pcs)
    {
        const bool clash = std::ranges::any_of(pcs,
                                               [&](int other)
                                               {
                                                   const int d = (other - pc + 12) % 12;
                                                   return other != pc && (d < 2 || d > 10);
                                               });
        if (!clash)
        {
            allowed.push_back(pc);
        }
    }
    return allowed;
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

int swungTick(int step, int permille)
{
    const int tick = step * kTicksPerSixteenth;
    return step % 2 == 0 ? tick : tick - kTicksPerSixteenth + ((480 * permille) / 1000);
}

// Onsets of one 2-bar block of a melody cell, tiled when the cell is 1 bar.
std::vector<int> cellOnsets(const RhythmCell& cell, int permille)
{
    std::vector<int> onsets;
    for (int tile = 0; tile < 2 / cell.bars; ++tile)
    {
        for (const Hit& hit : cell.hits)
        {
            onsets.push_back(swungTick((tile * cell.bars * 16) + hit.step, permille));
        }
    }
    std::ranges::sort(onsets);
    return onsets;
}

std::vector<int> blockOnsets(const Idea& idea, int block)
{
    std::vector<int> onsets;
    const int begin = block * 2 * kTicksPerBar;
    for (const NoteEvent& e : idea.lanes.at(2).events)
    {
        if (e.start >= begin && e.start < begin + (2 * kTicksPerBar))
        {
            onsets.push_back(e.start - begin);
        }
    }
    return onsets;
}

Bank bankWith(const std::function<void(StyleEntry&)>& edit)
{
    StyleEntry entry = bank().entries.front();
    edit(entry);
    return makeBank({entry});
}
} // namespace

TEST_CASE("generate is deterministic and produces a valid 8-bar idea", "[generate]")
{
    const Settings defaults;
    const Idea a = generate(bank(), defaults, 48213);
    const Idea b = generate(bank(), defaults, 48213);
    REQUIRE(writeMidiFile(ideaTracks(a), 124, ideaLengthTicks(a)) ==
            writeMidiFile(ideaTracks(b), 124, ideaLengthTicks(b)));
    REQUIRE(validate(a).empty());
    int bars = 0;
    for (const PlacedChord& chord : a.progression)
    {
        bars += chord.bars;
    }
    REQUIRE(bars == 8);
    for (const Lane& lane : a.lanes)
    {
        REQUIRE_FALSE(lane.events.empty());
    }
    REQUIRE(a.key.root == 9);
    REQUIRE(a.seed == 48213);
    REQUIRE(a.bankVersion == bank().version);
}

TEST_CASE("a progression longer than the idea is truncated", "[generate]")
{
    const Progression vampProgression{
        .chords = {{.degree = 1, .quality = Quality::min11}},
        .barsPerChord = {8},
        .weightMilli = 1000,
    };
    const Bank vamp = bankWith([&](StyleEntry& entry) { entry.progressions = {vampProgression}; });
    Settings settings;
    settings.bars = 4;
    const Idea idea = generate(vamp, settings, 7);
    REQUIRE(idea.progression.size() == 1);
    REQUIRE(idea.progression.front().bars == 4);
    for (const Lane& lane : idea.lanes)
    {
        REQUIRE_FALSE(lane.events.empty());
        for (const NoteEvent& e : lane.events)
        {
            REQUIRE(e.start + e.length <= 15360);
        }
    }
}

TEST_CASE("every voicing is four notes in range and not muddy", "[generate]")
{
    for (const Idea& idea : allIdeas())
    {
        const StyleEntry& entry = entryOf(idea);
        for (const PlacedChord& chord : idea.progression)
        {
            const Voicing& v = chord.voicing;
            REQUIRE(v.size() == 4);
            REQUIRE(std::ranges::is_sorted(v));
            REQUIRE(v.front() >= kChordLow);
            REQUIRE(v.back() <= kChordHigh);
            REQUIRE(v.back() >= std::max(kChordTopLow, entry.topNoteLow));
            REQUIRE(v.back() <= std::min(kChordTopHigh, entry.topNoteHigh));
            REQUIRE_FALSE(isMuddy(v));
        }
    }
}

TEST_CASE("bass is monophonic, in range and built from root and fifth", "[generate]")
{
    std::set<std::string> seen;
    for (const Idea& idea : allIdeas())
    {
        const Lane& bass = idea.lanes.at(1);
        seen.insert(bass.pattern);
        const bool offbeat = bass.pattern == "offbeat" || bass.pattern == "rolling-offbeat" ||
                             bass.pattern == "pedal";
        for (std::size_t i = 0; i < bass.events.size(); ++i)
        {
            const NoteEvent& e = bass.events.at(i);
            REQUIRE(e.pitch >= kBassLow);
            REQUIRE(e.pitch <= kBassHigh);
            if (i + 1 < bass.events.size())
            {
                REQUIRE(e.start + e.length <= bass.events.at(i + 1).start);
            }
            if (offbeat)
            {
                REQUIRE(e.start % kTicksPerQuarter != 0);
            }
            const int root = chordRootPc(idea.key, chordAt(idea, e.start).symbol);
            const std::vector<int> pcs = bass.pattern == "pedal"
                                             ? std::vector<int>{idea.key.root}
                                             : std::vector<int>{root, (root + 7) % 12};
            REQUIRE(contains(pcs, e.pitch % 12));
        }
    }
    for (const char* pattern :
         {"offbeat", "rolling-offbeat", "octave-bounce", "garage-skip", "sustained", "pedal"})
    {
        REQUIRE(seen.contains(pattern));
    }
}

TEST_CASE("melody stays in range, leaps are limited and strong beats avoid clashes", "[generate]")
{
    for (const Idea& idea : allIdeas())
    {
        const auto& events = idea.lanes.at(2).events;
        REQUIRE_FALSE(events.empty());
        for (std::size_t i = 0; i < events.size(); ++i)
        {
            const NoteEvent& e = events.at(i);
            REQUIRE(e.pitch >= kMelodyLow);
            REQUIRE(e.pitch <= kMelodyHigh);
            if (i > 0)
            {
                const int leap = std::abs(e.pitch - events.at(i - 1).pitch);
                REQUIRE((leap <= kMaxMelodyLeap || leap == 12));
                REQUIRE(events.at(i - 1).start + events.at(i - 1).length <= e.start);
            }
            if (e.start % kTicksPerQuarter == 0)
            {
                REQUIRE(contains(allowedPcs(idea, chordAt(idea, e.start)), e.pitch % 12));
            }
        }
    }
}

TEST_CASE("the final melody note lands on the home chord when it can", "[generate]")
{
    int checked = 0;
    for (const Idea& idea : allIdeas())
    {
        const Lane& melody = idea.lanes.at(2);
        if (melody.pattern == "vocal-chop")
        {
            continue;
        }
        const NoteEvent& last = melody.events.back();
        const auto allowed = allowedPcs(idea, chordAt(idea, last.start));
        const auto home = soundingPcs(idea, homeChord(idea));
        const bool overlap =
            std::ranges::any_of(allowed, [&](int pc) { return contains(home, pc); });
        if (overlap)
        {
            ++checked;
            REQUIRE(contains(home, last.pitch % 12));
        }
    }
    REQUIRE(checked > 0);
}

TEST_CASE("melody blocks follow the entry's rhythm cells", "[generate]")
{
    for (const Idea& idea : allIdeas())
    {
        const StyleEntry& entry = entryOf(idea);
        const int permille = swingPermille(idea.settings.groove);
        std::vector<std::vector<int>> cells;
        std::ranges::transform(entry.melodyRhythms, std::back_inserter(cells),
                               [permille](const RhythmCell& cell)
                               { return cellOnsets(cell, permille); });
        const int blocks = idea.settings.bars / 2;
        for (int block = 0; block < blocks; ++block)
        {
            REQUIRE(std::ranges::find(cells, blockOnsets(idea, block)) != cells.end());
        }
        // Phrase form A A A B, repeated every 4 blocks.
        for (int block = 1; block < blocks; ++block)
        {
            const bool phraseEnd = block % 4 == 3;
            const bool sameAsFirst = blockOnsets(idea, block) == blockOnsets(idea, 0);
            REQUIRE(sameAsFirst == (!phraseEnd || entry.melodyRhythms.size() < 2));
        }
        if (blocks == 8)
        {
            REQUIRE(blockOnsets(idea, 7) == blockOnsets(idea, 3));
        }
    }
}

TEST_CASE("energy picks the sparsest and the busiest melody rhythm", "[generate]")
{
    for (const int energy : {0, 100})
    {
        for (const Idea& idea : sampleIdeas(energy))
        {
            const StyleEntry& entry = entryOf(idea);
            std::vector<std::size_t> counts;
            std::ranges::transform(entry.melodyRhythms, std::back_inserter(counts),
                                   [](const RhythmCell& cell)
                                   { return cellOnsets(cell, 500).size(); });
            const std::size_t expected =
                energy == 0 ? *std::ranges::min_element(counts) : *std::ranges::max_element(counts);
            REQUIRE(blockOnsets(idea, 0).size() == expected);
        }
    }
}

TEST_CASE("every chord is in the mode or in one borrowed mode", "[generate]")
{
    for (const Idea& idea : allIdeas())
    {
        auto scale = scalePitchClasses(idea.key);
        if (idea.borrowedFrom)
        {
            const auto extra =
                scalePitchClasses({.root = idea.key.root, .mode = *idea.borrowedFrom});
            scale.insert(scale.end(), extra.begin(), extra.end());
        }
        REQUIRE(idea.parallel == entryOf(idea).parallel);
        if (idea.parallel)
        {
            continue;
        }
        std::vector<ChordSymbol> symbols;
        std::ranges::transform(idea.progression, std::back_inserter(symbols), &PlacedChord::symbol);
        REQUIRE(checkProgression(idea.key.mode, symbols).ok);
        for (const NoteEvent& e : idea.lanes.at(0).events)
        {
            REQUIRE(contains(scale, e.pitch % 12));
        }
    }
}

TEST_CASE("every bank style and every generated idea validates", "[generate]")
{
    for (const Idea& idea : allIdeas())
    {
        REQUIRE(validate(idea).empty());
    }
}

TEST_CASE("swing moves the second sixteenth of each pair and nothing else", "[generate]")
{
    const RhythmCell cell{
        .id = "t",
        .bars = 1,
        .hits = {Hit{.step = 2, .length = 1}, Hit{.step = 3, .length = 1}},
    };
    const Bank swung = bankWith([&](StyleEntry& entry) { entry.chordRhythms = {cell}; });
    const auto chordStarts = [&](Groove groove)
    {
        Settings settings;
        settings.groove = groove;
        std::set<int> starts;
        const Idea idea = generate(swung, settings, 5);
        for (const NoteEvent& e : idea.lanes.at(0).events)
        {
            if (e.start < kTicksPerBar)
            {
                starts.insert(e.start);
            }
        }
        return starts;
    };
    REQUIRE(chordStarts(Groove::heavy62) == std::set<int>{480, 777});
    REQUIRE(chordStarts(Groove::straight) == std::set<int>{480, 720});
}

TEST_CASE("generate rejects invalid settings and empty banks", "[generate]")
{
    REQUIRE_THROWS_AS(generate(bank(), Settings{}, kSeedMax + 1), std::invalid_argument);
    Settings settings;
    settings.bars = 5;
    REQUIRE_THROWS_AS(generate(bank(), settings, 1), std::invalid_argument);
    settings = {};
    settings.keyRoot = 12;
    REQUIRE_THROWS_AS(generate(bank(), settings, 1), std::invalid_argument);
    settings = {};
    settings.keyRoot = -1;
    REQUIRE_THROWS_AS(generate(bank(), settings, 1), std::invalid_argument);
    settings = {};
    settings.mood = 101;
    REQUIRE_THROWS_AS(generate(bank(), settings, 1), std::invalid_argument);
    settings = {};
    settings.energy = -1;
    REQUIRE_THROWS_AS(generate(bank(), settings, 1), std::invalid_argument);
    const Bank emptyBank{.entries = {}, .version = 0};
    REQUIRE_THROWS_AS(generate(emptyBank, Settings{}, 1), std::invalid_argument);
    const Bank mkDubOnly = bankWith([](StyleEntry& entry) { entry.family = Family::mkDub; });
    settings = {};
    settings.style = Style::soulful;
    REQUIRE_THROWS_AS(generate(mkDubOnly, settings, 1), std::invalid_argument);
}

TEST_CASE("octave bounce falls back to the fifth when the root has no octave in range",
          "[generate]")
{
    const Bank bounce =
        bankWith([](StyleEntry& entry) { entry.bassPatterns = {BassPattern::octaveBounce}; });
    Settings settings;
    settings.keyRoot = 2; // D: the root sits at 38, with no octave inside 28 to 48
    const Idea idea = generate(bounce, settings, 3);
    const auto& bass = idea.lanes.at(1).events;
    REQUIRE(idea.lanes.at(1).pattern == "octave-bounce");
    REQUIRE(std::ranges::any_of(bass, [](const NoteEvent& e) { return e.pitch == 45; }));
    for (const NoteEvent& e : bass)
    {
        REQUIRE(e.pitch >= kBassLow);
        REQUIRE(e.pitch <= kBassHigh);
    }
}

TEST_CASE("generate rejects entries that would break it", "[generate]")
{
    const Settings settings;
    REQUIRE_THROWS_AS(generate(bankWith([](StyleEntry& e)
                                        { e.progressions.front().barsPerChord = {0, 0, 0, 0}; }),
                               settings, 1),
                      std::logic_error);
    REQUIRE_THROWS_AS(
        generate(bankWith([](StyleEntry& e) { e.melodyRhythms.front().hits.clear(); }), settings,
                 1),
        std::logic_error);
    REQUIRE_THROWS_AS(
        generate(bankWith([](StyleEntry& e) { e.chordRhythms.front().bars = 0; }), settings, 1),
        std::logic_error);
    REQUIRE_THROWS_AS(
        generate(bankWith([](StyleEntry& e) { e.melodyRhythms.front().bars = 0; }), settings, 1),
        std::logic_error);
}

TEST_CASE("every bundled entry generates across keys, bar counts, mood and energy", "[generate]")
{
    constexpr std::array<Style, 5> families = {
        Style::mkDub, Style::soulful, Style::bassline, Style::festival, Style::nyGroove,
    };
    std::set<std::string> seen;
    for (const Style style : families)
    {
        for (int key = 0; key < 12; ++key)
        {
            for (const int bars : {4, 8, 16})
            {
                for (const int corner : {0, 1, 2, 3})
                {
                    Settings settings;
                    settings.style = style;
                    settings.keyRoot = key;
                    settings.bars = bars;
                    settings.mood = (corner % 2) * 100;
                    settings.energy = (corner / 2) * 100;
                    for (int seed = 0; seed < 20; ++seed)
                    {
                        const Idea idea =
                            generate(bank(), settings, static_cast<std::uint32_t>(seed * 4999));
                        seen.insert(idea.entryId);
                    }
                }
            }
        }
    }
    REQUIRE(seen.size() == bank().entries.size());
}

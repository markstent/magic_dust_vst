#include "md/generate.hpp"

#include "internal.hpp"
#include "md/bank.hpp"
#include "md/ideacode.hpp"
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

TEST_CASE("every voicing is four or five notes in range and not muddy", "[generate]")
{
    for (const Idea& idea : allIdeas())
    {
        const StyleEntry& entry = entryOf(idea);
        for (const PlacedChord& chord : idea.progression)
        {
            const Voicing& v = chord.voicing;
            REQUIRE((v.size() == 4 || v.size() == 5));
            REQUIRE(std::ranges::is_sorted(v));
            REQUIRE(v.front() >= kChordLow);
            REQUIRE(v.back() <= kChordHigh);
            REQUIRE(v.back() >= std::max(kChordTopLow, entry.topNoteLow));
            REQUIRE(v.back() <= std::min(kChordTopHigh, entry.topNoteHigh));
            REQUIRE_FALSE(isMuddy(v));
        }
    }
}

namespace
{
// Each chord change after bar 1, then the idea's end (the seam into bar 1).
std::vector<int> changeTicks(const Idea& idea)
{
    std::vector<int> changes;
    for (std::size_t i = 1; i < idea.progression.size(); ++i)
    {
        changes.push_back(idea.progression.at(i).startBar * kTicksPerBar);
    }
    changes.push_back(idea.settings.bars * kTicksPerBar);
    return changes;
}

int approachTick(const Idea& idea, int change)
{
    return detail::swung(change - kTicksPerSixteenth, swingPermille(idea.settings.groove));
}

const NoteEvent* noteStartingAt(const Idea& idea, int tick)
{
    const auto& events = idea.lanes.at(1).events;
    const auto it = std::ranges::find(events, tick, &NoteEvent::start);
    return it == events.end() ? nullptr : &*it;
}

const NoteEvent& bassTarget(const Idea& idea, int change)
{
    const auto& events = idea.lanes.at(1).events;
    if (change == idea.settings.bars * kTicksPerBar)
    {
        return events.front();
    }
    return *std::ranges::find_if(events,
                                 [change](const NoteEvent& e) { return e.start >= change; });
}
} // namespace

TEST_CASE("bass is monophonic, in range and built from root and fifth", "[generate]")
{
    std::set<std::string> seen;
    for (const Idea& idea : allIdeas())
    {
        const Lane& bass = idea.lanes.at(1);
        seen.insert(bass.pattern);
        // Above half energy an approach note leaves the root and fifth; its own tests check it.
        const bool approaches =
            idea.settings.energy > 50 && bass.pattern != "pedal" && bass.pattern != "sustained";
        const std::vector<int> changes = changeTicks(idea);
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
            const auto isApproach = [&](int change)
            {
                return e.start == approachTick(idea, change) &&
                       std::abs(e.pitch - bassTarget(idea, change).pitch) <= 2;
            };
            if (approaches && std::ranges::any_of(changes, isApproach))
            {
                continue;
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
        if (melody.pattern == "pluck")
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
    int roots = 0;
    for (std::size_t i = 0; i < bass.size(); ++i)
    {
        if (bass.at(i).pitch == 38)
        {
            ++roots;
            REQUIRE(i + 1 < bass.size());
            REQUIRE(bass.at(i + 1).pitch == 45); // the fifth, not 38 + 12
        }
    }
    REQUIRE(roots > 0);
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
    // The entry depends only on the style and the seed, so find seeds that reach every entry.
    std::set<std::string> seen;
    std::set<std::uint32_t> seeds;
    const Settings defaults;
    for (std::uint32_t seed = 0; seed <= 10000 && seen.size() < bank().entries.size(); ++seed)
    {
        if (seen.insert(generate(bank(), defaults, seed).entryId).second)
        {
            seeds.insert(seed);
        }
    }
    REQUIRE(seen.size() == bank().entries.size());
    for (std::uint32_t seed = 0; seed < 20; ++seed)
    {
        seeds.insert(seed);
    }
    for (const std::uint32_t seed : seeds)
    {
        for (int key = 0; key < 12; ++key)
        {
            for (const int bars : {4, 8, 16})
            {
                for (const int corner : {0, 1, 2, 3})
                {
                    Settings settings;
                    settings.keyRoot = key;
                    settings.bars = bars;
                    settings.mood = (corner % 2) * 100;
                    settings.energy = (corner / 2) * 100;
                    REQUIRE_NOTHROW(generate(bank(), settings, seed));
                }
            }
        }
    }
}

TEST_CASE("the pluck throws when the home chord has no allowed tone", "[generate]")
{
    Pcg32 rng(1, 1);
    REQUIRE_THROWS_AS(detail::pluckPitches({}, rng), std::logic_error);
}

namespace
{
std::vector<std::uint8_t> laneBytes(const Idea& idea, LaneId lane)
{
    const std::array tracks = {laneTrack(idea, lane)};
    return writeMidiFile(tracks, 120, ideaLengthTicks(idea));
}

Idea withLaneSeeds(const LaneSeeds& seeds) { return generate(bank(), Settings{}, 4242, seeds); }
} // namespace

TEST_CASE("default lane seeds reproduce the three-argument generate", "[generate][seeds]")
{
    const Idea plain = generate(bank(), Settings{}, 4242);
    const Idea seeded = withLaneSeeds(defaultLaneSeeds(4242));
    for (const LaneId lane : {LaneId::chords, LaneId::bass, LaneId::melody})
    {
        REQUIRE(laneBytes(plain, lane) == laneBytes(seeded, lane));
    }
    REQUIRE(seeded.lanes.at(0).seed == defaultLaneSeeds(4242).chords);
    REQUIRE(seeded.lanes.at(1).seed == defaultLaneSeeds(4242).bass);
    REQUIRE(seeded.lanes.at(2).seed == defaultLaneSeeds(4242).melody);
}

TEST_CASE("rerolling the bass seed changes only the bass", "[generate][seeds]")
{
    const LaneSeeds base = defaultLaneSeeds(4242);
    LaneSeeds other = base;
    other.bass += 1;
    const Idea a = withLaneSeeds(base);
    const Idea b = withLaneSeeds(other);
    REQUIRE(laneBytes(a, LaneId::chords) == laneBytes(b, LaneId::chords));
    REQUIRE(laneBytes(a, LaneId::melody) == laneBytes(b, LaneId::melody));
    REQUIRE(laneBytes(a, LaneId::bass) != laneBytes(b, LaneId::bass));
    REQUIRE(b.lanes.at(1).seed == other.bass);
}

TEST_CASE("rerolling the melody seed changes only the melody", "[generate][seeds]")
{
    const LaneSeeds base = defaultLaneSeeds(4242);
    LaneSeeds other = base;
    other.melody += 1;
    const Idea a = withLaneSeeds(base);
    const Idea b = withLaneSeeds(other);
    REQUIRE(laneBytes(a, LaneId::chords) == laneBytes(b, LaneId::chords));
    REQUIRE(laneBytes(a, LaneId::bass) == laneBytes(b, LaneId::bass));
    REQUIRE(laneBytes(a, LaneId::melody) != laneBytes(b, LaneId::melody));
    REQUIRE(b.lanes.at(2).seed == other.melody);
}

TEST_CASE("rerolling the chords seed may change every lane", "[generate][seeds]")
{
    const LaneSeeds base = defaultLaneSeeds(4242);
    std::set<std::uint32_t> entryAndProgression;
    for (std::uint64_t bump = 0; bump < 20; ++bump)
    {
        LaneSeeds other = base;
        other.chords += bump;
        const Idea idea = withLaneSeeds(other);
        REQUIRE(validate(idea).empty());
        entryAndProgression.insert(
            static_cast<std::uint32_t>(std::hash<std::string>{}(idea.entryId)));
    }
    REQUIRE(entryAndProgression.size() > 1);
}

TEST_CASE("an idea code recreates byte-identical MIDI", "[generate][ideacode]")
{
    for (const Style style : kStyles)
    {
        for (int i = 0; i < 12; ++i)
        {
            const Settings settings = settingsFor(style, i);
            const auto seed = static_cast<std::uint32_t>(i * 8191);
            const Idea first = generate(bank(), settings, seed);
            const auto decoded = decodeIdea(encodeIdea(seed, settings, bank().version));
            if (!decoded)
            {
                FAIL("the idea code did not decode");
            }
            const Idea again = generate(bank(), decoded->settings, decoded->seed);
            REQUIRE(decoded->seed == seed);
            const auto bytes = [](const Idea& idea)
            { return writeMidiFile(ideaTracks(idea), 124, ideaLengthTicks(idea)); };
            REQUIRE(bytes(again) == bytes(first));
        }
    }
}

TEST_CASE("soulful flavour is the default and keeps the written chords", "[generate][flavour]")
{
    const auto bytes = [](const Idea& idea)
    { return writeMidiFile(ideaTracks(idea), 124, ideaLengthTicks(idea)); };
    REQUIRE(Settings{}.flavour == Flavour::soulful);
    Settings settings = settingsFor(Style::ukFunky, 3);
    settings.flavour = Flavour::soulful;
    const Settings defaults = settingsFor(Style::ukFunky, 3);
    REQUIRE(bytes(generate(bank(), settings, 7)) == bytes(generate(bank(), defaults, 7)));
}

TEST_CASE("simple flavour has no chord above a 7th", "[generate][flavour]")
{
    for (const Style style : kStyles)
    {
        for (std::uint32_t seed = 0; seed < 50; ++seed)
        {
            Settings settings = settingsFor(style, static_cast<int>(seed));
            settings.flavour = Flavour::simple;
            const Idea idea = generate(bank(), settings, seed);
            REQUIRE(validate(idea).empty());
            for (const PlacedChord& chord : idea.progression)
            {
                const Quality q = chord.symbol.quality;
                REQUIRE(q != Quality::min9);
                REQUIRE(q != Quality::min11);
                REQUIRE(q != Quality::maj9);
                REQUIRE(q != Quality::dom9);
                REQUIRE(q != Quality::dom13);
                REQUIRE(q != Quality::add9);
            }
        }
    }
}

TEST_CASE("jazzy flavour changes chord qualities and stays valid", "[generate][flavour]")
{
    bool changed = false;
    for (const Style style : kStyles)
    {
        for (std::uint32_t seed = 0; seed < 50; ++seed)
        {
            Settings settings = settingsFor(style, static_cast<int>(seed));
            settings.flavour = Flavour::soulful;
            const Idea plain = generate(bank(), settings, seed);
            settings.flavour = Flavour::jazzy;
            const Idea jazzy = generate(bank(), settings, seed);
            REQUIRE(validate(jazzy).empty());
            for (std::size_t i = 0; i < plain.progression.size(); ++i)
            {
                changed = changed || plain.progression.at(i).symbol.quality !=
                                         jazzy.progression.at(i).symbol.quality;
            }
        }
    }
    REQUIRE(changed);
}

namespace
{
Bank bankOf(const StyleEntry& entry, VoicingStyle style)
{
    StyleEntry copy = entry;
    copy.voicingStyle = style;
    return makeBank({copy});
}

const StyleEntry& entryWithStyle(VoicingStyle style)
{
    const auto it = std::ranges::find(bank().entries, style, &StyleEntry::voicingStyle);
    REQUIRE(it != bank().entries.end());
    return *it;
}

Settings settingsOf(const StyleEntry& entry)
{
    Settings settings;
    settings.style = static_cast<Style>(static_cast<int>(entry.family) + 1);
    return settings;
}

int spread(const Voicing& v) { return v.back() - v.front(); }
} // namespace

TEST_CASE("every bundled entry's voicings validate for seeds 0 to 99", "[generate][voicing]")
{
    for (const StyleEntry& entry : bank().entries)
    {
        const Bank single = makeBank({entry});
        for (std::uint32_t seed = 0; seed < 100; ++seed)
        {
            REQUIRE(validate(generate(single, settingsOf(entry), seed)).empty());
        }
    }
}

TEST_CASE("every voicing shape validates on every entry", "[generate][voicing]")
{
    for (const VoicingStyle style : {VoicingStyle::close, VoicingStyle::drop2,
                                     VoicingStyle::organStab, VoicingStyle::pianoSpread})
    {
        for (const StyleEntry& entry : bank().entries)
        {
            const Bank single = bankOf(entry, style);
            for (std::uint32_t seed = 0; seed < 100; ++seed)
            {
                REQUIRE(validate(generate(single, settingsOf(entry), seed)).empty());
            }
        }
    }
}

TEST_CASE("drop 2 voicings are wider than the close voicing of the same chord",
          "[generate][voicing]")
{
    const StyleEntry& entry = entryWithStyle(VoicingStyle::drop2);
    const Idea dropped = generate(bankOf(entry, VoicingStyle::drop2), settingsOf(entry), 0);
    const Idea close = generate(bankOf(entry, VoicingStyle::close), settingsOf(entry), 0);
    REQUIRE(dropped.progression.size() == close.progression.size());
    for (std::size_t i = 0; i < close.progression.size(); ++i)
    {
        REQUIRE(spread(dropped.progression.at(i).voicing) >
                spread(close.progression.at(i).voicing));
    }
}

TEST_CASE("organ stab voicings double the lowest note up an octave", "[generate][voicing]")
{
    const StyleEntry& entry = entryWithStyle(VoicingStyle::organStab);
    bool five = false;
    for (std::uint32_t seed = 0; seed < 100; ++seed)
    {
        const Idea idea = generate(makeBank({entry}), settingsOf(entry), seed);
        for (const PlacedChord& chord : idea.progression)
        {
            five = five || chord.voicing.size() == 5;
        }
    }
    REQUIRE(five);
}

TEST_CASE("piano spread voicings put the root at the bottom", "[generate][voicing]")
{
    const StyleEntry& entry = entryWithStyle(VoicingStyle::pianoSpread);
    int rooted = 0;
    int total = 0;
    for (std::uint32_t seed = 0; seed < 100; ++seed)
    {
        const Idea idea = generate(makeBank({entry}), settingsOf(entry), seed);
        for (const PlacedChord& chord : idea.progression)
        {
            ++total;
            rooted += chord.voicing.front() % 12 == chordRootPc(idea.key, chord.symbol) ? 1 : 0;
        }
    }
    REQUIRE(rooted * 4 >= total * 3);
}

TEST_CASE("consecutive top notes move at most 7 semitones", "[generate][voicing]")
{
    for (const StyleEntry& entry : bank().entries)
    {
        for (std::uint32_t seed = 0; seed < 100; ++seed)
        {
            const Idea idea = generate(makeBank({entry}), settingsOf(entry), seed);
            for (std::size_t i = 1; i < idea.progression.size() && !idea.parallel; ++i)
            {
                REQUIRE(std::abs(idea.progression.at(i).voicing.back() -
                                 idea.progression.at(i - 1).voicing.back()) <= kMaxVoiceMove);
            }
        }
    }
}

TEST_CASE("a progression a shape can only leap through is voiced in close position",
          "[generate][voicing]")
{
    const auto it = std::ranges::find(bank().entries, "soulful-starter-01", &StyleEntry::id);
    REQUIRE(it != bank().entries.end());
    const Settings settings = settingsFor(Style::soulful, 1);
    const Idea spread = generate(bankOf(*it, VoicingStyle::pianoSpread), settings, 1);
    const Idea close = generate(bankOf(*it, VoicingStyle::close), settings, 1);
    REQUIRE(spread.progression.size() == close.progression.size());
    for (std::size_t i = 0; i < close.progression.size(); ++i)
    {
        REQUIRE(spread.progression.at(i).voicing == close.progression.at(i).voicing);
    }
}

TEST_CASE("every lane starts its notes on the shared swing grid at every groove",
          "[generate][swing]")
{
    constexpr int kGuardSeeds = 12;
    for (const Groove groove :
         {Groove::straight, Groove::swing54, Groove::ukShuffle58, Groove::heavy62})
    {
        Settings settings = settingsFor(Style::ukFunky, 0);
        settings.groove = groove;
        const int permille = swingPermille(groove);
        std::set<int> grid; // the tick each nominal 16th step lands on
        for (int step = 0; step < settings.bars * kStepsPerBar; ++step)
        {
            grid.insert(detail::swung(step * kTicksPerSixteenth, permille));
        }
        REQUIRE(grid.size() == static_cast<std::size_t>(settings.bars * kStepsPerBar));
        for (int seed = 1; seed <= kGuardSeeds; ++seed)
        {
            const Idea idea = generate(bank(), settings, static_cast<std::uint32_t>(seed));
            for (const Lane& lane : idea.lanes)
            {
                for (const NoteEvent& n : lane.events)
                {
                    INFO("groove " << static_cast<int>(groove) << " seed " << seed << " lane "
                                   << static_cast<int>(lane.id) << " start " << n.start);
                    CHECK(grid.contains(n.start));
                }
            }
        }
    }
}

TEST_CASE("every lane's notes stay inside the idea, from tick 0 to its length at every groove",
          "[generate][timing]")
{
    constexpr int kGuardSeeds = 12;
    for (const Groove groove :
         {Groove::straight, Groove::swing54, Groove::ukShuffle58, Groove::heavy62})
    {
        Settings settings = settingsFor(Style::ukFunky, 0);
        settings.groove = groove;
        for (int seed = 1; seed <= kGuardSeeds; ++seed)
        {
            const Idea idea = generate(bank(), settings, static_cast<std::uint32_t>(seed));
            for (const Lane& lane : idea.lanes)
            {
                for (const NoteEvent& n : lane.events)
                {
                    INFO("groove " << static_cast<int>(groove) << " seed " << seed << " lane "
                                   << static_cast<int>(lane.id) << " start " << n.start);
                    CHECK(n.start >= 0);
                    CHECK(n.start + n.length <= ideaLengthTicks(idea));
                }
            }
        }
    }
}

namespace
{
Idea bassWith(const std::vector<BassPattern>& patterns, const Settings& settings,
              std::uint32_t seed)
{
    const Bank only = bankWith([&](StyleEntry& entry) { entry.bassPatterns = patterns; });
    return generate(only, settings, seed);
}

Settings energySettings(int energy)
{
    Settings settings;
    settings.energy = energy;
    return settings;
}

int countPattern(const std::vector<BassPattern>& patterns, int energy, const char* name)
{
    int count = 0;
    for (std::uint32_t seed = 1; seed <= 200; ++seed)
    {
        count +=
            bassWith(patterns, energySettings(energy), seed).lanes.at(1).pattern == name ? 1 : 0;
    }
    return count;
}

// The chord root in 33 to 44.
int expectedRootNote(int rootPc)
{
    for (int note = 33; note <= 44; ++note)
    {
        if (note % 12 == rootPc)
        {
            return note;
        }
    }
    return -1;
}

std::vector<int> bassPitches(const Idea& idea)
{
    std::vector<int> pitches;
    for (const NoteEvent& e : idea.lanes.at(1).events)
    {
        pitches.push_back(e.pitch);
    }
    return pitches;
}
} // namespace

TEST_CASE("energy chooses the bass pattern", "[generate][bass]")
{
    const std::vector<BassPattern> both = {BassPattern::rollingOffbeat, BassPattern::offbeat};
    REQUIRE(countPattern(both, 0, "offbeat") >= 160);
    REQUIRE(countPattern(both, 100, "rolling-offbeat") >= 160);
}

TEST_CASE("the energy choice orders patterns by density, not by the entry's listing",
          "[generate][bass]")
{
    const std::vector<BassPattern> listed = {BassPattern::rollingOffbeat, BassPattern::sustained,
                                             BassPattern::offbeat};
    const auto wins = [&](int energy, const char* name)
    { return countPattern(listed, energy, name); };
    for (const int energy : {0, 50, 100})
    {
        const std::array<int, 3> counts = {wins(energy, "sustained"), wins(energy, "offbeat"),
                                           wins(energy, "rolling-offbeat")};
        const auto best = std::ranges::max_element(counts) - counts.begin();
        REQUIRE(best == energy / 50);
    }
}

TEST_CASE("an entry with one bass pattern always gets it", "[generate][bass]")
{
    REQUIRE(countPattern({BassPattern::garageSkip}, 50, "garage-skip") == 200);
}

TEST_CASE("riff and call-and-response entries play offbeat until those patterns exist",
          "[generate][bass]")
{
    REQUIRE(countPattern({BassPattern::riff}, 80, "offbeat") == 200);
    REQUIRE(countPattern({BassPattern::callResponse}, 20, "offbeat") == 200);
    REQUIRE(countPattern({BassPattern::riff, BassPattern::offbeat}, 100, "offbeat") == 200);
}

TEST_CASE("the bass seed varies pitch and nothing else", "[generate][bass][seeds]")
{
    const Bank rolling =
        bankWith([](StyleEntry& entry) { entry.bassPatterns = {BassPattern::rollingOffbeat}; });
    int differing = 0;
    for (std::uint32_t seed = 1; seed <= 200; ++seed)
    {
        const LaneSeeds base = defaultLaneSeeds(seed);
        LaneSeeds other = base;
        other.bass += 1;
        const Idea a = generate(rolling, Settings{}, seed, base);
        const Idea b = generate(rolling, Settings{}, seed, other);
        REQUIRE(laneBytes(a, LaneId::chords) == laneBytes(b, LaneId::chords));
        REQUIRE(laneBytes(a, LaneId::melody) == laneBytes(b, LaneId::melody));
        differing += bassPitches(a) != bassPitches(b) ? 1 : 0;
    }
    REQUIRE(differing >= 180);
}

TEST_CASE("pitch variation spares sustained and pedal and keeps each chord's first hit on the root",
          "[generate][bass]")
{
    for (std::uint32_t seed = 1; seed <= 50; ++seed)
    {
        const Idea pedal = bassWith({BassPattern::pedal}, energySettings(50), seed);
        const auto held = bassPitches(pedal);
        REQUIRE(std::ranges::adjacent_find(held, std::not_equal_to<>{}) == held.end());
        const Idea sustained = bassWith({BassPattern::sustained}, energySettings(50), seed);
        for (const NoteEvent& e : sustained.lanes.at(1).events)
        {
            const int pc = chordRootPc(sustained.key, chordAt(sustained, e.start).symbol);
            REQUIRE(e.pitch == expectedRootNote(pc));
        }
        const Idea idea = bassWith({BassPattern::rollingOffbeat}, energySettings(50), seed);
        const PlacedChord* previous = nullptr;
        for (const NoteEvent& e : idea.lanes.at(1).events)
        {
            const PlacedChord& chord = chordAt(idea, e.start);
            if (&chord != previous)
            {
                REQUIRE(e.pitch == expectedRootNote(chordRootPc(idea.key, chord.symbol)));
                previous = &chord;
            }
        }
    }
}

namespace
{
constexpr std::array kApproachPatterns = {BassPattern::rollingOffbeat, BassPattern::offbeat,
                                          BassPattern::octaveBounce, BassPattern::garageSkip};
} // namespace

TEST_CASE("above half energy every chord change has a bass approach note on the last swung 16th",
          "[generate][bass][approach]")
{
    for (const BassPattern pattern : kApproachPatterns)
    {
        for (std::uint32_t seed = 1; seed <= 30; ++seed)
        {
            const Idea idea = bassWith({pattern}, energySettings(51), seed);
            REQUIRE(validate(idea).empty());
            for (const int change : changeTicks(idea))
            {
                INFO("pattern " << static_cast<int>(pattern) << " seed " << seed << " change "
                                << change);
                const NoteEvent* approach = noteStartingAt(idea, approachTick(idea, change));
                REQUIRE(approach != nullptr);
                REQUIRE(std::abs(approach->pitch - bassTarget(idea, change).pitch) <= 2);
                REQUIRE(approach->pitch != bassTarget(idea, change).pitch);
                REQUIRE(approach->pitch >= kBassLow);
                REQUIRE(approach->pitch <= kBassHigh);
            }
        }
    }
}

TEST_CASE("every approach note is in the key scale", "[generate][bass][approach]")
{
    for (const BassPattern pattern : kApproachPatterns)
    {
        for (std::uint32_t seed = 1; seed <= 100; ++seed)
        {
            const Idea idea = bassWith({pattern}, energySettings(100), seed);
            const std::vector<int> scale = scalePitchClasses(idea.key);
            for (const int change : changeTicks(idea))
            {
                const NoteEvent* approach = noteStartingAt(idea, approachTick(idea, change));
                REQUIRE(approach != nullptr);
                REQUIRE(contains(scale, approach->pitch % 12));
            }
        }
    }
}

TEST_CASE("at half energy the bass has no approach notes", "[generate][bass][approach]")
{
    for (std::uint32_t seed = 1; seed <= 30; ++seed)
    {
        const Idea idea = bassWith({BassPattern::offbeat}, energySettings(50), seed);
        for (const int change : changeTicks(idea))
        {
            REQUIRE(noteStartingAt(idea, approachTick(idea, change)) == nullptr);
        }
    }
}

TEST_CASE("sustained and pedal bass get no approach notes", "[generate][bass][approach]")
{
    for (const BassPattern pattern : {BassPattern::sustained, BassPattern::pedal})
    {
        for (std::uint32_t seed = 1; seed <= 30; ++seed)
        {
            const Idea idea = bassWith({pattern}, energySettings(100), seed);
            for (const int change : changeTicks(idea))
            {
                REQUIRE(noteStartingAt(idea, approachTick(idea, change)) == nullptr);
            }
        }
    }
}

TEST_CASE("the note held into an approach note is shortened to end where it starts",
          "[generate][bass][approach]")
{
    for (std::uint32_t seed = 1; seed <= 30; ++seed)
    {
        const Idea idea = bassWith({BassPattern::offbeat}, energySettings(100), seed);
        const auto& events = idea.lanes.at(1).events;
        for (const int change : changeTicks(idea))
        {
            const auto approach =
                std::ranges::find(events, approachTick(idea, change), &NoteEvent::start);
            REQUIRE(approach != events.end());
            REQUIRE(approach != events.begin());
            const NoteEvent& before = *std::prev(approach);
            REQUIRE(before.length > 0);
            REQUIRE(before.start + before.length == approach->start);
        }
    }
}

TEST_CASE("the bar 8 approach note ends at the idea's end", "[generate][bass][approach]")
{
    for (const BassPattern pattern : kApproachPatterns)
    {
        for (std::uint32_t seed = 1; seed <= 30; ++seed)
        {
            const Idea idea = bassWith({pattern}, energySettings(100), seed);
            const NoteEvent& last = idea.lanes.at(1).events.back();
            REQUIRE(last.start == approachTick(idea, idea.settings.bars * kTicksPerBar));
            REQUIRE(last.length > 0);
            REQUIRE(last.start + last.length <= ideaLengthTicks(idea));
        }
    }
}

#include "md/generate.hpp"

#include "md/bank.hpp"
#include "md/ranges.hpp"
#include "md/types.hpp"
#include "md/validate.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>
#include <set>
#include <utility>
#include <vector>

using namespace md;

namespace
{
constexpr int kSeeds = 50;
constexpr std::uint32_t kNextSeedOffset = 1000;

const Bank& bank()
{
    static const Bank loaded = loadBundledBank();
    return loaded;
}

const Lane& laneOf(const Idea& idea, LaneId id)
{
    return idea.lanes.at(static_cast<std::size_t>(id));
}

Idea first(const Settings& settings, std::uint32_t seed)
{
    return generate(bank(), settings, seed, defaultLaneSeeds(seed));
}

Idea next(const Settings& settings, std::uint32_t seed, const Idea& previous, Pins pins)
{
    return generate(bank(), settings, seed, defaultLaneSeeds(seed), previous, pins);
}

bool sameProgression(const Idea& a, const Idea& b)
{
    if (a.progression.size() != b.progression.size() || a.entryId != b.entryId ||
        a.parallel != b.parallel)
    {
        return false;
    }
    for (std::size_t i = 0; i < a.progression.size(); ++i)
    {
        const PlacedChord& x = a.progression.at(i);
        const PlacedChord& y = b.progression.at(i);
        if (!(x.symbol == y.symbol) || x.startBar != y.startBar || x.bars != y.bars ||
            x.voicing != y.voicing)
        {
            return false;
        }
    }
    return true;
}

bool sameLane(const Idea& a, const Idea& b, LaneId id)
{
    const Lane& x = laneOf(a, id);
    const Lane& y = laneOf(b, id);
    return x.pattern == y.pattern && x.events == y.events && x.seed == y.seed;
}

bool sameStartsAndLengths(const Idea& a, const Idea& b, LaneId id)
{
    const auto& x = laneOf(a, id).events;
    const auto& y = laneOf(b, id).events;
    if (x.size() != y.size())
    {
        return false;
    }
    for (std::size_t i = 0; i < x.size(); ++i)
    {
        if (x.at(i).start != y.at(i).start || x.at(i).length != y.at(i).length)
        {
            return false;
        }
    }
    return true;
}

// The distinct (start, length) pairs of the chord lane, whatever the voicing sizes.
std::set<std::pair<int, int>> onsets(const Idea& idea)
{
    std::set<std::pair<int, int>> result;
    for (const NoteEvent& e : laneOf(idea, LaneId::chords).events)
    {
        result.emplace(e.start, e.length);
    }
    return result;
}

bool samePitches(const Idea& a, const Idea& b, LaneId id)
{
    const auto& x = laneOf(a, id).events;
    const auto& y = laneOf(b, id).events;
    for (std::size_t i = 0; i < x.size() && i < y.size(); ++i)
    {
        if (x.at(i).pitch != y.at(i).pitch)
        {
            return false;
        }
    }
    return true;
}
} // namespace

TEST_CASE("with no pins the new overload equals the old one", "[pins]")
{
    for (std::uint32_t seed = 1; seed <= kSeeds; ++seed)
    {
        const Settings settings;
        const Idea previous = first(settings, seed + 7);
        const Idea a = generate(bank(), settings, seed, defaultLaneSeeds(seed));
        const Idea b = next(settings, seed, previous, {});
        CHECK(sameProgression(a, b));
        for (const LaneId id : {LaneId::chords, LaneId::bass, LaneId::melody})
        {
            CHECK(sameLane(a, b, id));
            CHECK(laneOf(b, id).pinned == false);
        }
        CHECK(a.seed == b.seed);
        CHECK(a.key.mode == b.key.mode);
    }
}

TEST_CASE("pinned chords survive sprinkle and bass and melody are rewritten", "[pins]")
{
    int rewritten = 0;
    for (std::uint32_t seed = 1; seed <= kSeeds; ++seed)
    {
        const Settings settings;
        const Idea previous = first(settings, seed);
        const Idea result = next(settings, seed + kNextSeedOffset, previous, {.chords = true});
        CHECK(sameProgression(previous, result));
        CHECK(sameLane(previous, result, LaneId::chords));
        CHECK(laneOf(result, LaneId::chords).pinned);
        CHECK(validate(result).empty());
        if (!sameLane(previous, result, LaneId::bass) &&
            !sameLane(previous, result, LaneId::melody))
        {
            ++rewritten;
        }
    }
    CHECK(rewritten >= 45);
}

TEST_CASE("pinned chords survive a flavour change", "[pins]")
{
    for (std::uint32_t seed = 1; seed <= kSeeds; ++seed)
    {
        Settings settings;
        settings.flavour = Flavour::soulful;
        const Idea previous = first(settings, seed);
        settings.flavour = Flavour::jazzy;
        const Idea result = next(settings, seed, previous, {.chords = true});
        CHECK(sameProgression(previous, result));
        CHECK(sameLane(previous, result, LaneId::chords));
        CHECK(validate(result).empty());
    }
}

TEST_CASE("a pinned bass keeps its starts and lengths and is refitted", "[pins]")
{
    for (std::uint32_t seed = 1; seed <= kSeeds; ++seed)
    {
        const Settings settings;
        const Idea previous = first(settings, seed);
        const Idea result = next(settings, seed + kNextSeedOffset, previous, {.bass = true});
        CHECK(sameStartsAndLengths(previous, result, LaneId::bass));
        CHECK(validate(result).empty());
        CHECK(laneOf(result, LaneId::bass).pinned);
        CHECK(laneOf(result, LaneId::bass).seed == laneOf(previous, LaneId::bass).seed);
        if (!samePitches(previous, result, LaneId::bass))
        {
            CHECK(laneOf(result, LaneId::bass).refitted);
        }
    }
}

TEST_CASE("a pinned bass survives three sprinkles in a row", "[pins]")
{
    for (std::uint32_t seed = 1; seed <= kSeeds; ++seed)
    {
        const Settings settings;
        const Idea original = first(settings, seed);
        Idea current = original;
        for (std::uint32_t round = 1; round <= 3; ++round)
        {
            current = next(settings, seed + (round * kNextSeedOffset), current, {.bass = true});
            CHECK(sameStartsAndLengths(original, current, LaneId::bass));
            CHECK(validate(current).empty());
        }
    }
}

TEST_CASE("a pinned melody keeps its starts and lengths and is refitted", "[pins]")
{
    for (std::uint32_t seed = 1; seed <= kSeeds; ++seed)
    {
        const Settings settings;
        const Idea previous = first(settings, seed);
        const Idea result = next(settings, seed + kNextSeedOffset, previous, {.melody = true});
        CHECK(sameStartsAndLengths(previous, result, LaneId::melody));
        CHECK(validate(result).empty());
        CHECK(laneOf(result, LaneId::melody).pinned);
        if (!samePitches(previous, result, LaneId::melody))
        {
            CHECK(laneOf(result, LaneId::melody).refitted);
        }
    }
}

TEST_CASE("everything pinned changes nothing", "[pins]")
{
    for (std::uint32_t seed = 1; seed <= kSeeds; ++seed)
    {
        const Settings settings;
        const Idea previous = first(settings, seed);
        const Idea result = next(settings, seed + kNextSeedOffset, previous,
                                 {.chords = true, .bass = true, .melody = true});
        CHECK(sameProgression(previous, result));
        CHECK(result.seed == previous.seed);
        for (const LaneId id : {LaneId::chords, LaneId::bass, LaneId::melody})
        {
            CHECK(sameLane(previous, result, id));
        }
    }
}

TEST_CASE("a key change moves pinned chords and keeps the bass valid", "[pins]")
{
    for (std::uint32_t seed = 1; seed <= kSeeds; ++seed)
    {
        Settings settings;
        settings.keyRoot = 9;
        const Idea previous = first(settings, seed);
        settings.keyRoot = 0;
        const Idea result =
            next(settings, seed + kNextSeedOffset, previous, {.chords = true, .bass = true});
        CHECK(result.key.root == 0);
        CHECK(validate(result).empty());
        REQUIRE(result.progression.size() == previous.progression.size());
        for (std::size_t i = 0; i < result.progression.size(); ++i)
        {
            std::set<int> moved;
            for (const int pitch : previous.progression.at(i).voicing)
            {
                moved.insert((pitch + 3) % 12);
            }
            std::set<int> actual;
            for (const int pitch : result.progression.at(i).voicing)
            {
                actual.insert(pitch % 12);
            }
            CHECK(actual == moved);
        }
        for (const NoteEvent& e : laneOf(result, LaneId::bass).events)
        {
            CHECK(e.pitch >= kBassLow);
            CHECK(e.pitch <= kBassHigh);
        }
    }
}

TEST_CASE("a bar count change ignores the pins", "[pins]")
{
    for (std::uint32_t seed = 1; seed <= kSeeds; ++seed)
    {
        Settings settings;
        settings.bars = 8;
        const Idea previous = first(settings, seed);
        settings.bars = 4;
        const Pins all{.chords = true, .bass = true, .melody = true};
        const Idea result = next(settings, seed + kNextSeedOffset, previous, all);
        const Idea plain = first(settings, seed + kNextSeedOffset);
        CHECK(sameProgression(plain, result));
        for (const LaneId id : {LaneId::chords, LaneId::bass, LaneId::melody})
        {
            CHECK(sameLane(plain, result, id));
            CHECK_FALSE(laneOf(result, id).pinned);
        }
    }
}

TEST_CASE("an empty previous idea ignores the pins", "[pins]")
{
    const Settings settings;
    const Pins all{.chords = true, .bass = true, .melody = true};
    const Idea result = next(settings, 5, Idea{}, all);
    const Idea plain = first(settings, 5);
    CHECK(sameProgression(plain, result));
    for (const LaneId id : {LaneId::chords, LaneId::bass, LaneId::melody})
    {
        CHECK(sameLane(plain, result, id));
        CHECK_FALSE(laneOf(result, id).pinned);
    }
}

TEST_CASE("pinned chords revoice when no shift keeps the voicings valid", "[pins]")
{
    int revoiced = 0;
    for (std::uint32_t seed = 1; seed <= kSeeds; ++seed)
    {
        Settings settings;
        settings.keyRoot = 9;
        Idea previous = first(settings, seed);
        if (previous.progression.size() < 2)
        {
            continue;
        }
        // Tops at both ends of the allowed span: no whole-progression shift of 3, -9 or 15 fits.
        const auto moveTop = [](PlacedChord& chord, int top)
        {
            const int delta = top - chord.voicing.back();
            for (int& pitch : chord.voicing)
            {
                pitch += delta;
            }
        };
        moveTop(previous.progression.front(), kChordTopLow);
        moveTop(previous.progression.back(), kChordTopHigh);
        settings.keyRoot = 0;
        const Idea result = next(settings, seed + kNextSeedOffset, previous, {.chords = true});
        CHECK(validate(result).empty());
        CHECK(onsets(previous) == onsets(result));
        if (result.progression.front().voicing.back() != kChordTopLow + 3)
        {
            ++revoiced;
        }
    }
    CHECK(revoiced > 0);
}

TEST_CASE("pinned chords keep their rhythm across a key change", "[pins]")
{
    for (std::uint32_t seed = 1; seed <= kSeeds; ++seed)
    {
        Settings settings;
        settings.keyRoot = 9;
        const Idea previous = first(settings, seed);
        settings.keyRoot = 2;
        const Idea result = next(settings, seed + kNextSeedOffset, previous, {.chords = true});
        CHECK(sameStartsAndLengths(previous, result, LaneId::chords));
        CHECK(validate(result).empty());
    }
}

TEST_CASE("a pinned melody survives a key change", "[pins]")
{
    for (std::uint32_t seed = 1; seed <= kSeeds; ++seed)
    {
        Settings settings;
        settings.keyRoot = 9;
        const Idea previous = first(settings, seed);
        settings.keyRoot = 4;
        const Idea result = next(settings, seed + kNextSeedOffset, previous, {.melody = true});
        CHECK(sameStartsAndLengths(previous, result, LaneId::melody));
        CHECK(validate(result).empty());
    }
}

TEST_CASE("pinned chords survive a style change", "[pins]")
{
    for (std::uint32_t seed = 1; seed <= kSeeds; ++seed)
    {
        Settings settings;
        settings.style = Style::soulful;
        const Idea previous = first(settings, seed);
        settings.style = Style::bassline;
        const Idea result = next(settings, seed + kNextSeedOffset, previous, {.chords = true});
        CHECK(sameProgression(previous, result));
        CHECK(sameLane(previous, result, LaneId::chords));
        CHECK(validate(result).empty());
    }
}

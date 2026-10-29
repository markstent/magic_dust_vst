#include "md/generate.hpp"

#include "internal.hpp"
#include "md/bank.hpp"
#include "md/ranges.hpp"
#include "md/theory.hpp"
#include "md/types.hpp"
#include "md/validate.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

using namespace md;

namespace
{
const Bank& bank()
{
    static const Bank loaded = loadBundledBank();
    return loaded;
}

Lane& bassLane(Idea& idea) { return idea.lanes.at(static_cast<std::size_t>(LaneId::bass)); }

Settings settingsFor(Style style)
{
    Settings settings;
    settings.style = style;
    settings.energy = 80;
    return settings;
}

// A's bass played under B's chords and melody.
Idea bassUnderOtherChords(Style style, std::uint32_t a, std::uint32_t b)
{
    Idea idea = generate(bank(), settingsFor(style), a);
    const Idea other = generate(bank(), settingsFor(style), b);
    idea.progression = other.progression;
    idea.borrowedFrom = other.borrowedFrom;
    idea.parallel = other.parallel;
    idea.lanes.at(static_cast<std::size_t>(LaneId::chords)) =
        other.lanes.at(static_cast<std::size_t>(LaneId::chords));
    idea.lanes.at(static_cast<std::size_t>(LaneId::melody)) =
        other.lanes.at(static_cast<std::size_t>(LaneId::melody));
    return idea;
}

struct Mismatch
{
    Idea idea;
    bool found = false;
};

Mismatch findMismatch()
{
    for (std::uint32_t a = 1; a < 60; ++a)
    {
        Idea idea = bassUnderOtherChords(Style::ukFunky, a, a + 100);
        if (!validate(idea).empty())
        {
            return {.idea = idea, .found = true};
        }
    }
    return {};
}

const Mismatch& firstMismatch()
{
    static const Mismatch cached = findMismatch();
    return cached;
}

NoteEvent noteAt(int pitch, int start)
{
    return {.pitch = pitch, .start = start, .length = 50, .velocity = 90};
}

Settings settingsWithKey(std::uint32_t seed)
{
    Settings settings = settingsFor(Style::ukFunky);
    settings.keyRoot = static_cast<int>(seed % 12);
    return settings;
}
} // namespace

TEST_CASE("a valid bass is not refitted", "[refit]")
{
    Idea idea = generate(bank(), settingsFor(Style::ukFunky), 7);
    const std::vector<NoteEvent> before = bassLane(idea).events;
    CHECK_FALSE(refitBass(idea));
    CHECK_FALSE(bassLane(idea).refitted);
    CHECK(bassLane(idea).events == before);
}

TEST_CASE("a bass under new chords is refitted and valid with the same rhythm", "[refit]")
{
    const Mismatch& m = firstMismatch();
    REQUIRE(m.found);
    Idea idea = m.idea;
    const std::vector<NoteEvent> before = bassLane(idea).events;

    CHECK(refitBass(idea));
    CHECK(bassLane(idea).refitted);
    CHECK(validate(idea).empty());
    REQUIRE(bassLane(idea).events.size() == before.size());
    for (std::size_t i = 0; i < before.size(); ++i)
    {
        CHECK(bassLane(idea).events.at(i).start == before.at(i).start);
        CHECK(bassLane(idea).events.at(i).length == before.at(i).length);
        CHECK(bassLane(idea).events.at(i).velocity == before.at(i).velocity);
    }
}

TEST_CASE("the refit is deterministic and settles", "[refit]")
{
    const Mismatch& m = firstMismatch();
    REQUIRE(m.found);
    Idea first = m.idea;
    Idea second = m.idea;
    refitBass(first);
    refitBass(second);
    CHECK(bassLane(first).events == bassLane(second).events);
    CHECK_FALSE(refitBass(first));
}

TEST_CASE("a chord tone wins over a nearer scale tone", "[refit]")
{
    for (std::uint32_t seed = 1; seed < 60; ++seed)
    {
        Idea idea = generate(bank(), settingsWithKey(seed), seed);
        const NoteEvent top{
            .pitch = kBassHigh, .start = 2 * kTicksPerSixteenth, .length = 100, .velocity = 90};
        const auto sounding = detail::soundingPcsAt(idea, top.start);
        if (std::ranges::find(sounding, top.pitch % 12) != sounding.end() ||
            !detail::bassPitchOk(idea, top, {}))
        {
            continue;
        }
        bassLane(idea).events = {{.pitch = kBassHigh + 12,
                                  .start = top.start,
                                  .length = top.length,
                                  .velocity = top.velocity}};
        REQUIRE(refitBass(idea));
        const int pitch = bassLane(idea).events.front().pitch;
        CHECK(std::ranges::find(sounding, pitch % 12) != sounding.end());
        CHECK(pitch < kBassHigh);
        return;
    }
    FAIL("no seed puts a scale tone at the top of the range");
}

TEST_CASE("a tie between two chord tones goes down", "[refit]")
{
    for (std::uint32_t seed = 1; seed < 60; ++seed)
    {
        Idea idea = generate(bank(), settingsWithKey(seed), seed);
        const int start = 2 * kTicksPerSixteenth;
        for (int pitch = kBassLow + 1; pitch < kBassHigh; ++pitch)
        {
            const auto ok = [&](int p) { return detail::bassPitchOk(idea, noteAt(p, start), {}); };
            const auto sounding = detail::soundingPcsAt(idea, start);
            const auto tone = [&](int p)
            { return ok(p) && std::ranges::find(sounding, p % 12) != sounding.end(); };
            for (int d = 1; d <= 6 && pitch - d >= kBassLow && pitch + d <= kBassHigh; ++d)
            {
                bool nearer = false;
                for (int k = 0; k < d; ++k)
                {
                    nearer = nearer || tone(pitch - k) || tone(pitch + k);
                }
                if (!nearer && tone(pitch - d) && tone(pitch + d) && !ok(pitch))
                {
                    bassLane(idea).events = {noteAt(pitch, start)};
                    REQUIRE(refitBass(idea));
                    CHECK(bassLane(idea).events.front().pitch == pitch - d);
                    return;
                }
            }
        }
    }
    FAIL("no seed has a failing note between two chord tones");
}

TEST_CASE("with no chord sounding the refit falls back to the nearest scale tone", "[refit]")
{
    Idea idea = generate(bank(), settingsFor(Style::ukFunky), 3);
    idea.progression.clear();
    const auto scale = scalePitchClasses(idea.key);
    int expected = kBassHigh;
    while (std::ranges::find(scale, expected % 12) == scale.end())
    {
        --expected;
    }
    bassLane(idea).events = {noteAt(kBassHigh + 12, 2 * kTicksPerSixteenth)};
    REQUIRE(refitBass(idea));
    CHECK(bassLane(idea).events.front().pitch == expected);
}

TEST_CASE("a refit that removes an approach exemption takes a second pass", "[refit]")
{
    for (std::uint32_t seed = 1; seed < 200; ++seed)
    {
        Idea idea = generate(bank(), settingsWithKey(seed), seed);
        if (idea.progression.size() < 2)
        {
            continue;
        }
        const int change = idea.progression.at(1).startBar * kTicksPerBar;
        const int approachStart =
            detail::swung(change - kTicksPerSixteenth, swingPermille(idea.settings.groove));
        // The approach note clashes and is exempt only because its target is within 2 semitones.
        bassLane(idea).events = {noteAt(kBassHigh, approachStart), noteAt(kBassHigh + 2, change)};
        const auto approaches = detail::approachStarts(idea);
        const NoteEvent approach = bassLane(idea).events.front();
        if (!detail::bassClashes(idea, approach, {}) ||
            !detail::bassPitchOk(idea, approach, approaches))
        {
            continue;
        }
        REQUIRE(refitBass(idea));
        if (bassLane(idea).events.front().pitch == kBassHigh)
        {
            continue; // the refit left the approach note exempt, so there was no second pass
        }
        const auto after = detail::approachStarts(idea);
        for (const NoteEvent& e : bassLane(idea).events)
        {
            CHECK(detail::bassPitchOk(idea, e, after));
        }
        return;
    }
    FAIL("no seed gives an approach note that clashes only by exemption");
}

namespace
{
Lane& melodyLane(Idea& idea) { return idea.lanes.at(static_cast<std::size_t>(LaneId::melody)); }

// A's melody played under B's chords.
Idea melodyUnderOtherChords(std::uint32_t a, std::uint32_t b)
{
    Idea idea = bassUnderOtherChords(Style::ukFunky, a, b);
    melodyLane(idea) = generate(bank(), settingsFor(Style::ukFunky), a)
                           .lanes.at(static_cast<std::size_t>(LaneId::melody));
    return idea;
}

Idea firstMelodyMismatch()
{
    for (std::uint32_t a = 1; a < 60; ++a)
    {
        Idea idea = melodyUnderOtherChords(a, a + 100);
        Idea fitted = idea;
        if (refitMelody(fitted))
        {
            return idea;
        }
    }
    FAIL("no seed puts a melody under chords it fails against");
    return {};
}
} // namespace

TEST_CASE("a valid melody is not refitted", "[refit]")
{
    Idea idea = generate(bank(), settingsFor(Style::ukFunky), 7);
    const std::vector<NoteEvent> before = melodyLane(idea).events;
    CHECK_FALSE(refitMelody(idea));
    CHECK_FALSE(melodyLane(idea).refitted);
    CHECK(melodyLane(idea).events == before);
}

TEST_CASE("a melody under foreign chords is refitted and valid with the same rhythm", "[refit]")
{
    Idea idea = firstMelodyMismatch();
    const std::vector<NoteEvent> before = melodyLane(idea).events;

    CHECK(refitMelody(idea));
    CHECK(melodyLane(idea).refitted);
    refitBass(idea);
    CHECK(validate(idea).empty());
    REQUIRE(melodyLane(idea).events.size() == before.size());
    for (std::size_t i = 0; i < before.size(); ++i)
    {
        CHECK(melodyLane(idea).events.at(i).start == before.at(i).start);
        CHECK(melodyLane(idea).events.at(i).length == before.at(i).length);
        CHECK(melodyLane(idea).events.at(i).velocity == before.at(i).velocity);
    }
}

TEST_CASE("the melody refit is deterministic and settles", "[refit]")
{
    Idea first = firstMelodyMismatch();
    Idea second = first;
    refitMelody(first);
    refitMelody(second);
    CHECK(melodyLane(first).events == melodyLane(second).events);
    CHECK_FALSE(refitMelody(first));
}

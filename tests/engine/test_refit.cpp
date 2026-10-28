#include "md/generate.hpp"

#include "internal.hpp"
#include "md/bank.hpp"
#include "md/ranges.hpp"
#include "md/types.hpp"
#include "md/validate.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
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

// A's bass played under B's chords.
Idea bassUnderOtherChords(Style style, std::uint32_t a, std::uint32_t b)
{
    Idea idea = generate(bank(), settingsFor(style), a);
    const Idea other = generate(bank(), settingsFor(style), b);
    idea.progression = other.progression;
    idea.borrowedFrom = other.borrowedFrom;
    idea.parallel = other.parallel;
    idea.lanes.at(static_cast<std::size_t>(LaneId::chords)) =
        other.lanes.at(static_cast<std::size_t>(LaneId::chords));
    return idea;
}

struct Mismatch
{
    Idea idea;
    bool found = false;
};

Mismatch firstMismatch()
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
    const Mismatch m = firstMismatch();
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
    const Mismatch m = firstMismatch();
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
        Settings settings = settingsFor(Style::ukFunky);
        settings.keyRoot = static_cast<int>(seed % 12);
        Idea idea = generate(bank(), settings, seed);
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

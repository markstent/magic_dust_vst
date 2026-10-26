#include "md/validate.hpp"

#include "md/ranges.hpp"
#include "md/types.hpp"

#include <catch2/catch_test_macros.hpp>

#include <string>
#include <vector>

using namespace md;

namespace
{
NoteEvent note(int pitch, int start, int length)
{
    return {.pitch = pitch, .start = start, .length = length, .velocity = 90};
}

// Four 2-bar min7 chords on the key root, one chord hit, one bass note and one melody note each.
Idea validIdea(int root)
{
    Idea idea{};
    idea.seed = 1;
    idea.settings.keyRoot = root;
    idea.settings.bars = 8;
    idea.key = {.root = root, .mode = Mode::aeolian};
    const int low = 56 + ((root + 12 - (56 % 12)) % 12);
    const int bass = 33 + ((root + 12 - (33 % 12)) % 12);
    for (const auto id : {LaneId::chords, LaneId::bass, LaneId::melody})
    {
        idea.lanes.at(static_cast<std::size_t>(id)).id = id;
    }
    for (int slot = 0; slot < 4; ++slot)
    {
        const int start = slot * 2 * kTicksPerBar;
        const Voicing voicing = {low, low + 3, low + 7, low + 10};
        const PlacedChord chord{
            .symbol = {.degree = 1, .quality = Quality::min7},
            .feel = "home",
            .startBar = slot * 2,
            .bars = 2,
            .voicing = voicing,
        };
        idea.progression.push_back(chord);
        for (const int pitch : voicing)
        {
            idea.lanes.at(0).events.push_back(note(pitch, start, kTicksPerQuarter));
        }
        idea.lanes.at(1).events.push_back(note(bass, start, 480));
        idea.lanes.at(2).events.push_back(note(60 + root, start, 480));
    }
    return idea;
}

void requireOneViolation(const Idea& idea)
{
    const auto violations = validate(idea);
    const std::string first = violations.empty() ? "none" : violations.front();
    INFO(first);
    REQUIRE(violations.size() == 1);
}
} // namespace

TEST_CASE("a valid hand-built idea has no violations", "[validate]")
{
    REQUIRE(validate(validIdea(9)).empty());
    REQUIRE(validate(validIdea(8)).empty());
}

TEST_CASE("a chord note above the range is one violation", "[validate]")
{
    Idea idea = validIdea(8);
    idea.lanes.at(0).events.at(0).pitch = 80;
    requireOneViolation(idea);
}

TEST_CASE("a bass note below the range is one violation", "[validate]")
{
    Idea idea = validIdea(8);
    idea.lanes.at(1).events.at(0).pitch = 27;
    requireOneViolation(idea);
}

TEST_CASE("a melody note above the range is one violation", "[validate]")
{
    Idea idea = validIdea(8);
    idea.lanes.at(2).events.at(0).pitch = 85;
    requireOneViolation(idea);
}

TEST_CASE("overlapping bass notes are one violation", "[validate]")
{
    Idea idea = validIdea(9);
    idea.lanes.at(1).events.at(0).length = 480;
    idea.lanes.at(1).events.insert(idea.lanes.at(1).events.begin() + 1,
                                   note(idea.lanes.at(1).events.at(0).pitch, 240, 480));
    requireOneViolation(idea);
}

TEST_CASE("a melody note out of key and out of the chord is one violation", "[validate]")
{
    Idea idea = validIdea(9);
    idea.lanes.at(2).events.at(0).pitch = 61;
    requireOneViolation(idea);
}

TEST_CASE("an out of key melody note that is a chord tone is accepted", "[validate]")
{
    Idea idea = validIdea(9);
    idea.progression.at(0).symbol = {.degree = 1, .quality = Quality::maj7};
    idea.lanes.at(2).events.at(0).pitch = 61; // C#, not in A aeolian but in Amaj7
    REQUIRE(validate(idea).empty());
}

TEST_CASE("a chord note outside the scale and the borrowed scale is one violation", "[validate]")
{
    Idea idea = validIdea(9);
    idea.lanes.at(0).events.at(0).pitch = 61;
    requireOneViolation(idea);
    idea.borrowedFrom = Mode::dorian;
    requireOneViolation(idea); // C# is in neither aeolian nor dorian
    idea.lanes.at(0).events.at(0).pitch = 66;
    REQUIRE(validate(idea).empty()); // F# is in A dorian
}

TEST_CASE("a voice that moves 8 semitones is a violation unless the idea is parallel", "[validate]")
{
    Idea idea = validIdea(9);
    idea.progression.back().voicing.back() += 8;
    requireOneViolation(idea);
    idea.parallel = true;
    REQUIRE(validate(idea).empty());
}

TEST_CASE("a voicing outside the range, with a bad top note or muddy, is a violation", "[validate]")
{
    Idea idea = validIdea(9);
    idea.parallel = true; // a jump from the previous voicing is not under test here
    idea.progression.back().voicing = {57, 60, 64, 80};
    requireOneViolation(idea); // one line per voicing, even though the top note is also too high
    idea = validIdea(9);
    idea.parallel = true;
    idea.progression.back().voicing = {49, 53, 57, 66}; // a third below E3
    requireOneViolation(idea);
    idea = validIdea(9);
    idea.parallel = true;
    idea.progression.back().voicing = {52, 55, 59, 62}; // top 62 is fine, E3 start is not muddy
    REQUIRE(validate(idea).empty());
}

TEST_CASE("a note that ends after the idea is one violation", "[validate]")
{
    Idea idea = validIdea(9);
    idea.lanes.at(1).events.push_back(note(33, (idea.settings.bars * kTicksPerBar) - 240, 480));
    requireOneViolation(idea);
}

TEST_CASE("a progression whose bars do not sum to the idea bars is one violation", "[validate]")
{
    Idea idea = validIdea(9);
    idea.progression.back().bars = 1;
    requireOneViolation(idea);
}

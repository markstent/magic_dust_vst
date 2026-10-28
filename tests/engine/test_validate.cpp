#include "md/validate.hpp"

#include "internal.hpp"
#include "md/names.hpp"
#include "md/ranges.hpp"
#include "md/types.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
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
    idea.lanes.at(1).pattern = std::string(bassPatternName(BassPattern::sustained));
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

// Moves a melody note off the quarter grid, so a test of another rule does not also trip the
// strong-beat rule.
void offBeat(Idea& idea, std::size_t index = 0) { idea.lanes.at(2).events.at(index).start += 240; }

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
    idea.lanes.at(2).events = {note(85, 240, 480)}; // a lone note, so no leap either
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
    idea.lanes.at(2).events.at(0).pitch = 70; // A#, not in A aeolian or the chord
    offBeat(idea);
    requireOneViolation(idea);
}

TEST_CASE("a melody leap over 7 semitones that is not an octave is one violation", "[validate]")
{
    Idea idea = validIdea(9);
    idea.lanes.at(2).events.at(3).pitch = 79; // 10 semitones above A4, a G in key
    requireOneViolation(idea);
    idea.lanes.at(2).events.at(3).pitch = 81; // an octave above A4 is allowed
    REQUIRE(validate(idea).empty());
}

TEST_CASE("a clashing melody note on a quarter-note start is one violation", "[validate]")
{
    Idea idea = validIdea(9);
    idea.progression.at(0).symbol = {.degree = 1, .quality = Quality::min9}; // A C E G B
    idea.lanes.at(2).events.at(0).pitch = 71; // B sits a semitone under C
    requireOneViolation(idea);
    idea.lanes.at(2).events.at(0).start += 240; // off the quarter grid, so it may clash
    REQUIRE(validate(idea).empty());
}

TEST_CASE("an out of key melody note that is a chord tone is accepted", "[validate]")
{
    Idea idea = validIdea(9);
    idea.progression.at(0).symbol = {.degree = 4, .quality = Quality::maj}; // D F# A
    idea.borrowedFrom = Mode::dorian;
    idea.lanes.at(2).events.at(0).pitch = 66; // F#, not in A aeolian but in the D chord
    REQUIRE(validate(idea).empty());
}

TEST_CASE("a parallel idea accepts melody notes from the sounding voicing", "[validate]")
{
    Idea idea = validIdea(9);
    idea.parallel = true;
    idea.progression.at(0).voicing = {57, 60, 64, 66};
    idea.lanes.at(0).events.at(3).pitch = 66;
    idea.lanes.at(2).events.at(0).pitch = 66; // F# is not in A aeolian or Am7, but it sounds
    REQUIRE(validate(idea).empty());
    idea.lanes.at(2).events.at(0).pitch = 70;
    offBeat(idea);
    requireOneViolation(idea);
}

TEST_CASE("a chord note outside the scale and the borrowed scale is one violation", "[validate]")
{
    Idea idea = validIdea(9);
    idea.progression.at(0).symbol = {.degree = 4, .quality = Quality::maj};
    idea.borrowedFrom = Mode::dorian;
    idea.progression.at(0).voicing.back() = 70;
    idea.lanes.at(0).events.at(3).pitch = 70;
    requireOneViolation(idea); // Bb is in neither aeolian nor dorian
    idea.progression.at(0).voicing.back() = 66;
    idea.lanes.at(0).events.at(3).pitch = 66;
    REQUIRE(validate(idea).empty()); // F# is in A dorian
}

TEST_CASE("a chord note that is not in its chord's voicing is one violation", "[validate]")
{
    Idea idea = validIdea(9);
    idea.lanes.at(0).events.at(3).pitch = 69;
    requireOneViolation(idea);
}

namespace
{
void dropLastChordEvents(Idea& idea)
{
    std::erase_if(idea.lanes.at(0).events,
                  [](const NoteEvent& e) { return e.start >= 6 * kTicksPerBar; });
}
} // namespace

TEST_CASE("a voice that moves 8 semitones is a violation unless the idea is parallel", "[validate]")
{
    Idea idea = validIdea(9);
    dropLastChordEvents(idea);
    idea.progression.back().voicing.back() += 8;
    requireOneViolation(idea);
    idea.parallel = true;
    REQUIRE(validate(idea).empty());
}

TEST_CASE("a voicing outside the range, with a bad top note or muddy, is a violation", "[validate]")
{
    Idea idea = validIdea(9);
    dropLastChordEvents(idea);
    offBeat(idea, 3);     // the last chord's voicings below sound against the melody on the beat
    idea.parallel = true; // a jump from the previous voicing is not under test here
    idea.progression.back().voicing = {57, 60, 64, 80};
    requireOneViolation(idea); // one line per voicing, even though the top note is also too high
    idea.progression.back().voicing = {49, 53, 57, 66}; // a third below E3
    requireOneViolation(idea);
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

TEST_CASE("a progression outside the mode is one violation", "[validate]")
{
    Idea idea = validIdea(9);
    idea.progression.at(0).symbol = {.degree = 2, .quality = Quality::maj}; // B D# F#
    offBeat(idea);
    requireOneViolation(idea);
    idea.parallel = true; // parallel entries are exempt
    REQUIRE(validate(idea).empty());
}

TEST_CASE("borrowedFrom must match the progression", "[validate]")
{
    Idea idea = validIdea(9);
    idea.borrowedFrom = Mode::dorian;
    requireOneViolation(idea);
}

TEST_CASE("settings.bars must be 4, 8 or 16", "[validate]")
{
    Idea idea = validIdea(9);
    idea.settings.bars = 5;
    requireOneViolation(idea);
}

TEST_CASE("a gap or an overlap in the chord sequence is one violation", "[validate]")
{
    Idea idea = validIdea(9);
    idea.progression.at(1).startBar = 3;
    requireOneViolation(idea); // gap
    idea = validIdea(9);
    idea.progression.at(1).startBar = 1;
    requireOneViolation(idea); // overlap
}

TEST_CASE("a note that starts before the idea is one violation", "[validate]")
{
    Idea idea = validIdea(9);
    idea.lanes.at(2).events.at(0).start = -1;
    requireOneViolation(idea);
}

TEST_CASE("a note with no length is one violation", "[validate]")
{
    Idea idea = validIdea(9);
    idea.lanes.at(2).events.at(0).length = 0;
    requireOneViolation(idea);
}

TEST_CASE("a chord with no bars is one violation", "[validate]")
{
    Idea idea = validIdea(9);
    PlacedChord empty = idea.progression.at(1);
    empty.startBar = 4;
    empty.bars = 0;
    idea.progression.insert(idea.progression.begin() + 2, empty);
    requireOneViolation(idea);
}

TEST_CASE("a top voice leap over 7 between a 4 and a 5 note chord is one violation", "[validate]")
{
    Idea idea = validIdea(9);
    const int low = idea.progression.at(0).voicing.front();
    const Voicing voicing = {low, low + 3, low + 7, low + 12, low + 19};
    idea.progression.at(3).voicing = voicing;
    auto& events = idea.lanes.at(0).events;
    events.resize(12);
    for (const int pitch : voicing)
    {
        events.push_back(note(pitch, 6 * kTicksPerBar, kTicksPerQuarter));
    }
    requireOneViolation(idea);
}

namespace
{
bool mentions(const Idea& idea, const std::string& text)
{
    const auto violations = validate(idea);
    return std::ranges::any_of(violations, [&](const std::string& line)
                               { return line.find(text) != std::string::npos; });
}

// validIdea(9) is A aeolian with Am7 (A C E G) throughout and a sustained A1 on each chord start.
Idea bassIdea(int pitch, int start, BassPattern pattern)
{
    Idea idea = validIdea(9);
    auto& lane = idea.lanes.at(1);
    lane.pattern = std::string(bassPatternName(pattern));
    lane.events = {note(pitch, start, 240)};
    return idea;
}
} // namespace

TEST_CASE("a bass note a semitone from a chord tone and not one is a clash", "[validate]")
{
    // F (41) is in A aeolian but a semitone under the E of Am7.
    const Idea idea = bassIdea(41, 2 * 240, BassPattern::offbeat);
    REQUIRE(mentions(idea, "clashes with the sounding chord"));
}

TEST_CASE("a bass chord tone never clashes", "[validate]")
{
    Idea idea = bassIdea(36, 2 * 240, BassPattern::offbeat);
    idea.progression.at(0).symbol.quality = Quality::maj7;
    idea.key = {.root = 0, .mode = Mode::ionian};
    REQUIRE_FALSE(mentions(idea, "clashes with the sounding chord"));
}

TEST_CASE("a bass onset on step 4 is a downbeat violation", "[validate]")
{
    const Idea idea = bassIdea(33, 4 * 240, BassPattern::offbeat);
    REQUIRE(mentions(idea, "downbeat"));
}

TEST_CASE("only the sustained pattern may start on step 0", "[validate]")
{
    REQUIRE(validate(bassIdea(33, 0, BassPattern::sustained)).empty());
    REQUIRE(mentions(bassIdea(33, 0, BassPattern::offbeat), "downbeat"));
}

TEST_CASE("a pedal under a chord a semitone from it is not a clash", "[validate]")
{
    Idea idea = bassIdea(33, 2 * 240, BassPattern::pedal);
    idea.key = {.root = 9, .mode = Mode::phrygian};
    idea.progression.at(0).symbol = {.degree = 2, .quality = Quality::maj};
    REQUIRE_FALSE(mentions(idea, "clashes with the sounding chord"));
    idea.lanes.at(1).pattern = std::string(bassPatternName(BassPattern::offbeat));
    REQUIRE(mentions(idea, "clashes with the sounding chord"));
}

namespace
{
constexpr const char* kClash = "clashes with the sounding chord";

// An offbeat bass note on step 2 of bar 1 over a chord of the given quality on the key's root.
Idea clashIdea(int pitch, Key key, Quality quality)
{
    Idea idea = bassIdea(pitch, 2 * 240, BassPattern::offbeat);
    idea.key = key;
    idea.progression.at(0).symbol = {.degree = 1, .quality = quality};
    return idea;
}

// Key A major with an A chord (A C# E) throughout, an offbeat bass, and G# (44, in scale and a half
// step under A) on the last swung 16th before bar 3. The first bass note is an A.
Idea approachIdea(int targetPitch)
{
    Idea idea = validIdea(9);
    idea.key = {.root = 9, .mode = Mode::ionian};
    idea.settings.groove = Groove::ukShuffle58;
    for (auto& chord : idea.progression)
    {
        chord.symbol.quality = Quality::maj;
    }
    idea.lanes.at(1).pattern = std::string(bassPatternName(BassPattern::offbeat));
    const int change = 2 * kTicksPerBar;
    const int swung = detail::swung(change - kTicksPerSixteenth, 580);
    idea.lanes.at(1).events = {note(45, 2 * 240, 240), note(44, swung, 120),
                               note(targetPitch, change + 480, 240)};
    return idea;
}
} // namespace

TEST_CASE("a bass note a semitone above a chord tone and not one is a clash", "[validate]")
{
    // Bb (46) is in A phrygian and a semitone over the A of Am7.
    REQUIRE(mentions(clashIdea(46, {.root = 9, .mode = Mode::phrygian}, Quality::min7), kClash));
}

TEST_CASE("a bass clash wraps across B and C", "[validate]")
{
    const Key cMajor = {.root = 0, .mode = Mode::ionian};
    REQUIRE(mentions(clashIdea(47, cMajor, Quality::maj), kClash));
    // Db (37) is in C phrygian and a semitone over the C of Cm.
    REQUIRE(mentions(clashIdea(37, {.root = 0, .mode = Mode::phrygian}, Quality::min), kClash));
}

TEST_CASE("a parallel idea clashes against the voicing heard, not the symbol", "[validate]")
{
    // The voicing A C# E has a C#; the symbol Am7 has a C, a semitone under it.
    Idea idea = clashIdea(37, {.root = 9, .mode = Mode::aeolian}, Quality::min7);
    idea.progression.at(0).voicing = {57, 61, 64};
    REQUIRE(mentions(idea, kClash));
    idea.parallel = true;
    REQUIRE_FALSE(mentions(idea, kClash));
}

TEST_CASE("an approach note on the last swung 16th before a chord change is not a clash",
          "[validate]")
{
    Idea idea = approachIdea(45);
    REQUIRE_FALSE(mentions(idea, kClash));
    idea.lanes.at(1).events.at(1).start -= 480;
    REQUIRE(mentions(idea, kClash));
}

TEST_CASE("an approach note more than 2 semitones from its target is a clash", "[validate]")
{
    REQUIRE(mentions(approachIdea(47), kClash));
}

TEST_CASE("the idea's end counts as a change into bar 1 for approach notes", "[validate]")
{
    Idea idea = approachIdea(45);
    auto& events = idea.lanes.at(1).events;
    events.at(1).start = detail::swung((8 * kTicksPerBar) - kTicksPerSixteenth, 580);
    events.pop_back();
    REQUIRE_FALSE(mentions(idea, kClash));
    events.front().pitch = 50;
    REQUIRE(mentions(idea, kClash));
}

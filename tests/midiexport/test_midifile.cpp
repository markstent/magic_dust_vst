#include "md/bank.hpp"
#include "md/generate.hpp"
#include "md/midifile.hpp"

#include "md/ranges.hpp"
#include "md/theory.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <vector>

using namespace md;
using Bytes = std::vector<std::uint8_t>;

namespace
{
Bytes bytes(std::initializer_list<int> values)
{
    Bytes out;
    for (const int v : values)
    {
        out.push_back(static_cast<std::uint8_t>(v));
    }
    return out;
}

bool contains(const Bytes& haystack, const Bytes& needle)
{
    return std::ranges::search(haystack, needle).begin() != haystack.end();
}

NoteEvent note(int pitch, int start, int length, int velocity)
{
    return {.pitch = pitch, .start = start, .length = length, .velocity = velocity};
}

Bytes concat(Bytes a, const Bytes& b)
{
    a.insert(a.end(), b.begin(), b.end());
    return a;
}

Bytes writeOne(std::vector<NoteEvent> notes, int tempo = 120, int total = 3840)
{
    const std::vector<MidiTrack> tracks{{.name = "Test", .notes = std::move(notes), .markers = {}}};
    return writeMidiFile(tracks, tempo, total);
}

Lane makeLane(LaneId id, NoteEvent event, bool muted)
{
    return {.id = id, .pattern = "", .events = {event}, .pinned = false, .muted = muted, .seed = 0};
}

PlacedChord placed(int degree, int startBar)
{
    return {
        .symbol = {.degree = degree, .quality = Quality::min7},
        .feel = "",
        .startBar = startBar,
        .bars = 2,
        .voicing = {},
    };
}

// The chords lane is muted.
Idea makeIdea()
{
    Idea idea{};
    idea.key = {.root = 9, .mode = Mode::aeolian};
    idea.settings.bars = 4;
    idea.progression = {placed(1, 0), placed(4, 2)};
    idea.lanes = {
        makeLane(LaneId::chords, note(57, 0, 3840, 90), true),
        makeLane(LaneId::bass, note(33, 0, 240, 100), false),
        makeLane(LaneId::melody, note(69, 480, 240, 80), false),
    };
    return idea;
}

// Golden case 0, see tests/golden/cases.hpp.
Settings goldenCase0Settings()
{
    Settings settings;
    settings.style = Style{};
    settings.keyRoot = 0;
    settings.mood = 0;
    settings.energy = 0;
    settings.groove = Groove{};
    settings.flavour = Flavour{};
    settings.bars = 8;
    return settings;
}
} // namespace

TEST_CASE("single track writes exact format 0 bytes")
{
    const Bytes expected = bytes({
        0x4D, 0x54, 0x68, 0x64, 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x01, 0x03, 0xC0, 0x4D,
        0x54, 0x72, 0x6B, 0x00, 0x00, 0x00, 0x25, 0x00, 0xFF, 0x03, 0x04, 0x54, 0x65, 0x73, 0x74,
        0x00, 0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20, 0x00, 0xFF, 0x58, 0x04, 0x04, 0x02, 0x18, 0x08,
        0x00, 0x90, 0x3C, 0x64, 0x87, 0x40, 0x80, 0x3C, 0x00, 0x96, 0x40, 0xFF, 0x2F, 0x00,
    });
    CHECK(writeOne({note(60, 0, 960, 100)}) == expected);
}

TEST_CASE("two tracks write format 1 with a conductor track")
{
    std::vector<MidiTrack> tracks{
        {.name = "A", .notes = {note(60, 0, 960, 100)}, .markers = {{0, "Am"}}},
        {.name = "B", .notes = {note(62, 0, 480, 90)}, .markers = {}, .channel = 1},
    };
    const Bytes out = writeMidiFile(tracks, 120, 3840);
    CHECK(Bytes(out.begin(), out.begin() + 14) ==
          bytes({0x4D, 0x54, 0x68, 0x64, 0, 0, 0, 6, 0, 1, 0, 3, 0x03, 0xC0}));
    // Conductor: no name, tempo, time signature, marker, end of track.
    const Bytes conductor = bytes({
        0x4D, 0x54, 0x72, 0x6B, 0,    0,    0,    0x1A, 0x00, 0xFF, 0x51, 0x03,
        0x07, 0xA1, 0x20, 0x00, 0xFF, 0x58, 0x04, 0x04, 0x02, 0x18, 0x08, 0x00,
        0xFF, 0x06, 0x02, 'A',  'm',  0x9E, 0x00, 0xFF, 0x2F, 0x00,
    });
    CHECK(Bytes(out.begin() + 14, out.begin() + 14 + static_cast<long>(conductor.size())) ==
          conductor);
    const Bytes trackA = bytes({
        0x4D, 0x54, 0x72, 0x6B, 0,    0,    0,    0x13, 0x00, 0xFF, 0x03, 0x01, 'A',  0x00,
        0x90, 0x3C, 0x64, 0x87, 0x40, 0x80, 0x3C, 0x00, 0x96, 0x40, 0xFF, 0x2F, 0x00,
    });
    const Bytes trackB = bytes({
        0x4D, 0x54, 0x72, 0x6B, 0,    0,    0,    0x13, 0x00, 0xFF, 0x03, 0x01, 'B',  0x00,
        0x91, 0x3E, 0x5A, 0x83, 0x60, 0x81, 0x3E, 0x00, 0x9A, 0x20, 0xFF, 0x2F, 0x00,
    });
    Bytes rest(out.begin() + 14 + static_cast<long>(conductor.size()), out.end());
    CHECK(rest == concat(trackA, trackB));
}

TEST_CASE("a track's channel is the low nibble of its note bytes")
{
    const std::vector<MidiTrack> tracks{
        {.name = "T", .notes = {note(60, 0, 960, 100)}, .markers = {}, .channel = 15}};
    const Bytes out = writeMidiFile(tracks, 120, 3840);
    CHECK(contains(out, bytes({0x00, 0x9F, 0x3C, 0x64, 0x87, 0x40, 0x8F, 0x3C, 0x00})));
}

TEST_CASE("note-offs precede note-ons at equal ticks, then ascending pitch")
{
    // Same pitch back to back: off at 480 must come before the next on.
    const Bytes same = writeOne({note(60, 0, 480, 100), note(60, 480, 480, 100)});
    CHECK(contains(same, bytes({0x83, 0x60, 0x80, 0x3C, 0x00, 0x00, 0x90, 0x3C, 0x64})));
    // Offs sort by pitch, and every off precedes every on.
    const Bytes mixed = writeOne({
        note(64, 0, 480, 100),
        note(60, 0, 480, 100),
        note(67, 480, 480, 100),
        note(62, 480, 480, 100),
    });
    CHECK(contains(mixed, bytes({
                              0x83,
                              0x60,
                              0x80,
                              0x3C,
                              0x00,
                              0x00,
                              0x80,
                              0x40,
                              0x00,
                              0x00,
                              0x90,
                              0x3E,
                              0x64,
                              0x00,
                              0x90,
                              0x43,
                              0x64,
                          })));
}

TEST_CASE("end of track sits at totalTicks")
{
    const Bytes out = writeOne({note(60, 0, 240, 100)});
    // note-off at 240, end of track 3600 ticks later (0x9C 0x10 0x... = 3600).
    CHECK(Bytes(out.end() - 8, out.end()) ==
          bytes({0x80, 0x3C, 0x00, 0x9C, 0x10, 0xFF, 0x2F, 0x00}));
}

TEST_CASE("tempo is integer microseconds per quarter")
{
    CHECK(contains(writeOne({note(60, 0, 240, 100)}, 124),
                   bytes({0xFF, 0x51, 0x03, 0x07, 0x62, 0x1E})));
}

TEST_CASE("writeMidiFile rejects bad input")
{
    const std::vector<MidiTrack> none;
    CHECK_THROWS_AS(writeMidiFile(none, 120, 3840), std::invalid_argument);
    CHECK_THROWS_AS(writeOne({note(60, 0, 3841, 100)}), std::invalid_argument);
    CHECK_THROWS_AS(writeOne({note(60, 3800, 100, 100)}), std::invalid_argument);
    CHECK_THROWS_AS(writeOne({note(60, 100, std::numeric_limits<int>::max(), 100)}),
                    std::invalid_argument);
    const std::vector<MidiTrack> many(255, MidiTrack{.name = "T", .notes = {}, .markers = {}});
    CHECK_THROWS_AS(writeMidiFile(many, 120, 3840), std::invalid_argument);
    CHECK_NOTHROW(writeMidiFile(std::span(many).first(254), 120, 3840));
    CHECK_THROWS_AS(writeOne({note(128, 0, 240, 100)}), std::invalid_argument);
    CHECK_THROWS_AS(writeOne({note(-1, 0, 240, 100)}), std::invalid_argument);
    CHECK_THROWS_AS(writeOne({note(60, 0, 240, 0)}), std::invalid_argument);
    CHECK_THROWS_AS(writeOne({note(60, 0, 240, 128)}), std::invalid_argument);
    CHECK_THROWS_AS(writeOne({note(60, 0, 0, 100)}), std::invalid_argument);
    CHECK_THROWS_AS(writeOne({note(60, -1, 240, 100)}), std::invalid_argument);
    CHECK_THROWS_AS(writeOne({note(60, 0, 240, 100)}, 19), std::invalid_argument);
    CHECK_THROWS_AS(writeOne({note(60, 0, 240, 100)}, 1000), std::invalid_argument);
    CHECK_NOTHROW(writeOne({note(60, 0, 240, 100)}, 20));
    CHECK_NOTHROW(writeOne({note(60, 0, 240, 100)}, 999));
    const std::vector<MidiTrack> badChannel{
        {.name = "T", .notes = {}, .markers = {}, .channel = 16}};
    CHECK_THROWS_AS(writeMidiFile(badChannel, 120, 3840), std::invalid_argument);
    const std::vector<MidiTrack> negativeChannel{
        {.name = "T", .notes = {}, .markers = {}, .channel = -1}};
    CHECK_THROWS_AS(writeMidiFile(negativeChannel, 120, 3840), std::invalid_argument);
    const std::vector<MidiTrack> badMarker{{.name = "T", .notes = {}, .markers = {{3841, "x"}}}};
    CHECK_THROWS_AS(writeMidiFile(badMarker, 120, 3840), std::invalid_argument);
}

TEST_CASE("laneTrack names lanes and marks chord starts")
{
    const Idea idea = makeIdea();
    const MidiTrack chords = laneTrack(idea, LaneId::chords);
    CHECK(chords.name == "Chords");
    CHECK(chords.notes == idea.lanes.at(0).events);
    REQUIRE(chords.markers.size() == 2);
    CHECK(chords.markers.at(0).first == 0);
    CHECK(chords.markers.at(0).second == chordName(idea.key, idea.progression.at(0).symbol));
    CHECK(chords.markers.at(1).first == 2 * kTicksPerBar);
    CHECK(chords.markers.at(1).second == chordName(idea.key, idea.progression.at(1).symbol));

    const MidiTrack bass = laneTrack(idea, LaneId::bass);
    CHECK(bass.name == "Bass");
    CHECK(bass.markers.empty());
    CHECK(laneTrack(idea, LaneId::melody).name == "Melody");
    CHECK(chords.channel == 0);
    CHECK(bass.channel == 1);
    CHECK(laneTrack(idea, LaneId::melody).channel == 2);
}

TEST_CASE("ideaTracks exports all three lanes, muted included")
{
    const Idea idea = makeIdea(); // the chords lane is muted
    REQUIRE(idea.lanes.at(0).muted);
    const auto tracks = ideaTracks(idea);
    REQUIRE(tracks.size() == 3);
    CHECK(tracks.at(0).name == "Chords");
    CHECK(tracks.at(0).notes.size() == 1);
    CHECK(tracks.at(1).name == "Bass");
    CHECK(tracks.at(2).name == "Melody");
    CHECK(ideaLengthTicks(idea) == 4 * kTicksPerBar);
    const Bytes out = writeMidiFile(tracks, 124, ideaLengthTicks(idea));
    // Conductor track: tempo, time signature, then a marker per chord at its delta.
    const Bytes conductor = bytes(
        {0x00, 0xFF, 0x51, 0x03, 0x07, 0xA1, 0x20, 0x00, 0xFF, 0x58, 0x04, 0x04, 0x02, 0x18, 0x08});
    Bytes expected = conductor;
    const auto marker = [&](const Bytes& delta, const PlacedChord& chord)
    {
        const std::string name = chordName(idea.key, chord.symbol);
        expected = concat(expected, delta);
        expected = concat(expected, bytes({0xFF, 0x06, static_cast<int>(name.size())}));
        expected.insert(expected.end(), name.begin(), name.end());
    };
    marker(bytes({0x00}), idea.progression.at(0));
    marker(bytes({0xBC, 0x00}), idea.progression.at(1)); // 2 bars = 7680 ticks
    CHECK(contains(writeMidiFile(tracks, 120, ideaLengthTicks(idea)), expected));
}

TEST_CASE("laneTrack throws when lanes are out of order")
{
    Idea idea = makeIdea();
    std::swap(idea.lanes.at(1), idea.lanes.at(2));
    CHECK_THROWS_AS(laneTrack(idea, LaneId::bass), std::logic_error);
}

TEST_CASE("chordSlotTrack holds one chord's notes from tick 0")
{
    const Bank bank = loadBundledBank();
    const Idea idea = generate(bank, goldenCase0Settings(), 1000);
    const auto& lane = idea.lanes.at(0).events;
    for (std::size_t slot = 0; slot < idea.progression.size(); ++slot)
    {
        const PlacedChord& chord = idea.progression.at(slot);
        const int begin = chord.startBar * kTicksPerBar;
        const int end = begin + (chord.bars * kTicksPerBar);
        std::vector<NoteEvent> expected;
        for (const NoteEvent& n : lane)
        {
            if (n.start >= begin && n.start < end)
            {
                expected.push_back(note(n.pitch, n.start - begin,
                                        std::min(n.start + n.length, end) - n.start, n.velocity));
            }
        }
        const MidiTrack track = chordSlotTrack(idea, slot);
        CHECK(track.name == "Chord");
        CHECK(track.channel == 0);
        CHECK_FALSE(track.notes.empty());
        CHECK(track.notes == expected);
        for (const NoteEvent& n : track.notes)
        {
            CHECK(n.start >= 0);
            CHECK(n.start + n.length <= chord.bars * kTicksPerBar);
        }
    }
    CHECK_THROWS_AS(chordSlotTrack(idea, idea.progression.size()), std::out_of_range);
}

TEST_CASE("chordSlotTrack clips a note that runs past the chord")
{
    Idea idea = makeIdea();
    idea.lanes.at(0).events = {note(57, 3840, 20000, 90)};
    const MidiTrack track = chordSlotTrack(idea, 0);
    REQUIRE(track.notes.size() == 1);
    CHECK(track.notes.at(0) == note(57, 3840, 3840, 90));
}

TEST_CASE("dragFileName names each drag")
{
    Idea idea = makeIdea();
    const auto chord = [](int degree, Quality q, int startBar)
    {
        return PlacedChord{.symbol = {.degree = degree, .quality = q},
                           .feel = "",
                           .startBar = startBar,
                           .bars = 1,
                           .voicing = {}};
    };
    idea.progression = {chord(1, Quality::min9, 0), chord(4, Quality::dom9, 1),
                        chord(6, Quality::maj7, 2), chord(5, Quality::min9, 3)};
    CHECK(dragFileName(idea, LaneId::chords, std::nullopt, 124) ==
          "Chords - Am9 D9 Fmaj7 Em9 - 124bpm.mid");
    CHECK(dragFileName(idea, LaneId::bass, std::nullopt, 124) ==
          "Bass - Am9 D9 Fmaj7 Em9 - 124bpm.mid");
    CHECK(dragFileName(idea, LaneId::melody, std::nullopt, 124) ==
          "Melody - Am9 D9 Fmaj7 Em9 - 124bpm.mid");
    CHECK(dragFileName(idea, std::nullopt, 0, 124) == "Chord - Am9 - 124bpm.mid");
    CHECK(dragFileName(idea, std::nullopt, std::nullopt, 124) ==
          "Magic Dust - Am9 D9 Fmaj7 Em9 - 124bpm.mid");
    idea.progression.erase(idea.progression.begin() + 2, idea.progression.end());
    CHECK(dragFileName(idea, std::nullopt, std::nullopt, 90) == "Magic Dust - Am9 D9 - 90bpm.mid");
}

TEST_CASE("shiftOctaves moves every note 12 semitones per step and drops those out of range")
{
    const MidiTrack track{.name = "T",
                          .notes = {note(60, 0, 240, 90), note(120, 240, 240, 90),
                                    note(5, 480, 240, 90), note(127, 720, 240, 90)},
                          .markers = {{0, "Am"}},
                          .channel = 2};
    CHECK(shiftOctaves(track, 0).notes == track.notes);
    const MidiTrack up = shiftOctaves(track, 1);
    CHECK(up.notes == std::vector<NoteEvent>{note(72, 0, 240, 90), note(17, 480, 240, 90)});
    CHECK(up.channel == 2);
    CHECK(up.markers == track.markers);
    CHECK(shiftOctaves(track, -1).notes == std::vector<NoteEvent>{note(48, 0, 240, 90),
                                                                  note(108, 240, 240, 90),
                                                                  note(115, 720, 240, 90)});
    CHECK(shiftOctaves(track, -2).notes == std::vector<NoteEvent>{note(36, 0, 240, 90),
                                                                  note(96, 240, 240, 90),
                                                                  note(103, 720, 240, 90)});
}

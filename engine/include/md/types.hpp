#pragma once

#include <array>
#include <compare>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace md
{
// Darkest to brightest.
enum class Mode : std::uint8_t
{
    phrygian,
    aeolian,
    dorian,
    mixolydian,
    ionian,
};

struct Key
{
    int root;
    Mode mode;
};

enum class Quality : std::uint8_t
{
    maj,
    min,
    min7,
    min9,
    min11,
    maj7,
    maj9,
    dom7,
    dom9,
    dom13,
    sus4,
    six,
    add9,
};

struct ChordSymbol
{
    int degree; // 1 to 7
    Quality quality;
    bool operator==(const ChordSymbol&) const = default;
};

using Voicing = std::vector<int>; // sorted MIDI notes

struct NoteEvent
{
    int pitch, start, length, velocity;
    auto operator<=>(const NoteEvent&) const = default;
};

enum class LaneId : std::uint8_t
{
    chords,
    bass,
    melody,
};

enum class Style : std::uint8_t
{
    ukFunky,
    mkDub,
    soulful,
    bassline,
    festival,
    nyGroove,
};

enum class Groove : std::uint8_t
{
    straight,
    swing54,
    ukShuffle58,
    heavy62,
};

enum class Flavour : std::uint8_t
{
    simple,
    soulful,
    jazzy,
};

struct Settings
{
    Style style = Style::ukFunky;
    int keyRoot = 9;
    int mood = 30;   // 0 to 100
    int energy = 50; // 0 to 100
    Groove groove = Groove::ukShuffle58;
    Flavour flavour = Flavour::soulful;
    int bars = 8; // 4, 8 or 16
};

struct Lane
{
    LaneId id;
    std::string pattern;
    std::vector<NoteEvent> events;
    bool pinned = false;
    bool muted = false;
    std::uint64_t seed = 0;
    bool refitted = false; // set when a refit changed a pitch
};

struct PlacedChord
{
    ChordSymbol symbol;
    std::string feel;
    int startBar;
    int bars;
    Voicing voicing;
};

struct Idea
{
    std::uint32_t seed;
    Settings settings;
    Key key;
    std::optional<Mode> borrowedFrom;
    std::uint64_t bankVersion;
    std::string entryId;
    bool parallel = false;
    std::vector<PlacedChord> progression;
    std::array<Lane, 3> lanes;
};
} // namespace md

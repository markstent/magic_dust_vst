#pragma once

#include "md/types.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace md
{
enum class Family : std::uint8_t
{
    mkDub,
    soulful,
    bassline,
    festival,
    nyGroove,
};

enum class Confidence : std::uint8_t
{
    placeholder,
    estimated,
    verified,
};

enum class VoicingStyle : std::uint8_t
{
    close,
    drop2,
    organStab,
    pianoSpread,
};

enum class BassPattern : std::uint8_t
{
    rollingOffbeat,
    offbeat,
    octaveBounce,
    garageSkip,
    sustained,
    riff,
    pedal,
    callResponse,
};

enum class MelodyMode : std::uint8_t
{
    vocalChop,
    hook,
    riff,
    sparse,
};

struct Hit
{
    int step;
    int length;
};

struct RhythmCell
{
    std::string id;
    int bars = 1;
    std::vector<Hit> hits;
};

struct Progression
{
    std::vector<ChordSymbol> chords;
    std::vector<int> barsPerChord;
    int weightMilli = 0;
};

struct StyleEntry
{
    std::string id;
    Family family;
    std::string name;
    Confidence confidence;
    int tempoMin;
    int tempoMax;
    std::vector<Mode> modes;
    std::vector<Progression> progressions;
    VoicingStyle voicingStyle;
    bool parallel;
    int topNoteLow;
    int topNoteHigh;
    std::vector<RhythmCell> chordRhythms;
    std::vector<RhythmCell> melodyRhythms;
    std::vector<BassPattern> bassPatterns;
    std::vector<MelodyMode> melodyModes;
    int swingPermille;
    std::vector<std::string> tags;
    std::uint64_t contentHash;
};

struct Bank
{
    std::vector<StyleEntry> entries; // sorted by id
    std::uint64_t version;
};
} // namespace md

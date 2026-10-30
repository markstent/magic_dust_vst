#pragma once

#include "md/types.hpp"

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace md
{
namespace detail
{
struct Models;
} // namespace detail

struct Bank;

// The models generate learns from a bank's entries: built on first use, then shared by every
// generate call on that bank, from any thread. A copied or moved cache starts empty, so a copy
// whose entries are edited learns from the edited entries; edit a bank's entries before generating
// from it.
class ModelCache
{
  public:
    ModelCache();
    ModelCache(const ModelCache& other);
    ModelCache(ModelCache&& other) noexcept;
    ModelCache& operator=(const ModelCache& other);
    ModelCache& operator=(ModelCache&& other) noexcept;
    ~ModelCache();

    const detail::Models& get(const Bank& bank) const;

  private:
    mutable std::mutex mutex_;
    mutable std::unique_ptr<const detail::Models> models_;
};
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
    pluck,
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
    // Never assign a bank while another thread generates from it: assignment empties the cache.
    // The initializer lets a designated initializer leave the cache out.
    ModelCache models{}; // NOLINT(readability-redundant-member-init)
};
} // namespace md

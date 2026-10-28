#include "md/generate.hpp"

#include "internal.hpp"
#include "md/names.hpp"
#include "md/pcg32.hpp"
#include "md/ranges.hpp"
#include "md/theory.hpp"
#include "md/validate.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <iterator>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace md
{
namespace detail
{
int swung(int start, int swingPermille)
{
    if ((start / kTicksPerSixteenth) % 2 == 0)
    {
        return start;
    }
    return start - kTicksPerSixteenth + ((2 * kTicksPerSixteenth * swingPermille) / 1000);
}

const PlacedChord& chordAt(std::span<const PlacedChord> progression, int tick)
{
    const PlacedChord* chord = findChord(progression, tick);
    if (chord == nullptr)
    {
        throw std::logic_error("no chord at tick " + std::to_string(tick));
    }
    return *chord;
}

std::vector<int> soundingPitchClasses(const Idea& idea, const PlacedChord& chord)
{
    if (!idea.parallel)
    {
        return chordPitchClasses(idea.key, chord.symbol);
    }
    std::vector<int> pcs;
    std::ranges::transform(chord.voicing, std::back_inserter(pcs),
                           [](int pitch) { return pitch % 12; });
    std::ranges::sort(pcs);
    pcs.erase(std::ranges::unique(pcs).begin(), pcs.end());
    return pcs;
}

std::vector<int> soundingPcsAt(const Idea& idea, int tick)
{
    const PlacedChord* chord = findChord(idea.progression, tick);
    return chord == nullptr ? std::vector<int>{} : soundingPitchClasses(idea, *chord);
}

std::vector<int> allowedPitchClasses(const std::vector<int>& pcs)
{
    std::vector<int> allowed;
    for (const int pc : pcs)
    {
        const bool clash =
            std::ranges::any_of(pcs,
                                [pc](int other)
                                {
                                    const int distance = (other - pc + 12) % 12;
                                    return other != pc && (distance < 2 || distance > 10);
                                });
        if (!clash)
        {
            allowed.push_back(pc);
        }
    }
    return allowed;
}

void clampMonophonic(std::vector<NoteEvent>& events, int endTick)
{
    for (std::size_t i = 0; i < events.size(); ++i)
    {
        const int limit =
            i + 1 < events.size() ? std::min(events.at(i + 1).start, endTick) : endTick;
        events.at(i).length = std::min(events.at(i).length, limit - events.at(i).start);
    }
    std::erase_if(events, [](const NoteEvent& e) { return e.length < 1; });
}

ChordSymbol applyFlavour(const Key& key, std::optional<Mode> borrowedFrom, const ChordSymbol& chord,
                         Flavour flavour)
{
    if (flavour == Flavour::soulful)
    {
        return chord;
    }
    if (flavour == Flavour::simple)
    {
        ChordSymbol plain = chord;
        switch (chord.quality)
        {
        case Quality::min9:
        case Quality::min11:
            plain.quality = Quality::min7;
            break;
        case Quality::maj9:
            plain.quality = Quality::maj7;
            break;
        case Quality::dom9:
        case Quality::dom13:
            plain.quality = Quality::dom7;
            break;
        case Quality::add9:
            plain.quality = Quality::maj;
            break;
        default:
            break;
        }
        return plain;
    }
    const auto scale = scalePitchClasses(key);
    const auto borrowedScale = borrowedFrom
                                   ? scalePitchClasses({.root = key.root, .mode = *borrowedFrom})
                                   : std::vector<int>{};
    const auto allowed = [&](Quality quality)
    {
        const auto pcs = chordPitchClasses(key, {.degree = chord.degree, .quality = quality});
        const auto within = [&pcs](const std::vector<int>& notes)
        {
            return std::ranges::all_of(pcs, [&notes](int pc)
                                       { return std::ranges::find(notes, pc) != notes.end(); });
        };
        return within(scale) || (borrowedFrom && within(borrowedScale));
    };
    const auto next = [&](Quality quality) -> std::optional<Quality>
    {
        switch (quality)
        {
        case Quality::min:
            return Quality::min7;
        case Quality::min7:
            return Quality::min9;
        case Quality::min9:
            return Quality::min11;
        case Quality::maj:
            return allowed(Quality::maj7) ? Quality::maj7 : Quality::dom7;
        case Quality::maj7:
            return Quality::maj9;
        case Quality::dom7:
            return Quality::dom9;
        case Quality::dom9:
            return Quality::dom13;
        default:
            return std::nullopt;
        }
    };
    ChordSymbol result = chord;
    for (int steps = 2; steps > 0; --steps)
    {
        const auto step = next(result.quality);
        if (!step || !allowed(*step))
        {
            break;
        }
        result.quality = *step;
    }
    return result;
}
} // namespace detail

namespace
{
constexpr int kVerifiedWeightFactor = 2;
constexpr int kVelocityBase = 80;
constexpr int kVelocitySpread = 30;
constexpr int kHumanizeRange = 13;

std::optional<Mode> borrowedFor(const Idea& idea)
{
    std::vector<ChordSymbol> symbols;
    std::ranges::transform(idea.progression, std::back_inserter(symbols), &PlacedChord::symbol);
    return checkProgression(idea.key.mode, symbols).borrowedFrom;
}

void checkSettings(const Settings& settings, std::uint32_t seed)
{
    if (seed > static_cast<std::uint32_t>(kSeedMax))
    {
        throw std::invalid_argument("generate: seed above " + std::to_string(kSeedMax));
    }
    if (settings.bars != 4 && settings.bars != 8 && settings.bars != 16)
    {
        throw std::invalid_argument("generate: bars must be 4, 8 or 16");
    }
    if (settings.keyRoot < 0 || settings.keyRoot > 11)
    {
        throw std::invalid_argument("generate: key root must be 0 to 11");
    }
    if (settings.mood < 0 || settings.mood > 100 || settings.energy < 0 || settings.energy > 100)
    {
        throw std::invalid_argument("generate: mood and energy must be 0 to 100");
    }
}

void checkCells(const std::string& entryId, const std::vector<RhythmCell>& cells)
{
    if (cells.empty())
    {
        throw std::logic_error("entry " + entryId + " has no rhythm cells");
    }
    for (const RhythmCell& cell : cells)
    {
        const bool badHit = std::ranges::any_of(
            cell.hits, [&cell](const Hit& hit)
            { return hit.step < 0 || hit.step >= kStepsPerBar * cell.bars || hit.length < 1; });
        if ((cell.bars != 1 && cell.bars != 2) || cell.hits.empty() || badHit)
        {
            throw std::logic_error("entry " + entryId + " has a malformed rhythm cell " + cell.id);
        }
    }
}

// The bank loader rejects these; a bank built by hand from unvalidated entries may not.
void checkEntry(const StyleEntry& entry)
{
    if (entry.progressions.empty() || entry.melodyModes.empty())
    {
        throw std::logic_error("entry " + entry.id + " has no progressions or melody modes");
    }
    for (const Progression& p : entry.progressions)
    {
        const bool badBars = std::ranges::any_of(p.barsPerChord, [](int bars) { return bars < 1; });
        if (p.chords.empty() || p.chords.size() != p.barsPerChord.size() || badBars)
        {
            throw std::logic_error("entry " + entry.id + " has a malformed progression");
        }
    }
    checkCells(entry.id, entry.chordRhythms);
    checkCells(entry.id, entry.melodyRhythms);
}

bool inStyle(Style style, Family family)
{
    switch (style)
    {
    case Style::ukFunky:
        return true;
    case Style::mkDub:
        return family == Family::mkDub;
    case Style::soulful:
        return family == Family::soulful;
    case Style::bassline:
        return family == Family::bassline;
    case Style::festival:
        return family == Family::festival;
    case Style::nyGroove:
        return family == Family::nyGroove;
    }
    return false;
}

// The entry's modes that fit the whole progression (all of them for parallel entries), darkest
// first; mood picks one.
Mode pickMode(const StyleEntry& entry, const Progression& progression, int mood)
{
    std::vector<Mode> fitting;
    for (const Mode mode : entry.modes)
    {
        if (entry.parallel || checkProgression(mode, progression.chords).ok)
        {
            fitting.push_back(mode);
        }
    }
    if (fitting.empty())
    {
        throw std::logic_error("entry " + entry.id +
                               " has a progression that fits none of its modes");
    }
    std::ranges::sort(fitting);
    return fitting.at(static_cast<std::size_t>(mood) * fitting.size() / 101);
}

// One velocity per note in event order, drawn from the lane's own seed.
void humanize(std::vector<NoteEvent>& events, Pcg32 rng, int energy)
{
    const int base = kVelocityBase + ((kVelocitySpread * energy) / 100);
    for (NoteEvent& e : events)
    {
        // Events arrive with their velocity as a percent of the base.
        const int scaled = (base * e.velocity) / 100;
        e.velocity = std::clamp(scaled + static_cast<int>(rng.below(kHumanizeRange)) - 6, 1, 127);
    }
}

Lane makeLane(LaneId id, std::uint64_t seed, std::string pattern, std::vector<NoteEvent> events,
              std::uint32_t humanizeSlot, int energy)
{
    humanize(events, Pcg32(seed, humanizeSlot), energy);
    return {
        .id = id,
        .pattern = std::move(pattern),
        .events = std::move(events),
        .pinned = false,
        .muted = false,
        .seed = seed,
    };
}

// The semitones from one key root to another, taken in -5 to +6.
int keyShift(int from, int to) { return ((((to - from) % 12) + 12 + 5) % 12) - 5; }

const StyleEntry* findEntry(const Bank& bank, const std::string& id)
{
    const auto it = std::ranges::find(bank.entries, id, &StyleEntry::id);
    return it == bank.entries.end() ? nullptr : &*it;
}

const StyleEntry& pickEntry(const Bank& bank, Style style, const LaneSeeds& laneSeeds)
{
    std::vector<const StyleEntry*> entries;
    for (const StyleEntry& entry : bank.entries)
    {
        if (inStyle(style, entry.family))
        {
            entries.push_back(&entry);
        }
    }
    if (entries.empty())
    {
        throw std::invalid_argument("generate: the bank has no entry for this style");
    }
    Pcg32 entryRng(laneSeeds.chords, detail::kChordEntry);
    std::vector<int> entryWeights;
    for (const StyleEntry* entry : entries)
    {
        const int sum =
            std::accumulate(entry->progressions.begin(), entry->progressions.end(), 0,
                            [](int total, const Progression& p) { return total + p.weightMilli; });
        entryWeights.push_back(
            entry->confidence == Confidence::verified ? sum * kVerifiedWeightFactor : sum);
    }
    return *entries.at(entryRng.pickWeighted(entryWeights));
}

// The chosen entry's progression, key mode, flavour, voicings and the chord lane.
void buildChords(Idea& idea, const StyleEntry& entry, const LaneSeeds& laneSeeds)
{
    const Settings& settings = idea.settings;
    std::vector<int> progressionWeights;
    std::ranges::transform(entry.progressions, std::back_inserter(progressionWeights),
                           &Progression::weightMilli);
    Pcg32 progressionRng(laneSeeds.chords, detail::kChordProgression);
    const Progression& progression =
        entry.progressions.at(progressionRng.pickWeighted(progressionWeights));

    idea.key = {.root = settings.keyRoot, .mode = pickMode(entry, progression, settings.mood)};
    idea.progression = detail::placeProgression(progression, settings.bars);
    idea.borrowedFrom = borrowedFor(idea);
    for (PlacedChord& chord : idea.progression)
    {
        chord.symbol =
            detail::applyFlavour(idea.key, idea.borrowedFrom, chord.symbol, settings.flavour);
    }
    idea.borrowedFrom = borrowedFor(idea);
    detail::voiceChords(entry, idea.key, idea.progression);

    Pcg32 rhythmRng(laneSeeds.chords, detail::kChordCell);
    const RhythmCell& chordCell = entry.chordRhythms.at(
        rhythmRng.below(static_cast<std::uint32_t>(entry.chordRhythms.size())));
    idea.lanes.at(0) = makeLane(LaneId::chords, laneSeeds.chords, chordCell.id,
                                detail::chordEvents(idea.progression, chordCell, settings),
                                detail::kChordHumanize, settings.energy);
}

bool voicingFits(const Voicing& voicing, int offset)
{
    Voicing moved = voicing;
    for (int& pitch : moved)
    {
        pitch += offset;
    }
    return !moved.empty() && moved.front() >= kChordLow && moved.back() >= kChordTopLow &&
           moved.back() <= kChordTopHigh && !detail::isMuddy(moved);
}

// The previous progression and chord lane moved by the key shift. The shift, or an octave off it,
// is used when every voicing stays valid; otherwise the chords are revoiced, keeping the rhythm.
void keepChords(Idea& idea, const Idea& previous, const StyleEntry& entry, int shift)
{
    idea.key = {.root = idea.settings.keyRoot, .mode = previous.key.mode};
    idea.borrowedFrom = previous.borrowedFrom;
    idea.progression = previous.progression;
    const Lane& lane = previous.lanes.at(0);
    const std::array<int, 3> offsets{shift, shift - 12, shift + 12};
    const auto fits = [&](int offset)
    {
        return std::ranges::all_of(idea.progression, [offset](const PlacedChord& chord)
                                   { return voicingFits(chord.voicing, offset); });
    };
    const auto* const offset = std::ranges::find_if(offsets, fits);
    if (offset != offsets.end())
    {
        for (PlacedChord& chord : idea.progression)
        {
            std::ranges::for_each(chord.voicing, [offset](int& pitch) { pitch += *offset; });
        }
        idea.lanes.at(0) = lane;
        idea.lanes.at(0).muted = false;
        std::ranges::for_each(idea.lanes.at(0).events,
                              [offset](NoteEvent& e) { e.pitch += *offset; });
        return;
    }
    detail::voiceChords(entry, idea.key, idea.progression);
    const auto cell = std::ranges::find(entry.chordRhythms, lane.pattern, &RhythmCell::id);
    idea.lanes.at(0) = makeLane(
        LaneId::chords, lane.seed, lane.pattern,
        detail::chordEvents(idea.progression,
                            cell == entry.chordRhythms.end() ? entry.chordRhythms.front() : *cell,
                            idea.settings),
        detail::kChordHumanize, idea.settings.energy);
}

// A previous lane moved by the key shift, each note an octave back into range where it left it.
struct PitchRange
{
    int low;
    int high;
};

Lane keptLane(const Lane& previous, int shift, PitchRange range)
{
    Lane lane = previous;
    lane.muted = false;
    for (NoteEvent& e : lane.events)
    {
        e.pitch += shift;
        while (e.pitch < range.low)
        {
            e.pitch += 12;
        }
        while (e.pitch > range.high)
        {
            e.pitch -= 12;
        }
    }
    return lane;
}
} // namespace

LaneSeeds defaultLaneSeeds(std::uint32_t ideaSeed)
{
    return {
        .chords = laneSeed(ideaSeed, Stream::chords, 0),
        .bass = laneSeed(ideaSeed, Stream::bass, 0),
        .melody = laneSeed(ideaSeed, Stream::melody, 0),
    };
}

Idea generate(const Bank& bank, const Settings& settings, std::uint32_t seed)
{
    return generate(bank, settings, seed, defaultLaneSeeds(seed));
}

Idea generate(const Bank& bank, const Settings& settings, std::uint32_t seed,
              const LaneSeeds& laneSeeds)
{
    return generate(bank, settings, seed, laneSeeds, Idea{}, Pins{});
}

Idea generate(const Bank& bank, const Settings& settings, std::uint32_t seed,
              const LaneSeeds& laneSeeds, const Idea& previous, Pins pins)
{
    checkSettings(settings, seed);
    if (previous.settings.bars != settings.bars || previous.entryId.empty())
    {
        pins = {};
    }
    const StyleEntry* keptEntry = pins.chords ? findEntry(bank, previous.entryId) : nullptr;
    // A bank without the previous entry cannot keep its chords.
    pins.chords = keptEntry != nullptr;
    const bool keepAny = pins.chords || pins.bass || pins.melody;
    const bool keepAll = pins.chords && pins.bass && pins.melody;
    const int shift = keyShift(previous.key.root, settings.keyRoot);
    const StyleEntry& entry = pins.chords ? *keptEntry : pickEntry(bank, settings.style, laneSeeds);
    checkEntry(entry);

    Idea idea{};
    idea.seed = keepAll ? previous.seed : seed;
    idea.settings = settings;
    idea.bankVersion = bank.version;
    idea.entryId = entry.id;
    idea.parallel = entry.parallel;
    if (pins.chords)
    {
        keepChords(idea, previous, entry, shift);
    }
    else
    {
        buildChords(idea, entry, laneSeeds);
    }

    if (pins.bass)
    {
        idea.lanes.at(1) =
            keptLane(previous.lanes.at(1), shift, {.low = kBassLow, .high = kBassHigh});
        refitBass(idea);
    }
    else
    {
        Pcg32 bassPatternRng(laneSeeds.bass, detail::kBassPattern);
        const BassPattern bassPattern =
            detail::bassPatternFor(entry, settings.energy, bassPatternRng);
        idea.lanes.at(1) =
            makeLane(LaneId::bass, laneSeeds.bass, std::string(bassPatternName(bassPattern)),
                     detail::bassEvents(idea, bassPattern, laneSeeds.bass), detail::kBassHumanize,
                     settings.energy);
    }
    if (pins.melody)
    {
        idea.lanes.at(2) =
            keptLane(previous.lanes.at(2), shift, {.low = kMelodyLow, .high = kMelodyHigh});
        refitMelody(idea);
    }
    else
    {
        idea.lanes.at(2) = makeLane(LaneId::melody, laneSeeds.melody,
                                    std::string(melodyModeName(entry.melodyModes.front())),
                                    detail::melodyEvents(entry, idea, laneSeeds.melody),
                                    detail::kMelodyHumanize, settings.energy);
    }
    if (keepAny)
    {
        idea.lanes.at(0).pinned = pins.chords;
        idea.lanes.at(1).pinned = pins.bass;
        idea.lanes.at(2).pinned = pins.melody;
    }

    const auto violations = validate(idea);
    if (!violations.empty())
    {
        std::string message = "generate produced an invalid idea:";
        for (const std::string& line : violations)
        {
            message += "\n" + line;
        }
        throw std::logic_error(message);
    }
    return idea;
}
} // namespace md

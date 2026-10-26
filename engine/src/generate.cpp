#include "md/generate.hpp"

#include "internal.hpp"
#include "md/pcg32.hpp"
#include "md/ranges.hpp"
#include "md/theory.hpp"
#include "md/validate.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <iterator>
#include <numeric>
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

std::vector<int> allowedPitchClasses(const Key& key, const ChordSymbol& chord)
{
    const auto pcs = chordPitchClasses(key, chord);
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
} // namespace detail

namespace
{
constexpr int kVerifiedWeightFactor = 2;
constexpr int kVelocityBase = 80;
constexpr int kVelocitySpread = 30;
constexpr int kHumanizeRange = 13;

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

Pcg32 rngFor(std::uint32_t seed, Stream stream, std::uint32_t slot = 0)
{
    return {laneSeed(seed, stream, slot), static_cast<std::uint64_t>(stream)};
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

void humanize(Idea& idea)
{
    Pcg32 rng = rngFor(idea.seed, Stream::humanize);
    const int base = kVelocityBase + ((kVelocitySpread * idea.settings.energy) / 100);
    for (Lane& lane : idea.lanes)
    {
        for (NoteEvent& e : lane.events)
        {
            e.velocity = std::clamp(base + static_cast<int>(rng.below(kHumanizeRange)) - 6, 1, 127);
        }
    }
}

Lane makeLane(LaneId id, Stream stream, std::uint32_t seed, std::string pattern,
              std::vector<NoteEvent> events)
{
    return {
        .id = id,
        .pattern = std::move(pattern),
        .events = std::move(events),
        .pinned = false,
        .muted = false,
        .seed = laneSeed(seed, stream, 0),
    };
}
} // namespace

Idea generate(const Bank& bank, const Settings& settings, std::uint32_t seed)
{
    checkSettings(settings, seed);
    std::vector<const StyleEntry*> entries;
    for (const StyleEntry& entry : bank.entries)
    {
        if (inStyle(settings.style, entry.family))
        {
            entries.push_back(&entry);
        }
    }
    if (entries.empty())
    {
        throw std::invalid_argument("generate: the bank has no entry for this style");
    }

    Pcg32 chordRng = rngFor(seed, Stream::chords);
    std::vector<int> entryWeights;
    for (const StyleEntry* entry : entries)
    {
        const int sum =
            std::accumulate(entry->progressions.begin(), entry->progressions.end(), 0,
                            [](int total, const Progression& p) { return total + p.weightMilli; });
        entryWeights.push_back(
            entry->confidence == Confidence::verified ? sum * kVerifiedWeightFactor : sum);
    }
    const StyleEntry& entry = *entries.at(chordRng.pickWeighted(entryWeights));
    std::vector<int> progressionWeights;
    std::ranges::transform(entry.progressions, std::back_inserter(progressionWeights),
                           &Progression::weightMilli);
    const Progression& progression =
        entry.progressions.at(chordRng.pickWeighted(progressionWeights));

    Idea idea{};
    idea.seed = seed;
    idea.settings = settings;
    idea.key = {.root = settings.keyRoot, .mode = pickMode(entry, progression, settings.mood)};
    idea.borrowedFrom = checkProgression(idea.key.mode, progression.chords).borrowedFrom;
    idea.bankVersion = bank.version;
    idea.entryId = entry.id;
    idea.parallel = entry.parallel;
    idea.progression = detail::placeProgression(progression, settings.bars);
    detail::voiceChords(entry, idea.key, idea.progression);

    Pcg32 rhythmRng = rngFor(seed, Stream::chords, 1);
    const RhythmCell& chordCell = entry.chordRhythms.at(
        rhythmRng.below(static_cast<std::uint32_t>(entry.chordRhythms.size())));
    const BassPattern bassPattern = detail::bassPatternFor(entry);
    idea.lanes.at(0) = makeLane(LaneId::chords, Stream::chords, seed, chordCell.id,
                                detail::chordEvents(idea.progression, chordCell, settings));
    idea.lanes.at(1) =
        makeLane(LaneId::bass, Stream::bass, seed, std::string(patternName(bassPattern)),
                 detail::bassEvents(idea, bassPattern));
    idea.lanes.at(2) = makeLane(LaneId::melody, Stream::melody, seed,
                                std::string(patternName(entry.melodyModes.front())),
                                detail::melodyEvents(entry, idea));
    humanize(idea);

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

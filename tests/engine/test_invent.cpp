#include "internal.hpp"
#include "md/bank.hpp"
#include "md/pcg32.hpp"
#include "md/ranges.hpp"
#include "md/theory.hpp"
#include "md/validate.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <utility>
#include <vector>

using namespace md;
using namespace md::detail;

namespace
{
constexpr std::array kFamilies = {
    Family::mkDub, Family::soulful, Family::bassline, Family::festival, Family::nyGroove,
};
constexpr int kSeeds = 500;

const Bank& bank()
{
    static const Bank loaded = loadBundledBank();
    return loaded;
}

struct Invented
{
    const StyleEntry* entry;
    int keyRoot;
    ProgressionModel model;
    std::optional<Progression> progression;
};

Pcg32 rngFor(int seed)
{
    return {laneSeed(static_cast<std::uint32_t>(seed), Stream::chords, 0), kChordProgression};
}

// One invention per seed, each entry of the family in turn and the key root cycling, from the
// model of the entry's mode group.
const std::vector<Invented>& invented(Family family)
{
    static std::map<Family, std::vector<Invented>> cache;
    auto [it, fresh] = cache.try_emplace(family);
    if (!fresh)
    {
        return it->second;
    }
    std::vector<const StyleEntry*> entries;
    for (const StyleEntry& entry : bank().entries)
    {
        if (entry.family == family)
        {
            entries.push_back(&entry);
        }
    }
    for (int seed = 0; seed < kSeeds; ++seed)
    {
        const StyleEntry& entry = *entries.at(static_cast<std::size_t>(seed) % entries.size());
        const int keyRoot = seed % kPitchClasses;
        ProgressionModel model = buildProgressionModel(bank(), family, modeGroup(entry));
        Pcg32 rng = rngFor(seed);
        auto progression = inventProgression(model, entry, rng);
        it->second.push_back({.entry = &entry,
                              .keyRoot = keyRoot,
                              .model = std::move(model),
                              .progression = std::move(progression)});
    }
    return it->second;
}

bool contains(const std::vector<ChordSymbol>& chords, const ChordSymbol& chord)
{
    return std::ranges::find(chords, chord) != chords.end();
}

// The degree and quality of each chord, as a set key.
std::vector<std::pair<int, Quality>> keyOf(const std::vector<ChordSymbol>& chords)
{
    std::vector<std::pair<int, Quality>> key;
    key.reserve(chords.size());
    for (const ChordSymbol& chord : chords)
    {
        key.emplace_back(chord.degree, chord.quality);
    }
    return key;
}

bool inBank(Family family, const Progression& progression)
{
    for (const StyleEntry& entry : bank().entries)
    {
        const bool written = std::ranges::any_of(entry.progressions, [&](const Progression& p)
                                                 { return p.chords == progression.chords; });
        if (entry.family == family && written)
        {
            return true;
        }
    }
    return false;
}
} // namespace

TEST_CASE("smoothing tokens are plain triads or 7ths in the family's style", "[invent]")
{
    const ProgressionModel soulful =
        buildProgressionModel(bank(), Family::soulful, ModeGroup::minor);
    CHECK(soulful.sevenths);
    const ProgressionModel festival =
        buildProgressionModel(bank(), Family::festival, ModeGroup::major);
    CHECK_FALSE(festival.sevenths);
    const auto& entry = *std::ranges::find(bank().entries, "soulful-starter-01", &StyleEntry::id);
    for (const ChordSymbol& chord : smoothingTokens(soulful, entry))
    {
        CHECK((chord.quality == Quality::min7 || chord.quality == Quality::maj7 ||
               chord.quality == Quality::dom7));
    }
}

TEST_CASE("an entry learns from the mode groups its modes span", "[invent]")
{
    const auto entryOf = [](const char* id)
    { return *std::ranges::find(bank().entries, id, &StyleEntry::id); };
    CHECK(modeGroup(entryOf("ny-groove-starter-01")) == ModeGroup::minor);
    CHECK(modeGroup(entryOf("festival-starter-03")) == ModeGroup::major);
    const StyleEntry& both = entryOf("ny-groove-starter-03");
    REQUIRE(modeGroup(both) == ModeGroup::both);
    const ProgressionModel model = buildProgressionModel(bank(), Family::nyGroove, ModeGroup::both);
    std::size_t templates = 0;
    for (const StyleEntry& entry : bank().entries)
    {
        if (entry.family != Family::nyGroove)
        {
            continue;
        }
        templates += entry.progressions.size();
        for (const Progression& progression : entry.progressions)
        {
            for (const ChordSymbol& chord : progression.chords)
            {
                CHECK(contains(model.tokens, chord));
            }
        }
    }
    CHECK(model.templates.size() == templates);
    const ProgressionModel major =
        buildProgressionModel(bank(), Family::nyGroove, ModeGroup::major);
    CHECK(major.templates.size() == both.progressions.size());
}

TEST_CASE("move weights are 1000 a count, 150 between learned tokens and 50 for smoothing",
          "[invent]")
{
    ProgressionModel model;
    model.tokens = {{.degree = 1, .quality = Quality::min},
                    {.degree = 4, .quality = Quality::min},
                    {.degree = 5, .quality = Quality::dom7}};
    model.transitions = {{0, 2, 0}, {0, 0, 0}, {0, 0, 0}};
    std::vector<ChordSymbol> states = model.tokens;
    states.push_back({.degree = 6, .quality = Quality::maj});
    CHECK(moveWeight(model, states, 0, 1) == 2150);
    CHECK(moveWeight(model, states, 0, 2) == 150);
    CHECK(moveWeight(model, states, 0, 0) == 0);
    CHECK(moveWeight(model, states, 0, 3) == 50);
    CHECK(moveWeight(model, states, 3, 1) == 50);
}

// One test case, so the 2500 inventions are made once per run.
TEST_CASE("invented progressions over 500 seeds per family", "[invent]")
{
    SECTION("every invented progression fits a mode of its entry")
    {
        for (const Family family : kFamilies)
        {
            int fallbacks = 0;
            for (const Invented& item : invented(family))
            {
                if (!item.progression)
                {
                    ++fallbacks;
                    continue;
                }
                const auto& chords = item.progression->chords;
                CHECK(std::ranges::any_of(item.entry->modes, [&](Mode mode)
                                          { return checkProgression(mode, chords).ok; }));
                CHECK(chords.size() == item.progression->barsPerChord.size());
            }
            INFO("family " << static_cast<int>(family));
            CHECK(fallbacks * 100 < kSeeds * 2);
        }
    }

    SECTION("invented progressions use only learned or smoothing tokens")
    {
        for (const Family family : kFamilies)
        {
            for (const Invented& item : invented(family))
            {
                const auto smoothing = smoothingTokens(item.model, *item.entry);
                for (const ChordSymbol& chord : smoothing)
                {
                    CHECK_FALSE(contains(item.model.tokens, chord));
                    const std::array plain = {chord};
                    const ProgressionFit fit = checkProgression(item.entry->modes.front(), plain);
                    CHECK((fit.ok && !fit.borrowedFrom));
                }
                const auto chords = item.progression.value_or(Progression{}).chords;
                for (const ChordSymbol& chord : chords)
                {
                    CHECK((contains(item.model.tokens, chord) || contains(smoothing, chord)));
                }
            }
        }
    }

    SECTION("smoothing tokens make up under 15% of invented chords")
    {
        for (const Family family : kFamilies)
        {
            int chords = 0;
            int smoothed = 0;
            for (const Invented& item : invented(family))
            {
                if (!item.progression)
                {
                    continue;
                }
                for (const ChordSymbol& chord : item.progression->chords)
                {
                    ++chords;
                    smoothed += contains(item.model.tokens, chord) ? 0 : 1;
                }
            }
            INFO("family " << static_cast<int>(family));
            REQUIRE(chords > 0);
            CHECK(smoothed * 100 < chords * 6); // measured 1 to 3.5%
        }
    }

    SECTION("invented progressions are mostly new and varied")
    {
        for (const Family family : kFamilies)
        {
            int count = 0;
            int fresh = 0;
            std::set<std::vector<std::pair<int, Quality>>> distinct;
            for (const Invented& item : invented(family))
            {
                if (!item.progression)
                {
                    continue;
                }
                ++count;
                fresh += inBank(family, *item.progression) ? 0 : 1;
                distinct.insert(keyOf(item.progression->chords));
            }
            INFO("family " << static_cast<int>(family));
            CHECK(fresh * 100 >= count * 60);
            CHECK(distinct.size() >= 15);
        }
    }

    SECTION("invented progressions never put a degree next to itself, the wrap included")
    {
        for (const Family family : kFamilies)
        {
            for (const Invented& item : invented(family))
            {
                if (!item.progression || item.progression->chords.size() < 2)
                {
                    continue;
                }
                const auto& chords = item.progression->chords;
                for (std::size_t i = 0; i < chords.size(); ++i)
                {
                    CHECK(chords.at(i).degree != chords.at((i + 1) % chords.size()).degree);
                }
            }
        }
    }

    SECTION("a 1-chord progression holds a learned tonic")
    {
        int held = 0;
        for (const Family family : kFamilies)
        {
            for (const Invented& item : invented(family))
            {
                if (!item.progression || item.progression->chords.size() != 1)
                {
                    continue;
                }
                ++held;
                const ChordSymbol& chord = item.progression->chords.front();
                CHECK(chord.degree == 1);
                CHECK(contains(item.model.tokens, chord));
            }
        }
        CHECK(held > 0);
    }

    SECTION("inventing a progression is deterministic")
    {
        for (const Family family : kFamilies)
        {
            const auto& items = invented(family);
            for (std::size_t seed = 0; seed < items.size(); seed += 25)
            {
                const Invented& item = items.at(seed);
                Pcg32 rng = rngFor(static_cast<int>(seed));
                const auto again = inventProgression(item.model, *item.entry, rng);
                REQUIRE(again.has_value() == item.progression.has_value());
                if (again && item.progression)
                {
                    CHECK(again->chords == item.progression->chords);
                    CHECK(again->barsPerChord == item.progression->barsPerChord);
                }
            }
        }
    }
}

TEST_CASE("invented progressions voice and validate at the key root", "[invent]")
{
    for (const Family family : kFamilies)
    {
        const auto& items = invented(family);
        for (std::size_t seed = 0; seed < items.size(); seed += 10)
        {
            const Invented& item = items.at(seed);
            if (!item.progression)
            {
                continue;
            }
            for (const Mode mode : fittingModes(*item.entry, *item.progression))
            {
                for (const Flavour flavour : {Flavour::simple, Flavour::soulful, Flavour::jazzy})
                {
                    Idea idea{};
                    idea.settings.bars = 16;
                    idea.settings.flavour = flavour;
                    idea.key = {.root = item.keyRoot, .mode = mode};
                    idea.parallel = item.entry->parallel;
                    placeChords(idea, *item.progression);
                    voiceChords(*item.entry, idea.key, idea.progression);
                    CHECK(validate(idea).empty());
                }
            }
        }
    }
}

namespace
{
constexpr int kRhythmSeeds = 500;

Pcg32 rhythmRng(int seed)
{
    return {laneSeed(static_cast<std::uint32_t>(seed), Stream::chords, kChordCell), 9};
}

int bars(RhythmKind kind) { return kind == RhythmKind::chords ? 1 : 2; }

struct Cells
{
    std::vector<RhythmCell> made;
    int fallbacks = 0;
};

// An invented cell per seed, with the energy cycling over 0 to 100 when it is negative.
Cells inventedCells(Family family, RhythmKind kind, int energy)
{
    const RhythmModel model = buildRhythmModel(bank(), family, kind);
    Cells cells;
    for (int seed = 0; seed < kRhythmSeeds; ++seed)
    {
        Pcg32 rng = rhythmRng(seed);
        auto cell = inventRhythm(model, bars(kind), energy < 0 ? seed % 101 : energy, rng);
        if (cell)
        {
            cells.made.push_back(std::move(*cell));
        }
        else
        {
            ++cells.fallbacks;
        }
    }
    return cells;
}

std::vector<std::pair<int, int>> stepsOf(const RhythmCell& cell)
{
    std::vector<std::pair<int, int>> steps;
    steps.reserve(cell.hits.size());
    for (const Hit& hit : cell.hits)
    {
        steps.emplace_back(hit.step, hit.length);
    }
    return steps;
}

// The onset steps of one bar of a cell.
std::vector<int> barSteps(const RhythmCell& cell, int bar)
{
    std::vector<int> steps;
    for (const Hit& hit : cell.hits)
    {
        if (hit.step / kStepsPerBar == bar)
        {
            steps.push_back(hit.step % kStepsPerBar);
        }
    }
    return steps;
}

const std::vector<RhythmCell>& learnedCells(const StyleEntry& entry, RhythmKind kind)
{
    return kind == RhythmKind::chords ? entry.chordRhythms : entry.melodyRhythms;
}

// The onsets per bar and the least gap over every learned cell of the family.
struct Learned
{
    int minOnsets = 99;
    int maxOnsets = 0;
    int minGap = 99;
};

Learned learnedOf(Family family, RhythmKind kind)
{
    Learned learned;
    for (const StyleEntry& entry : bank().entries)
    {
        for (const RhythmCell& cell : learnedCells(entry, kind))
        {
            if (entry.family != family)
            {
                continue;
            }
            for (int bar = 0; bar < cell.bars; ++bar)
            {
                const int count = static_cast<int>(barSteps(cell, bar).size());
                learned.minOnsets = std::min(learned.minOnsets, count);
                learned.maxOnsets = std::max(learned.maxOnsets, count);
            }
            for (std::size_t i = 1; i < cell.hits.size(); ++i)
            {
                learned.minGap =
                    std::min(learned.minGap, cell.hits.at(i).step - cell.hits.at(i - 1).step);
            }
        }
    }
    return learned;
}

// The onset steps within the beat of every chunk the family's cells have at each beat.
std::array<std::set<std::vector<int>>, kBeats> learnedBeats(Family family, RhythmKind kind)
{
    std::array<std::set<std::vector<int>>, kBeats> beats;
    for (const StyleEntry& entry : bank().entries)
    {
        for (const RhythmCell& cell : learnedCells(entry, kind))
        {
            for (int bar = 0; entry.family == family && bar < cell.bars; ++bar)
            {
                const auto steps = barSteps(cell, bar);
                for (int beat = 0; beat < kBeats; ++beat)
                {
                    std::vector<int> chunk;
                    for (const int step : steps)
                    {
                        if (step / kBeatSteps == beat)
                        {
                            chunk.push_back(step % kBeatSteps);
                        }
                    }
                    beats.at(static_cast<std::size_t>(beat)).insert(chunk);
                }
            }
        }
    }
    return beats;
}

bool writtenRhythm(Family family, RhythmKind kind, const RhythmCell& cell)
{
    return std::ranges::any_of(
        bank().entries,
        [&](const StyleEntry& entry)
        {
            return entry.family == family &&
                   std::ranges::any_of(learnedCells(entry, kind), [&](const RhythmCell& written)
                                       { return stepsOf(written) == stepsOf(cell); });
        });
}

constexpr std::array kKinds = {RhythmKind::chords, RhythmKind::melody};
} // namespace

TEST_CASE("invented rhythm cells over 500 seeds per family", "[invent]")
{
    SECTION("the model is named for its lane")
    {
        CHECK(buildRhythmModel(bank(), Family::soulful, RhythmKind::chords).name == "fresh-stabs");
        CHECK(buildRhythmModel(bank(), Family::soulful, RhythmKind::melody).name == "fresh-hook");
    }

    SECTION("cells are the right size, in the learned range and fall back rarely")
    {
        for (const Family family : kFamilies)
        {
            for (const RhythmKind kind : kKinds)
            {
                const Learned learned = learnedOf(family, kind);
                const Cells cells = inventedCells(family, kind, -1);
                for (const RhythmCell& cell : cells.made)
                {
                    CHECK(cell.bars == bars(kind));
                    CHECK(cell.id == (kind == RhythmKind::chords ? "fresh-stabs" : "fresh-hook"));
                    for (int bar = 0; bar < cell.bars; ++bar)
                    {
                        const int count = static_cast<int>(barSteps(cell, bar).size());
                        CHECK(count >= std::max(1, learned.minOnsets));
                        CHECK(count <= learned.maxOnsets + 1);
                    }
                }
                INFO("family " << static_cast<int>(family));
                CHECK(cells.fallbacks * 100 < kRhythmSeeds * 2);
            }
        }
    }

    SECTION("a chord bar has an onset in its first half and a melody cell at least 2 onsets")
    {
        for (const Family family : kFamilies)
        {
            for (const RhythmCell& cell : inventedCells(family, RhythmKind::chords, -1).made)
            {
                CHECK(cell.hits.front().step < kStepsPerBar / 2);
            }
            for (const RhythmCell& cell : inventedCells(family, RhythmKind::melody, -1).made)
            {
                CHECK(cell.hits.size() >= 2);
            }
        }
    }

    SECTION("every beat of an invented cell is a chunk learned at that beat")
    {
        for (const Family family : kFamilies)
        {
            for (const RhythmKind kind : kKinds)
            {
                const auto learned = learnedBeats(family, kind);
                for (const RhythmCell& cell : inventedCells(family, kind, -1).made)
                {
                    for (int bar = 0; bar < cell.bars; ++bar)
                    {
                        const auto steps = barSteps(cell, bar);
                        for (int beat = 0; beat < kBeats; ++beat)
                        {
                            std::vector<int> chunk;
                            for (const int step : steps)
                            {
                                if (step / kBeatSteps == beat)
                                {
                                    chunk.push_back(step % kBeatSteps);
                                }
                            }
                            CHECK(learned.at(static_cast<std::size_t>(beat)).contains(chunk));
                        }
                    }
                }
            }
        }
    }

    SECTION("lengths never reach the next onset or the end of the cell, and gaps stay learned")
    {
        for (const Family family : kFamilies)
        {
            for (const RhythmKind kind : kKinds)
            {
                const int minGap = learnedOf(family, kind).minGap;
                for (const RhythmCell& cell : inventedCells(family, kind, -1).made)
                {
                    for (std::size_t i = 0; i < cell.hits.size(); ++i)
                    {
                        const Hit& hit = cell.hits.at(i);
                        const int end = i + 1 < cell.hits.size() ? cell.hits.at(i + 1).step
                                                                 : cell.bars * kStepsPerBar;
                        CHECK(hit.length >= 1);
                        CHECK(hit.step + hit.length <= end);
                        CHECK((i + 1 == cell.hits.size() ||
                               cell.hits.at(i + 1).step - hit.step >= minGap));
                    }
                }
            }
        }
    }

    SECTION("energy 100 gives more onsets on average than energy 0")
    {
        const auto mean = [](const Cells& cells)
        {
            int hits = 0;
            for (const RhythmCell& cell : cells.made)
            {
                hits += static_cast<int>(cell.hits.size());
            }
            return static_cast<double>(hits) /
                   std::max(1.0, static_cast<double>(cells.made.size()));
        };
        for (const Family family : kFamilies)
        {
            for (const RhythmKind kind : kKinds)
            {
                CHECK(mean(inventedCells(family, kind, 100)) >
                      mean(inventedCells(family, kind, 0)));
            }
        }
    }

    SECTION("cells are mostly new and varied")
    {
        for (const Family family : kFamilies)
        {
            for (const RhythmKind kind : kKinds)
            {
                const Cells cells = inventedCells(family, kind, -1);
                const int count = static_cast<int>(cells.made.size());
                int fresh = 0;
                std::set<std::vector<std::pair<int, int>>> distinct;
                for (const RhythmCell& cell : cells.made)
                {
                    fresh += writtenRhythm(family, kind, cell) ? 0 : 1;
                    distinct.insert(stepsOf(cell));
                }
                INFO("family " << static_cast<int>(family));
                CHECK(fresh * 100 >= count * 60);
                CHECK(distinct.size() >= 15);
            }
        }
    }

    SECTION("a melody answer bar differs from its first bar")
    {
        for (const Family family : kFamilies)
        {
            for (const RhythmCell& cell : inventedCells(family, RhythmKind::melody, -1).made)
            {
                CHECK(barSteps(cell, 0) != barSteps(cell, 1));
            }
        }
    }

    SECTION("inventing a rhythm is deterministic")
    {
        for (const Family family : kFamilies)
        {
            for (const RhythmKind kind : kKinds)
            {
                const auto first = inventedCells(family, kind, 50);
                const auto second = inventedCells(family, kind, 50);
                REQUIRE(first.made.size() == second.made.size());
                for (std::size_t i = 0; i < first.made.size(); ++i)
                {
                    CHECK(stepsOf(first.made.at(i)) == stepsOf(second.made.at(i)));
                }
            }
        }
    }
}

TEST_CASE("chunk weights are 1000 a count plus 100, scaled by energy with a floor", "[invent]")
{
    RhythmModel model;
    model.chunks.front() = {{}, {{0, 1}, {1, 1}, {2, 1}, {3, 1}}};
    model.meanChunkOnsets = 5;
    const std::vector<int> counts = {2, 0};
    CHECK(chunkWeights(model, 0, counts, 50) == std::vector<int>{2100, 100});
    // Scale 1 + 0.6 * (energy / 100 - 0.5) * (onsets - 5): the empty chunk gains at energy 0 and
    // floors at 0.1 at energy 100, the 4-onset chunk moves the other way.
    CHECK(chunkWeights(model, 0, counts, 0) == std::vector<int>{5250, 130});
    CHECK(chunkWeights(model, 0, counts, 100) == std::vector<int>{210, 70});
}

TEST_CASE("a beat chunk follows the chunk it followed in the learned cells", "[invent]")
{
    const auto cell = [](int second, int third)
    {
        return RhythmCell{.id = "c",
                          .bars = 1,
                          .hits = {{.step = 0, .length = 1},
                                   {.step = second, .length = 1},
                                   {.step = third, .length = 1},
                                   {.step = 12, .length = 1}}};
    };
    StyleEntry entry{};
    entry.family = Family::mkDub;
    entry.chordRhythms = {cell(4, 8), cell(5, 10)};
    const Bank tiny{.entries = {entry}, .version = 0};
    const RhythmModel model = buildRhythmModel(tiny, Family::mkDub, RhythmKind::chords);
    const auto hasStep = [](const RhythmCell& made, int step)
    { return std::ranges::any_of(made.hits, [step](const Hit& hit) { return hit.step == step; }); };
    int afterFour = 0;
    int afterFive = 0;
    int pairedFour = 0;
    int pairedFive = 0;
    for (int seed = 0; seed < kRhythmSeeds; ++seed)
    {
        Pcg32 rng = rhythmRng(seed);
        const auto invented = inventRhythm(model, 1, 50, rng);
        REQUIRE(invented.has_value());
        const RhythmCell made = invented.value_or(RhythmCell{});
        afterFour += hasStep(made, 4) ? 1 : 0;
        afterFive += hasStep(made, 5) ? 1 : 0;
        pairedFour += hasStep(made, 4) && hasStep(made, 8) ? 1 : 0;
        pairedFive += hasStep(made, 5) && hasStep(made, 10) ? 1 : 0;
    }
    REQUIRE(afterFour > 0);
    REQUIRE(afterFive > 0);
    CHECK(pairedFour * 100 >= afterFour * 80);
    CHECK(pairedFive * 100 >= afterFive * 80);
}

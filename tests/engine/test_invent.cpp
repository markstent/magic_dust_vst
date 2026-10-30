#include "internal.hpp"
#include "md/bank.hpp"
#include "md/pcg32.hpp"
#include "md/theory.hpp"

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
    ProgressionModel model;
    std::optional<Progression> progression;
};

Pcg32 rngFor(int seed)
{
    return {laneSeed(static_cast<std::uint32_t>(seed), Stream::chords, 0), kChordProgression};
}

// One invention per seed, each entry of the family in turn, from the model of its first mode's
// group.
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
        ProgressionModel model =
            buildProgressionModel(bank(), family, modeGroup(entry.modes.front()));
        Pcg32 rng = rngFor(seed);
        auto progression = inventProgression(model, entry, rng);
        it->second.push_back(
            {.entry = &entry, .model = std::move(model), .progression = std::move(progression)});
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
            CHECK(smoothed * 100 < chords * 15);
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

    SECTION("invented progressions never repeat a chord back to back")
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
                    CHECK_FALSE(chords.at(i) == chords.at((i + 1) % chords.size()));
                }
            }
        }
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

#include "md/bank.hpp"
#include "md/generate.hpp"
#include "md/midifile.hpp"
#include "md/validate.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

using namespace md;

namespace
{
constexpr int kDefaultSeeds = 1000;
constexpr int kDeterminismSeeds = 50;
constexpr double kP99LimitMs = 20.0;

// Unset or empty means the default; anything else must be a positive whole number.
int seedCount()
{
    const char* text = std::getenv("MD_PROPERTY_SEEDS");
    if (text == nullptr || *text == '\0')
    {
        return kDefaultSeeds;
    }
    char* end = nullptr;
    const long count = std::strtol(text, &end, 10);
    INFO("MD_PROPERTY_SEEDS must be a positive whole number, got '" << text << "'");
    REQUIRE((*end == '\0' && count > 0 && count <= 100000));
    return static_cast<int>(count);
}

std::vector<std::uint8_t> midiBytes(const Idea& idea)
{
    return writeMidiFile(ideaTracks(idea), 124, ideaLengthTicks(idea));
}
} // namespace

TEST_CASE("properties: every generated idea is valid and generation is deterministic",
          "[properties]")
{
    const Bank bank = loadBundledBank();
    constexpr std::array<Style, 6> styles = {
        Style::ukFunky,  Style::mkDub,    Style::soulful,
        Style::bassline, Style::festival, Style::nyGroove,
    };
    constexpr std::array<std::pair<int, int>, 4> corners = {
        std::pair{0, 0},
        std::pair{0, 100},
        std::pair{100, 0},
        std::pair{100, 100},
    };
    constexpr std::array<int, 3> bars = {4, 8, 16};

    const int seeds = seedCount();
    std::vector<double> millis;
    for (int seed = 0; seed < seeds; ++seed)
    {
        for (const Style style : styles)
        {
            for (const auto& [mood, energy] : corners)
            {
                Settings settings;
                settings.style = style;
                settings.keyRoot = seed % 12;
                settings.mood = mood;
                settings.energy = energy;
                settings.groove = static_cast<Groove>(seed % 4);
                settings.flavour = static_cast<Flavour>(seed % 3);
                settings.bars = bars.at(static_cast<std::size_t>(seed % 3));

                const auto start = std::chrono::steady_clock::now();
                const Idea idea = generate(bank, settings, static_cast<std::uint32_t>(seed));
                millis.push_back(std::chrono::duration<double, std::milli>(
                                     std::chrono::steady_clock::now() - start)
                                     .count());

                const auto problems = validate(idea);
                INFO("seed " << seed << " style " << static_cast<int>(style) << " mood " << mood
                             << " energy " << energy);
                REQUIRE(problems.empty());

                if (seed < kDeterminismSeeds)
                {
                    const Idea again = generate(bank, settings, static_cast<std::uint32_t>(seed));
                    REQUIRE(again.entryId == idea.entryId);
                    REQUIRE(midiBytes(again) == midiBytes(idea));
                }
            }
        }
    }

#ifdef MD_PERF_ASSERT
    std::ranges::sort(millis);
    const double p99 = millis.at(millis.size() * 99 / 100);
    WARN("p99 per idea: " << p99 << " ms over " << millis.size() << " ideas");
    REQUIRE(p99 < kP99LimitMs);
#else
    static_cast<void>(kP99LimitMs);
#endif
}

TEST_CASE("properties: bass has no violations across style, flavour, energy and groove",
          "[properties]")
{
    const Bank bank = loadBundledBank();
    constexpr std::array<Style, 6> styles = {
        Style::ukFunky,  Style::mkDub,    Style::soulful,
        Style::bassline, Style::festival, Style::nyGroove,
    };
    constexpr std::array<int, 6> energies = {0, 25, 50, 51, 75, 100};

    const int seeds = seedCount();
    for (int seed = 0; seed < seeds; ++seed)
    {
        for (const Style style : styles)
        {
            for (int flavour = 0; flavour < 3; ++flavour)
            {
                for (const int energy : energies)
                {
                    for (int groove = 0; groove < 4; ++groove)
                    {
                        Settings settings;
                        settings.style = style;
                        settings.keyRoot = seed % 12;
                        settings.energy = energy;
                        settings.groove = static_cast<Groove>(groove);
                        settings.flavour = static_cast<Flavour>(flavour);
                        settings.bars = 8;

                        const Idea idea =
                            generate(bank, settings, static_cast<std::uint32_t>(seed));
                        const auto problems = validate(idea);
                        INFO("seed " << seed << " style " << static_cast<int>(style) << " flavour "
                                     << flavour << " energy " << energy << " groove " << groove
                                     << ": " << (problems.empty() ? "none" : problems.front()));
                        REQUIRE(problems.empty());
                    }
                }
            }
        }
    }
}

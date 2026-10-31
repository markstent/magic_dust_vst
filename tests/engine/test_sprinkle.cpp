#include "md/generate.hpp"

#include "internal.hpp"
#include "md/bank.hpp"
#include "md/ranges.hpp"
#include "md/validate.hpp"
#include "shared_bank.hpp"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <set>
#include <stdexcept>
#include <utility>
#include <vector>

using namespace md;
using namespace md::detail;

namespace
{
constexpr std::array kStyles = {
    Style::ukFunky, Style::mkDub, Style::soulful, Style::bassline, Style::festival, Style::nyGroove,
};
constexpr std::array kBars = {4, 8, 16};
constexpr int kSeeds = 400;
// The property test's mood and energy corners.
constexpr std::array kCorners = {
    std::pair{0, 0},
    std::pair{0, 100},
    std::pair{100, 0},
    std::pair{100, 100},
};

const Bank& bank() { return sharedBank(); }

const Models& models() { return bank().models.get(bank()); }

const StyleEntry& entryOf(const Idea& idea)
{
    const auto it = std::ranges::find(bank().entries, idea.entryId, &StyleEntry::id);
    REQUIRE(it != bank().entries.end());
    return *it;
}

// The settings the property test sweeps, for a seed, style and (mood, energy) corner.
Settings sweep(int seed, Style style, std::pair<int, int> corner)
{
    const auto [mood, energy] = corner;
    Settings settings;
    settings.style = style;
    settings.keyRoot = seed % 12;
    settings.mood = mood;
    settings.energy = energy;
    settings.groove = static_cast<Groove>(seed % 4);
    settings.flavour = static_cast<Flavour>(seed % 3);
    settings.bars = kBars.at(static_cast<std::size_t>(seed % 3));
    return settings;
}

Settings varied(int seed)
{
    return sweep(seed, kStyles.at(static_cast<std::size_t>(seed) % kStyles.size()),
                 {(seed * 37) % 101, (seed * 53) % 101});
}

bool written(const StyleEntry& entry, const Progression& progression)
{
    return std::ranges::any_of(entry.progressions, [&](const Progression& p)
                               { return p.chords == progression.chords; });
}

// The sources of an idea's chord progression, chord rhythm and melody rhythm, all fallback when
// the invented idea fails validate.
struct Sources
{
    Source progression;
    Source chords;
    Source melody;
};

Sources sourcesOf(const Settings& settings, std::uint32_t seed)
{
    const LaneSeeds lanes = defaultLaneSeeds(seed);
    try
    {
        const Idea idea = generateWith(bank(), settings, seed, lanes, Idea{}, Pins{}, Invent{});
        const StyleEntry& entry = entryOf(idea);
        return {
            .progression = chooseProgression(&models(), entry, lanes.chords).source,
            .chords = chooseChordRhythm(&models(), entry, settings, lanes.chords).source,
            .melody = chooseMelodyRhythm(&models(), entry, settings, lanes.melody).source,
        };
    }
    catch (const InvalidIdea&)
    {
        return {.progression = Source::fallback,
                .chords = Source::fallback,
                .melody = Source::fallback};
    }
}

bool fellBack(const Sources& sources)
{
    return sources.progression == Source::fallback || sources.chords == Source::fallback ||
           sources.melody == Source::fallback;
}

std::set<int> stepsOf(const std::vector<Hit>& hits)
{
    std::set<int> steps;
    for (const Hit& hit : hits)
    {
        steps.insert(hit.step);
    }
    return steps;
}

// The hit steps of a lane's events in [firstBar, firstBar + bars), relative to firstBar, unswung.
std::set<int> laneSteps(const Lane& lane, int firstBar, int bars)
{
    std::set<int> steps;
    for (const NoteEvent& e : lane.events)
    {
        const int bar = e.start / kTicksPerBar;
        if (bar >= firstBar && bar < firstBar + bars)
        {
            steps.insert(((bar - firstBar) * kStepsPerBar) + stepOf(e.start % kTicksPerBar));
        }
    }
    return steps;
}
} // namespace

TEST_CASE("sprinkle invents most progressions and rhythms and keeps about a fifth written",
          "[sprinkle]")
{
    std::array<int, 3> writtenShare{};
    int freshChords = 0;
    int freshProgressions = 0;
    for (int seed = 0; seed < kSeeds; ++seed)
    {
        const Settings settings = varied(seed);
        const auto id = static_cast<std::uint32_t>(seed);
        const Idea idea = generate(bank(), settings, id);
        const StyleEntry& entry = entryOf(idea);
        const LaneSeeds lanes = defaultLaneSeeds(id);
        const std::array sources = {
            chooseProgression(&models(), entry, lanes.chords).source,
            chooseChordRhythm(&models(), entry, settings, lanes.chords).source,
            chooseMelodyRhythm(&models(), entry, settings, lanes.melody).source,
        };
        for (std::size_t i = 0; i < sources.size(); ++i)
        {
            writtenShare.at(i) += sources.at(i) == Source::written ? 1 : 0;
        }
        freshChords += idea.lanes.at(0).pattern == "fresh-stabs" ? 1 : 0;
        const auto progression = chooseProgression(&models(), entry, lanes.chords).progression;
        freshProgressions += written(entry, progression) ? 0 : 1;
    }
    for (const int share : writtenShare)
    {
        CHECK(share * 100 >= kSeeds * 15);
        CHECK(share * 100 <= kSeeds * 25);
    }
    // About 80% invented, less the rare fallback.
    CHECK(freshChords * 100 >= kSeeds * 70);
    CHECK(freshChords * 100 <= kSeeds * 85);
    // An invented progression can match a written one, so a little under 80% are new.
    CHECK(freshProgressions * 100 >= kSeeds * 50);
}

TEST_CASE("generate plays the chosen progression and the invented cells", "[sprinkle]")
{
    int invented = 0;
    for (int seed = 0; seed < kSeeds; ++seed)
    {
        Settings settings = varied(seed);
        settings.flavour = Flavour::soulful; // keeps the chords as chosen
        const auto id = static_cast<std::uint32_t>(seed);
        const Idea idea = generate(bank(), settings, id);
        const StyleEntry& entry = entryOf(idea);
        const LaneSeeds lanes = defaultLaneSeeds(id);
        const auto chosen = chooseProgression(&models(), entry, lanes.chords);
        const auto rhythm = chooseChordRhythm(&models(), entry, settings, lanes.chords);
        if (chosen.source != Source::invented || rhythm.source != Source::invented ||
            idea.lanes.at(0).pattern != "fresh-stabs")
        {
            continue;
        }
        ++invented;
        for (std::size_t i = 0; i < idea.progression.size(); ++i)
        {
            const std::size_t written = i % chosen.progression.chords.size();
            REQUIRE(idea.progression.at(i).symbol == chosen.progression.chords.at(written));
        }
        // Bar 1 plays the invented cell: it is never a turnaround or a breath bar, and its
        // ghosts land on odd steps the cell does not use.
        const std::set<int> played = laneSteps(idea.lanes.at(0), 0, 1);
        for (const int step : stepsOf(rhythm.cell.hits))
        {
            REQUIRE(played.contains(step));
        }
    }
    REQUIRE(invented * 100 >= kSeeds * 50);
}

TEST_CASE("the turnaround and the melody answer are invented alternates", "[sprinkle][variety]")
{
    int turned = 0;
    int answered = 0;
    for (int seed = 0; seed < kSeeds; ++seed)
    {
        Settings settings = varied(seed);
        settings.energy = 50; // turnaround and answer, no breath, ghosts or fill
        settings.bars = 8;
        const auto id = static_cast<std::uint32_t>(seed);
        const Idea idea = generate(bank(), settings, id);
        const StyleEntry& entry = entryOf(idea);
        const LaneSeeds lanes = defaultLaneSeeds(id);
        const auto chords = chooseChordRhythm(&models(), entry, settings, lanes.chords);
        if (chords.source == Source::invented && idea.lanes.at(0).pattern == "fresh-stabs")
        {
            ++turned;
            REQUIRE(chords.turnarounds.size() == 1);
            const auto turn = stepsOf(chords.turnarounds.front());
            REQUIRE(turn != stepsOf(chords.cell.hits));
            REQUIRE(laneSteps(idea.lanes.at(0), settings.bars - 1, 1) == turn);
        }
        const auto melody = chooseMelodyRhythm(&models(), entry, settings, lanes.melody);
        if (melody.source != Source::invented)
        {
            continue;
        }
        const auto a = stepsOf(melody.a.hits);
        const auto answer = stepsOf(melody.answer.hits);
        REQUIRE(melody.answers);
        REQUIRE(a != stepsOf(melody.b.hits));
        REQUIRE(answer != a);
        REQUIRE(answer != stepsOf(melody.b.hits));
        ++answered;
        REQUIRE(laneSteps(idea.lanes.at(2), 6, 2) == answer);
        REQUIRE(laneSteps(idea.lanes.at(2), 0, 2) == a);
    }
    CHECK(turned * 100 >= kSeeds * 60);
    CHECK(answered * 100 >= kSeeds * 50);
}

namespace
{
std::vector<std::pair<int, int>> hitsOf(const std::vector<Hit>& hits)
{
    std::vector<std::pair<int, int>> out;
    out.reserve(hits.size());
    for (const Hit& hit : hits)
    {
        out.emplace_back(hit.step, hit.length);
    }
    return out;
}
} // namespace

TEST_CASE("energy within a tier keeps the invented rhythms", "[sprinkle]")
{
    int compared = 0;
    int written = 0;
    for (int seed = 0; seed < kSeeds; ++seed)
    {
        for (const auto& [low, high] : {std::pair{31, 59}, std::pair{61, 100}})
        {
            Settings lower = varied(seed);
            lower.energy = low;
            Settings upper = lower;
            upper.energy = high;
            const auto id = static_cast<std::uint32_t>(seed);
            const LaneSeeds lanes = defaultLaneSeeds(id);
            const StyleEntry& entry = entryOf(generate(bank(), lower, id));
            const auto chordsLow = chooseChordRhythm(&models(), entry, lower, lanes.chords);
            const auto chordsHigh = chooseChordRhythm(&models(), entry, upper, lanes.chords);
            REQUIRE(chordsLow.source == chordsHigh.source);
            REQUIRE(hitsOf(chordsLow.cell.hits) == hitsOf(chordsHigh.cell.hits));
            REQUIRE(chordsLow.turnarounds.size() == chordsHigh.turnarounds.size());
            for (std::size_t n = 0; n < chordsLow.turnarounds.size(); ++n)
            {
                REQUIRE(hitsOf(chordsLow.turnarounds.at(n)) ==
                        hitsOf(chordsHigh.turnarounds.at(n)));
            }
            const auto melodyLow = chooseMelodyRhythm(&models(), entry, lower, lanes.melody);
            const auto melodyHigh = chooseMelodyRhythm(&models(), entry, upper, lanes.melody);
            REQUIRE(melodyLow.source == melodyHigh.source);
            compared += melodyLow.source == Source::invented ? 1 : 0;
            written += melodyLow.source == Source::invented ? 0 : 1;
            REQUIRE(hitsOf(melodyLow.a.hits) == hitsOf(melodyHigh.a.hits));
            REQUIRE(hitsOf(melodyLow.b.hits) == hitsOf(melodyHigh.b.hits));
            REQUIRE(hitsOf(melodyLow.answer.hits) == hitsOf(melodyHigh.answer.hits));
        }
    }
    REQUIRE(compared > 0);
    REQUIRE(written > 0);
}

TEST_CASE("a key change keeps the degrees and bar lengths", "[sprinkle]")
{
    for (int seed = 0; seed < kSeeds; ++seed)
    {
        Settings settings = varied(seed);
        const auto id = static_cast<std::uint32_t>(seed);
        settings.keyRoot = 0;
        const Idea first = generate(bank(), settings, id);
        for (int root = 1; root < kPitchClasses; ++root)
        {
            settings.keyRoot = root;
            const Idea other = generate(bank(), settings, id);
            REQUIRE(other.entryId == first.entryId);
            REQUIRE(other.progression.size() == first.progression.size());
            for (std::size_t i = 0; i < first.progression.size(); ++i)
            {
                REQUIRE(other.progression.at(i).symbol.degree ==
                        first.progression.at(i).symbol.degree);
                REQUIRE(other.progression.at(i).bars == first.progression.at(i).bars);
            }
        }
    }
}

TEST_CASE("pinned chords keep their invented progression and rhythm", "[sprinkle][pins]")
{
    int kept = 0;
    for (int seed = 0; seed < 60; ++seed)
    {
        const Settings settings = varied(seed);
        const auto id = static_cast<std::uint32_t>(seed);
        const Idea first = generate(bank(), settings, id);
        if (first.lanes.at(0).pattern != "fresh-stabs")
        {
            continue;
        }
        ++kept;
        Settings louder = settings;
        louder.energy = (settings.energy + 40) % 101;
        const auto nextSeed = id + 1000;
        const Idea next =
            generate(bank(), louder, nextSeed, defaultLaneSeeds(nextSeed), first, {.chords = true});
        REQUIRE(next.entryId == first.entryId);
        REQUIRE(next.progression.size() == first.progression.size());
        for (std::size_t i = 0; i < next.progression.size(); ++i)
        {
            REQUIRE(next.progression.at(i).symbol == first.progression.at(i).symbol);
            REQUIRE(next.progression.at(i).voicing == first.progression.at(i).voicing);
        }
        REQUIRE(next.lanes.at(0).pattern == first.lanes.at(0).pattern);
        REQUIRE(next.lanes.at(0).events == first.lanes.at(0).events);
        REQUIRE(validate(next).empty());
    }
    REQUIRE(kept > 0);
}

TEST_CASE("bench: generate time, fallback rate and written share", "[.bench]")
{
    constexpr int kTimed = 1000;
    double total = 0;
    for (int seed = 0; seed < kTimed; ++seed)
    {
        const Settings settings = varied(seed);
        const auto start = std::chrono::steady_clock::now();
        const Idea idea = generate(bank(), settings, static_cast<std::uint32_t>(seed));
        total += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start)
                     .count();
        REQUIRE(validate(idea).empty());
    }
    WARN("mean per idea: " << total / kTimed << " ms over " << kTimed << " ideas");

    int ideas = 0;
    int fallbacks = 0;
    std::array<int, 3> writtenShare{};
    for (int seed = 0; seed < 1000; ++seed)
    {
        for (const Style style : kStyles)
        {
            for (const auto& corner : kCorners)
            {
                const Sources sources =
                    sourcesOf(sweep(seed, style, corner), static_cast<std::uint32_t>(seed));
                ++ideas;
                fallbacks += fellBack(sources) ? 1 : 0;
                writtenShare.at(0) += sources.progression == Source::written ? 1 : 0;
                writtenShare.at(1) += sources.chords == Source::written ? 1 : 0;
                writtenShare.at(2) += sources.melody == Source::written ? 1 : 0;
            }
        }
    }
    WARN("fallback: " << fallbacks << " of " << ideas << " ideas; written share: progression "
                      << writtenShare.at(0) << ", chord rhythm " << writtenShare.at(1)
                      << ", melody rhythm " << writtenShare.at(2));
}

TEST_CASE("only an invalid idea falls back; other errors on the invented path propagate",
          "[sprinkle]")
{
    const auto valid = std::ranges::find(bank().entries, Family::soulful, &StyleEntry::family);
    const auto other = std::ranges::find(bank().entries, Family::mkDub, &StyleEntry::family);
    REQUIRE(valid != bank().entries.end());
    REQUIRE(other != bank().entries.end());
    // A hit past its 1-bar cell, which the bank loader would reject, breaks the mk dub model.
    StyleEntry broken = *other;
    broken.chordRhythms.front().bars = 1;
    broken.chordRhythms.front().hits.push_back({.step = 40, .length = 1});
    const Bank mixed = makeBank({*valid, broken});
    Settings settings;
    settings.style = Style::soulful;
    REQUIRE_THROWS_AS(generate(mixed, settings, 1), std::out_of_range);
}

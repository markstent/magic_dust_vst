#include "IdeaState.h"

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "md/ranges.hpp"

namespace
{

constexpr const char* kIdea = "Idea";
constexpr const char* kChord = "Chord";
constexpr const char* kLane = "Lane";

constexpr std::size_t kEventInts = 4;

template <class T> std::optional<T> parseNumber(const juce::String& text)
{
    const std::string s = text.toStdString();
    T value{};
    // NOLINTBEGIN(cppcoreguidelines-pro-bounds-pointer-arithmetic): from_chars takes a range
    const auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    if (ec != std::errc{} || end != s.data() + s.size())
    // NOLINTEND(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    {
        return std::nullopt;
    }
    return value;
}

// Space-separated integers.
juce::String joinInts(const std::vector<int>& values)
{
    juce::StringArray parts;
    for (const int v : values)
    {
        parts.add(juce::String(v));
    }
    return parts.joinIntoString(" ");
}

std::optional<std::vector<int>> splitInts(const juce::String& text)
{
    std::vector<int> out;
    for (const auto& part : juce::StringArray::fromTokens(text, " ", ""))
    {
        const auto v = parseNumber<int>(part);
        if (!v)
        {
            return std::nullopt;
        }
        out.push_back(*v);
    }
    return out;
}

// Reads one property, then checks it against its range.
std::optional<int> intIn(const juce::ValueTree& t, const char* name, int lo, int hi)
{
    // An XML round trip turns every property into text, so parse rather than test the type.
    const auto n = parseNumber<int>(t.getProperty(name).toString());
    return n && *n >= lo && *n <= hi ? n : std::nullopt;
}

std::optional<bool> boolOf(const juce::ValueTree& t, const char* name)
{
    const auto n = intIn(t, name, 0, 1);
    return n ? std::optional<bool>(*n == 1) : std::nullopt;
}

std::optional<std::uint64_t> seedOf(const juce::ValueTree& t, const char* name)
{
    return parseNumber<std::uint64_t>(t.getProperty(name).toString());
}

juce::ValueTree laneToTree(const md::Lane& lane)
{
    juce::ValueTree t(kLane);
    t.setProperty("id", static_cast<int>(lane.id), nullptr);
    t.setProperty("pattern", juce::String(lane.pattern), nullptr);
    std::vector<int> flat;
    for (const md::NoteEvent& e : lane.events)
    {
        flat.insert(flat.end(), {e.pitch, e.start, e.length, e.velocity});
    }
    t.setProperty("events", joinInts(flat), nullptr);
    t.setProperty("muted", lane.muted ? 1 : 0, nullptr);
    t.setProperty("seed", juce::String(lane.seed), nullptr);
    t.setProperty("refitted", lane.refitted ? 1 : 0, nullptr);
    return t;
}

std::optional<md::Lane> laneFromTree(const juce::ValueTree& t)
{
    const auto id = intIn(t, "id", 0, 2);
    const auto events = splitInts(t.getProperty("events").toString());
    const auto muted = boolOf(t, "muted");
    const auto seed = seedOf(t, "seed");
    const auto refitted = boolOf(t, "refitted");
    if (!id || !events || events->size() % kEventInts != 0 || !muted || !seed || !refitted ||
        !t.hasProperty("pattern"))
    {
        return std::nullopt;
    }
    md::Lane lane{.id = static_cast<md::LaneId>(*id),
                  .pattern = t.getProperty("pattern").toString().toStdString(),
                  .events = {},
                  .muted = *muted,
                  .seed = *seed,
                  .refitted = *refitted};
    for (std::size_t i = 0; i < events->size(); i += kEventInts)
    {
        lane.events.push_back(md::NoteEvent{.pitch = events->at(i),
                                            .start = events->at(i + 1),
                                            .length = events->at(i + 2),
                                            .velocity = events->at(i + 3)});
    }
    return lane;
}

juce::ValueTree chordToTree(const md::PlacedChord& chord)
{
    juce::ValueTree t(kChord);
    t.setProperty("degree", chord.symbol.degree, nullptr);
    t.setProperty("quality", static_cast<int>(chord.symbol.quality), nullptr);
    t.setProperty("feel", juce::String(chord.feel), nullptr);
    t.setProperty("startBar", chord.startBar, nullptr);
    t.setProperty("bars", chord.bars, nullptr);
    t.setProperty("voicing", joinInts(chord.voicing), nullptr);
    return t;
}

std::optional<md::PlacedChord> chordFromTree(const juce::ValueTree& t)
{
    const auto degree = intIn(t, "degree", 1, 7);
    const auto quality = intIn(t, "quality", 0, static_cast<int>(md::Quality::add9));
    const auto startBar = intIn(t, "startBar", 0, 15);
    const auto bars = intIn(t, "bars", 1, 16);
    const auto voicing = splitInts(t.getProperty("voicing").toString());
    if (!degree || !quality || !startBar || !bars || !voicing || !t.hasProperty("feel"))
    {
        return std::nullopt;
    }
    return md::PlacedChord{
        .symbol = {.degree = *degree, .quality = static_cast<md::Quality>(*quality)},
        .feel = t.getProperty("feel").toString().toStdString(),
        .startBar = *startBar,
        .bars = *bars,
        .voicing = *voicing};
}

std::optional<md::Settings> settingsFromTree(const juce::ValueTree& t)
{
    const auto style = intIn(t, "style", 0, static_cast<int>(md::Style::nyGroove));
    const auto keyRoot = intIn(t, "keyRoot", 0, 11);
    const auto mood = intIn(t, "mood", 0, 100);
    const auto energy = intIn(t, "energy", 0, 100);
    const auto groove = intIn(t, "groove", 0, static_cast<int>(md::Groove::heavy62));
    const auto flavour = intIn(t, "flavour", 0, static_cast<int>(md::Flavour::jazzy));
    const auto bars = intIn(t, "bars", 1, 16);
    if (!style || !keyRoot || !mood || !energy || !groove || !flavour || !bars)
    {
        return std::nullopt;
    }
    return md::Settings{.style = static_cast<md::Style>(*style),
                        .keyRoot = *keyRoot,
                        .mood = *mood,
                        .energy = *energy,
                        .groove = static_cast<md::Groove>(*groove),
                        .flavour = static_cast<md::Flavour>(*flavour),
                        .bars = *bars};
}

} // namespace

juce::ValueTree ideaToTree(const md::Idea& idea)
{
    juce::ValueTree t(kIdea);
    t.setProperty("seed", static_cast<int>(idea.seed), nullptr);
    const md::Settings& s = idea.settings;
    t.setProperty("style", static_cast<int>(s.style), nullptr);
    t.setProperty("keyRoot", s.keyRoot, nullptr);
    t.setProperty("mood", s.mood, nullptr);
    t.setProperty("energy", s.energy, nullptr);
    t.setProperty("groove", static_cast<int>(s.groove), nullptr);
    t.setProperty("flavour", static_cast<int>(s.flavour), nullptr);
    t.setProperty("bars", s.bars, nullptr);
    t.setProperty("key", idea.key.root, nullptr);
    t.setProperty("mode", static_cast<int>(idea.key.mode), nullptr);
    t.setProperty("borrowedFrom", idea.borrowedFrom ? static_cast<int>(*idea.borrowedFrom) : -1,
                  nullptr);
    t.setProperty("bankVersion", juce::String(idea.bankVersion), nullptr);
    t.setProperty("entryId", juce::String(idea.entryId), nullptr);
    t.setProperty("parallel", idea.parallel ? 1 : 0, nullptr);
    for (const md::PlacedChord& chord : idea.progression)
    {
        t.appendChild(chordToTree(chord), nullptr);
    }
    for (const md::Lane& lane : idea.lanes)
    {
        t.appendChild(laneToTree(lane), nullptr);
    }
    return t;
}

std::optional<md::Idea> ideaFromTree(const juce::ValueTree& tree)
{
    if (!tree.hasType(kIdea))
    {
        return std::nullopt;
    }
    const auto seed = intIn(tree, "seed", 0, md::kSeedMax);
    const auto settings = settingsFromTree(tree);
    const auto root = intIn(tree, "key", 0, 11);
    const auto mode = intIn(tree, "mode", 0, static_cast<int>(md::Mode::ionian));
    const auto borrowed = intIn(tree, "borrowedFrom", -1, static_cast<int>(md::Mode::ionian));
    const auto bankVersion = seedOf(tree, "bankVersion");
    const auto parallel = boolOf(tree, "parallel");
    if (!seed || !settings || !root || !mode || !borrowed || !bankVersion || !parallel ||
        !tree.hasProperty("entryId") || tree.getNumChildren() < 3)
    {
        return std::nullopt;
    }
    md::Idea idea{};
    idea.seed = static_cast<std::uint32_t>(*seed);
    idea.settings = *settings;
    idea.key = {.root = *root, .mode = static_cast<md::Mode>(*mode)};
    if (*borrowed >= 0)
    {
        idea.borrowedFrom = static_cast<md::Mode>(*borrowed);
    }
    idea.bankVersion = *bankVersion;
    idea.entryId = tree.getProperty("entryId").toString().toStdString();
    idea.parallel = *parallel;
    const int chordCount = tree.getNumChildren() - static_cast<int>(idea.lanes.size());
    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        const juce::ValueTree child = tree.getChild(i);
        if (i < chordCount)
        {
            const auto chord = child.hasType(kChord) ? chordFromTree(child) : std::nullopt;
            if (!chord)
            {
                return std::nullopt;
            }
            idea.progression.push_back(*chord);
        }
        else
        {
            const auto lane = child.hasType(kLane) ? laneFromTree(child) : std::nullopt;
            const auto slot = static_cast<std::size_t>(i - chordCount);
            if (!lane || static_cast<std::size_t>(lane->id) != slot)
            {
                return std::nullopt;
            }
            idea.lanes.at(slot) = *lane;
        }
    }
    return idea;
}

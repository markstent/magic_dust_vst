#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdint>

#include <juce_data_structures/juce_data_structures.h>

#include "md/bank.hpp"
#include "md/generate.hpp"
#include "plugin/IdeaState.h"

namespace
{

constexpr int kIdeas = 50;

bool sameIdea(const md::Idea& a, const md::Idea& b)
{
    const auto sameSettings = [](const md::Settings& x, const md::Settings& y)
    {
        return x.style == y.style && x.keyRoot == y.keyRoot && x.mood == y.mood &&
               x.energy == y.energy && x.groove == y.groove && x.flavour == y.flavour &&
               x.bars == y.bars;
    };
    const auto sameLane = [](const md::Lane& x, const md::Lane& y)
    {
        return x.id == y.id && x.pattern == y.pattern && x.events == y.events &&
               x.muted == y.muted && x.seed == y.seed && x.refitted == y.refitted;
    };
    const auto sameChord = [](const md::PlacedChord& x, const md::PlacedChord& y)
    {
        return x.symbol == y.symbol && x.feel == y.feel && x.startBar == y.startBar &&
               x.bars == y.bars && x.voicing == y.voicing;
    };
    if (a.seed != b.seed || !sameSettings(a.settings, b.settings) || a.key.root != b.key.root ||
        a.key.mode != b.key.mode || a.borrowedFrom != b.borrowedFrom ||
        a.bankVersion != b.bankVersion || a.entryId != b.entryId || a.parallel != b.parallel ||
        a.progression.size() != b.progression.size())
    {
        return false;
    }
    for (std::size_t i = 0; i < a.progression.size(); ++i)
    {
        if (!sameChord(a.progression.at(i), b.progression.at(i)))
        {
            return false;
        }
    }
    for (std::size_t i = 0; i < a.lanes.size(); ++i)
    {
        if (!sameLane(a.lanes.at(i), b.lanes.at(i)))
        {
            return false;
        }
    }
    return true;
}

md::Idea pinnedIdea(std::uint32_t seed)
{
    static const md::Bank bank = md::loadBundledBank();
    const md::Settings settings;
    const md::Idea first = md::generate(bank, settings, seed, md::defaultLaneSeeds(seed));
    md::Settings moved = settings;
    moved.keyRoot = 2;
    return md::generate(bank, moved, seed + 1, md::defaultLaneSeeds(seed + 1), first,
                        {.chords = false, .bass = true, .melody = true});
}

} // namespace

TEST_CASE("an idea survives ideaToTree and ideaFromTree, also through xml", "[idea-state]")
{
    for (std::uint32_t seed = 1; seed <= kIdeas; ++seed)
    {
        const md::Idea idea = pinnedIdea(seed * 7);
        const auto direct = ideaFromTree(ideaToTree(idea));
        CHECK((direct && sameIdea(*direct, idea)));

        const auto xml = ideaToTree(idea).createXml();
        const auto viaXml = ideaFromTree(juce::ValueTree::fromXml(*xml));
        CHECK((viaXml && sameIdea(*viaXml, idea)));
    }
}

TEST_CASE("a tree that is not an idea is refused", "[idea-state]")
{
    CHECK_FALSE(ideaFromTree(juce::ValueTree("Other")).has_value());
    CHECK_FALSE(ideaFromTree(juce::ValueTree("Idea")).has_value());
    juce::ValueTree broken = ideaToTree(pinnedIdea(5));
    broken.removeChild(broken.getNumChildren() - 1, nullptr);
    CHECK_FALSE(ideaFromTree(broken).has_value());
    juce::ValueTree badNumber = ideaToTree(pinnedIdea(5));
    badNumber.setProperty("mood", "loud", nullptr);
    CHECK_FALSE(ideaFromTree(badNumber).has_value());
}

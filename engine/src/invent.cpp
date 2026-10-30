#include "internal.hpp"
#include "md/theory.hpp"
#include "md/validate.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <stdexcept>
#include <utility>

namespace md::detail
{
namespace
{
constexpr int kTries = 24;
// Weights in thousandths: a learned count is 1, smoothing adds 0.15 between learned tokens and
// gives 0.05 to any move to or from a smoothing token.
constexpr int kCountWeight = 1000;
constexpr int kLearnedSmoothing = 150;
constexpr int kNewSmoothing = 50;
// The longest idea, which plays every chord of any shorter one and the wrap of a short template.
constexpr int kLongestIdea = 16;
constexpr std::array kFlavours = {Flavour::simple, Flavour::soulful, Flavour::jazzy};

bool inGroup(const StyleEntry& entry, ModeGroup group)
{
    return std::ranges::any_of(entry.modes,
                               [group](Mode mode) { return modeGroup(mode) == group; });
}

// A quality with a 7th or a 9th over its root.
bool carriesSeventh(Quality quality)
{
    const auto intervals = chordIntervals(quality);
    return std::ranges::any_of(intervals, [](int interval)
                               { return interval == 10 || interval == 11 || interval == 14; });
}

std::size_t indexOf(const std::vector<ChordSymbol>& tokens, const ChordSymbol& chord)
{
    return static_cast<std::size_t>(std::ranges::find(tokens, chord) - tokens.begin());
}

// A learned count plus smoothing for a move between states (learned tokens first, then smoothing
// tokens), and 0 for staying on the same chord.
int moveWeight(const ProgressionModel& model, std::size_t from, std::size_t to)
{
    const std::size_t learned = model.tokens.size();
    if (from == to)
    {
        return 0;
    }
    if (from < learned && to < learned)
    {
        return (model.transitions.at(from).at(to) * kCountWeight) + kLearnedSmoothing;
    }
    return kNewSmoothing;
}

// True when the progression fits one of the entry's modes and passes validate in every fitting
// mode, key and flavour. A flavour that leaves the chords as another did is not voiced again.
bool accepted(const StyleEntry& entry, const Progression& progression)
{
    const bool fits = std::ranges::any_of(
        entry.modes, [&](Mode mode) { return checkProgression(mode, progression.chords).ok; });
    if (!fits)
    {
        return false;
    }
    for (const Mode mode : fittingModes(entry, progression))
    {
        for (int root = 0; root < kPitchClasses; ++root)
        {
            std::vector<std::vector<ChordSymbol>> voiced;
            for (const Flavour flavour : kFlavours)
            {
                Idea idea{};
                idea.settings.bars = kLongestIdea;
                idea.settings.flavour = flavour;
                idea.key = {.root = root, .mode = mode};
                idea.parallel = entry.parallel;
                placeChords(idea, progression);
                std::vector<ChordSymbol> symbols;
                std::ranges::transform(idea.progression, std::back_inserter(symbols),
                                       &PlacedChord::symbol);
                if (std::ranges::find(voiced, symbols) != voiced.end())
                {
                    continue;
                }
                voiced.push_back(std::move(symbols));
                try
                {
                    voiceChords(entry, idea.key, idea.progression);
                }
                catch (const std::logic_error&)
                {
                    return false; // no voicing fits a chord
                }
                if (!validate(idea).empty())
                {
                    return false;
                }
            }
        }
    }
    return true;
}
} // namespace

ModeGroup modeGroup(Mode mode)
{
    return mode == Mode::ionian || mode == Mode::mixolydian ? ModeGroup::major : ModeGroup::minor;
}

ProgressionModel buildProgressionModel(const Bank& bank, Family family, ModeGroup group)
{
    ProgressionModel model;
    std::vector<const Progression*> learned;
    for (const StyleEntry& entry : bank.entries)
    {
        if (entry.family != family || !inGroup(entry, group))
        {
            continue;
        }
        for (const Progression& progression : entry.progressions)
        {
            learned.push_back(&progression);
            for (const ChordSymbol& chord : progression.chords)
            {
                if (indexOf(model.tokens, chord) == model.tokens.size())
                {
                    model.tokens.push_back(chord);
                }
            }
        }
    }
    const std::size_t count = model.tokens.size();
    model.transitions.assign(count, std::vector<int>(count, 0));
    model.starts.assign(count, 0);
    int chords = 0;
    int sevenths = 0;
    for (const Progression* progression : learned)
    {
        const auto& symbols = progression->chords;
        ++model.starts.at(indexOf(model.tokens, symbols.front()));
        for (std::size_t i = 0; i < symbols.size() && symbols.size() > 1; ++i)
        {
            const ChordSymbol& next = symbols.at((i + 1) % symbols.size());
            ++model.transitions.at(indexOf(model.tokens, symbols.at(i)))
                  .at(indexOf(model.tokens, next));
        }
        chords += static_cast<int>(symbols.size());
        sevenths += static_cast<int>(std::ranges::count_if(
            symbols, [](const ChordSymbol& chord) { return carriesSeventh(chord.quality); }));
        model.templates.push_back(progression->barsPerChord);
    }
    model.sevenths = 2 * sevenths > chords;
    return model;
}

std::vector<ChordSymbol> smoothingTokens(const ProgressionModel& model, const StyleEntry& entry)
{
    const auto qualities = model.sevenths
                               ? std::vector<Quality>{Quality::min7, Quality::maj7, Quality::dom7}
                               : std::vector<Quality>{Quality::min, Quality::maj};
    std::vector<ChordSymbol> tokens;
    for (int degree = 1; degree <= 7; ++degree)
    {
        for (const Quality quality : qualities)
        {
            const std::array chord = {ChordSymbol{.degree = degree, .quality = quality}};
            const ProgressionFit fit = checkProgression(entry.modes.front(), chord);
            if (fit.ok && !fit.borrowedFrom &&
                indexOf(model.tokens, chord.front()) == model.tokens.size())
            {
                tokens.push_back(chord.front());
            }
        }
    }
    return tokens;
}

std::optional<Progression> inventProgression(const ProgressionModel& model, const StyleEntry& entry,
                                             Pcg32& rng)
{
    if (model.templates.empty())
    {
        return std::nullopt;
    }
    std::vector<ChordSymbol> states = model.tokens;
    const auto smoothing = smoothingTokens(model, entry);
    states.insert(states.end(), smoothing.begin(), smoothing.end());
    std::vector<int> startWeights(states.size(), kNewSmoothing);
    for (std::size_t i = 0; i < model.tokens.size(); ++i)
    {
        startWeights.at(i) = (model.starts.at(i) * kCountWeight) + kLearnedSmoothing;
    }
    for (int attempt = 0; attempt < kTries; ++attempt)
    {
        const auto& bars =
            model.templates.at(rng.below(static_cast<std::uint32_t>(model.templates.size())));
        std::vector<std::size_t> chain{rng.pickWeighted(startWeights)};
        std::vector<int> row(states.size());
        while (chain.size() < bars.size())
        {
            for (std::size_t to = 0; to < states.size(); ++to)
            {
                row.at(to) = moveWeight(model, chain.back(), to);
            }
            chain.push_back(rng.pickWeighted(row));
        }
        const bool repeats = std::ranges::adjacent_find(chain) != chain.end();
        if (repeats || (chain.size() > 1 && moveWeight(model, chain.back(), chain.front()) == 0))
        {
            continue;
        }
        Progression progression{.chords = {}, .barsPerChord = bars, .weightMilli = 0};
        for (const std::size_t state : chain)
        {
            progression.chords.push_back(states.at(state));
        }
        if (accepted(entry, progression))
        {
            return progression;
        }
    }
    return std::nullopt;
}
} // namespace md::detail

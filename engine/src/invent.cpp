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

ModeGroup modeGroupOf(Mode mode)
{
    return mode == Mode::ionian || mode == Mode::mixolydian ? ModeGroup::major : ModeGroup::minor;
}

bool inGroup(const StyleEntry& entry, ModeGroup group)
{
    return group == ModeGroup::both || std::ranges::any_of(entry.modes, [group](Mode mode)
                                                           { return modeGroupOf(mode) == group; });
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

// True when the progression fits one of the entry's modes and passes validate at the key root in
// every fitting mode and flavour. A flavour that leaves the chords as another did is not voiced
// again.
bool accepted(const StyleEntry& entry, const Progression& progression, int keyRoot)
{
    const bool fits = std::ranges::any_of(
        entry.modes, [&](Mode mode) { return checkProgression(mode, progression.chords).ok; });
    if (!fits)
    {
        return false;
    }
    for (const Mode mode : fittingModes(entry, progression))
    {
        std::vector<std::vector<ChordSymbol>> voiced;
        for (const Flavour flavour : kFlavours)
        {
            Idea idea{};
            idea.settings.bars = kLongestIdea;
            idea.settings.flavour = flavour;
            idea.key = {.root = keyRoot, .mode = mode};
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
    return true;
}
} // namespace

int moveWeight(const ProgressionModel& model, const std::vector<ChordSymbol>& states,
               std::size_t from, std::size_t to)
{
    const std::size_t learned = model.tokens.size();
    if (states.at(from).degree == states.at(to).degree)
    {
        return 0;
    }
    if (from < learned && to < learned)
    {
        return (model.transitions.at(from).at(to) * kCountWeight) + kLearnedSmoothing;
    }
    return kNewSmoothing;
}

ModeGroup modeGroup(const StyleEntry& entry)
{
    const ModeGroup first = modeGroupOf(entry.modes.front());
    const bool spans =
        std::ranges::any_of(entry.modes, [first](Mode mode) { return modeGroupOf(mode) != first; });
    return spans ? ModeGroup::both : first;
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
                                             int keyRoot, Pcg32& rng)
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
    // A 1-chord template holds a learned tonic.
    std::vector<int> tonicWeights(states.size(), 0);
    for (std::size_t i = 0; i < model.tokens.size(); ++i)
    {
        tonicWeights.at(i) = model.tokens.at(i).degree == 1 ? startWeights.at(i) : 0;
    }
    for (int attempt = 0; attempt < kTries; ++attempt)
    {
        const auto& bars =
            model.templates.at(rng.below(static_cast<std::uint32_t>(model.templates.size())));
        const bool held = bars.size() == 1;
        std::vector<std::size_t> chain{rng.pickWeighted(held ? tonicWeights : startWeights)};
        if (held && tonicWeights.at(chain.front()) == 0)
        {
            continue; // the model has no learned tonic
        }
        std::vector<int> row(states.size());
        while (chain.size() < bars.size())
        {
            for (std::size_t to = 0; to < states.size(); ++to)
            {
                row.at(to) = moveWeight(model, states, chain.back(), to);
            }
            chain.push_back(rng.pickWeighted(row));
        }
        const auto sameDegree = [&states](std::size_t a, std::size_t b)
        { return states.at(a).degree == states.at(b).degree; };
        const bool repeats = std::ranges::adjacent_find(chain, sameDegree) != chain.end() ||
                             (chain.size() > 1 && sameDegree(chain.back(), chain.front()));
        if (repeats)
        {
            continue;
        }
        Progression progression{.chords = {}, .barsPerChord = bars, .weightMilli = 0};
        for (const std::size_t state : chain)
        {
            progression.chords.push_back(states.at(state));
        }
        if (accepted(entry, progression, keyRoot))
        {
            return progression;
        }
    }
    return std::nullopt;
}

RhythmModel buildRhythmModel(const Bank& bank, Family family, RhythmKind kind)
{
    RhythmModel model;
    model.name = kind == RhythmKind::chords ? "fresh-stabs" : "fresh-hook";
    model.kind = kind;
    model.minOnsets = kStepsPerBar;
    std::vector<std::array<Chunk, kBeats>> slices;
    int onsets = 0;
    for (const StyleEntry& entry : bank.entries)
    {
        if (entry.family != family)
        {
            continue;
        }
        for (const RhythmCell& cell :
             kind == RhythmKind::chords ? entry.chordRhythms : entry.melodyRhythms)
        {
            const std::size_t first = slices.size();
            slices.resize(first + static_cast<std::size_t>(cell.bars));
            for (std::size_t i = 0; i < cell.hits.size(); ++i)
            {
                const Hit& hit = cell.hits.at(i);
                auto& slice = slices.at(first + static_cast<std::size_t>(hit.step / kStepsPerBar));
                const int inBar = hit.step % kStepsPerBar;
                slice.at(static_cast<std::size_t>(inBar / kBeatSteps))
                    .emplace_back(inBar % kBeatSteps, hit.length);
                ++onsets;
                if (i > 0)
                {
                    model.minGap = std::min(model.minGap, hit.step - cell.hits.at(i - 1).step);
                }
            }
        }
    }
    for (const auto& slice : slices)
    {
        int count = 0;
        for (std::size_t beat = 0; beat < kBeats; ++beat)
        {
            count += static_cast<int>(slice.at(beat).size());
            if (std::ranges::find(model.chunks.at(beat), slice.at(beat)) ==
                model.chunks.at(beat).end())
            {
                model.chunks.at(beat).push_back(slice.at(beat));
            }
        }
        model.minOnsets = std::min(model.minOnsets, count);
        model.maxOnsets = std::max(model.maxOnsets, count);
    }
    model.slices = static_cast<int>(slices.size());
    model.meanChunkOnsets = static_cast<double>(onsets) / std::max(1, model.slices * kBeats);
    model.starts.assign(model.chunks.front().size(), 0);
    for (std::size_t beat = 1; beat < kBeats; ++beat)
    {
        model.next.at(beat).assign(model.chunks.at(beat - 1).size(),
                                   std::vector<int>(model.chunks.at(beat).size(), 0));
    }
    const auto chunkIndex = [&model](std::size_t beat, const Chunk& chunk)
    {
        return static_cast<std::size_t>(std::ranges::find(model.chunks.at(beat), chunk) -
                                        model.chunks.at(beat).begin());
    };
    for (const auto& slice : slices)
    {
        ++model.starts.at(chunkIndex(0, slice.front()));
        for (std::size_t beat = 1; beat < kBeats; ++beat)
        {
            ++model.next.at(beat)
                  .at(chunkIndex(beat - 1, slice.at(beat - 1)))
                  .at(chunkIndex(beat, slice.at(beat)));
        }
    }
    return model;
}

namespace
{
constexpr int kChunkSmoothing = 100;
constexpr double kMinEnergyScale = 0.1;
constexpr double kEnergySwing = 0.6;
constexpr int kMinMelodyOnsets = 2;

// A learned count plus smoothing for each chunk of a beat, scaled so energy favours denser chunks
// when high and sparser ones when low.
std::vector<int> chunkWeights(const RhythmModel& model, std::size_t beat,
                              const std::vector<int>& counts, int energy)
{
    std::vector<int> weights;
    const double lean = kEnergySwing * ((energy / 100.0) - 0.5);
    for (std::size_t i = 0; i < counts.size(); ++i)
    {
        const auto onsets = static_cast<double>(model.chunks.at(beat).at(i).size());
        const double scale =
            std::max(kMinEnergyScale, 1 + (lean * (onsets - model.meanChunkOnsets)));
        weights.push_back(
            static_cast<int>(((counts.at(i) * kCountWeight) + kChunkSmoothing) * scale));
    }
    return weights;
}

// One slice drawn beat by beat, as hits at steps within the bar.
std::vector<Hit> drawSlice(const RhythmModel& model, int energy, Pcg32& rng)
{
    std::vector<Hit> hits;
    std::size_t chunk = 0;
    for (std::size_t beat = 0; beat < kBeats; ++beat)
    {
        const auto& counts = beat == 0 ? model.starts : model.next.at(beat).at(chunk);
        chunk = rng.pickWeighted(chunkWeights(model, beat, counts, energy));
        for (const auto& [step, length] : model.chunks.at(beat).at(chunk))
        {
            hits.push_back(
                {.step = (static_cast<int>(beat) * kBeatSteps) + step, .length = length});
        }
    }
    return hits;
}

// Whether a drawn slice keeps the learned onset range (the maximum widened by 1) and least gap,
// has an onset, and for chords one in the first half bar. lastStep is the last onset before the
// slice, as a step relative to its start.
bool sliceOk(const RhythmModel& model, const std::vector<Hit>& slice, int lastStep)
{
    const int count = static_cast<int>(slice.size());
    if (count < std::max(1, model.minOnsets) || count > model.maxOnsets + 1)
    {
        return false;
    }
    if (model.kind == RhythmKind::chords && slice.front().step >= kStepsPerBar / 2)
    {
        return false;
    }
    int previous = lastStep;
    for (const Hit& hit : slice)
    {
        if (hit.step - previous < model.minGap)
        {
            return false;
        }
        previous = hit.step;
    }
    return true;
}

bool sameSteps(const std::vector<Hit>& a, const std::vector<Hit>& b)
{
    return std::ranges::equal(a, b, {}, &Hit::step, &Hit::step);
}
} // namespace

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): (bars, energy) is the planned signature.
std::optional<RhythmCell> inventRhythm(const RhythmModel& model, int bars, int energy, Pcg32& rng)
{
    if (model.slices == 0)
    {
        return std::nullopt;
    }
    for (int attempt = 0; attempt < kTries; ++attempt)
    {
        RhythmCell cell{.id = model.name, .bars = bars, .hits = {}};
        std::vector<Hit> before;
        bool ok = true;
        for (int bar = 0; bar < bars && ok; ++bar)
        {
            const int base = bar * kStepsPerBar;
            std::vector<Hit> slice = drawSlice(model, energy, rng);
            const int lastStep = cell.hits.empty() ? -kStepsPerBar : cell.hits.back().step - base;
            ok = sliceOk(model, slice, lastStep) && (bar == 0 || !sameSteps(slice, before));
            before = slice;
            for (Hit& hit : slice)
            {
                hit.step += base;
            }
            cell.hits.insert(cell.hits.end(), slice.begin(), slice.end());
        }
        const bool enough = model.kind == RhythmKind::chords ||
                            static_cast<int>(cell.hits.size()) >= kMinMelodyOnsets;
        if (!ok || !enough)
        {
            continue;
        }
        for (std::size_t i = 0; i < cell.hits.size(); ++i)
        {
            const int end =
                i + 1 < cell.hits.size() ? cell.hits.at(i + 1).step : bars * kStepsPerBar;
            Hit& hit = cell.hits.at(i);
            hit.length = std::min(hit.length, end - hit.step);
        }
        return cell;
    }
    return std::nullopt;
}
} // namespace md::detail

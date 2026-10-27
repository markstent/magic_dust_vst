#include "md/theory.hpp"

#include "md/names.hpp"

#include <algorithm>

namespace md
{
namespace
{
struct Numeral
{
    std::string_view text;
    int degree;
};

// Longest first so that "vii" is not read as "vi" followed by "i".
constexpr auto kNumerals = std::to_array<Numeral>({
    {.text = "VII", .degree = 7},
    {.text = "III", .degree = 3},
    {.text = "VI", .degree = 6},
    {.text = "IV", .degree = 4},
    {.text = "II", .degree = 2},
    {.text = "V", .degree = 5},
    {.text = "I", .degree = 1},
    {.text = "vii", .degree = 7},
    {.text = "iii", .degree = 3},
    {.text = "vi", .degree = 6},
    {.text = "iv", .degree = 4},
    {.text = "ii", .degree = 2},
    {.text = "v", .degree = 5},
    {.text = "i", .degree = 1},
});

std::optional<Quality> parseQuality(std::string_view suffix, bool minor)
{
    if (minor)
    {
        if (suffix.empty())
        {
            return Quality::min;
        }
        if (suffix == "7")
        {
            return Quality::min7;
        }
        if (suffix == "9")
        {
            return Quality::min9;
        }
        if (suffix == "11")
        {
            return Quality::min11;
        }
        return std::nullopt;
    }
    if (suffix.empty())
    {
        return Quality::maj;
    }
    if (suffix == "7")
    {
        return Quality::dom7;
    }
    if (suffix == "9")
    {
        return Quality::dom9;
    }
    if (suffix == "maj7")
    {
        return Quality::maj7;
    }
    if (suffix == "maj9")
    {
        return Quality::maj9;
    }
    if (suffix == "6")
    {
        return Quality::six;
    }
    if (suffix == "add9")
    {
        return Quality::add9;
    }
    if (suffix == "sus4")
    {
        return Quality::sus4;
    }
    return std::nullopt;
}

std::string_view qualitySuffix(Quality quality)
{
    switch (quality)
    {
    case Quality::maj:
        return "";
    case Quality::min:
        return "m";
    case Quality::min7:
        return "m7";
    case Quality::min9:
        return "m9";
    case Quality::min11:
        return "m11";
    case Quality::maj7:
        return "maj7";
    case Quality::maj9:
        return "maj9";
    case Quality::dom7:
        return "7";
    case Quality::dom9:
        return "9";
    case Quality::dom13:
        return "13";
    case Quality::sus4:
        return "sus4";
    case Quality::six:
        return "6";
    case Quality::add9:
        return "add9";
    }
    return "";
}

bool fitsMode(Mode mode, const ChordSymbol& chord, const Key& spelling)
{
    const auto scale = scalePitchClasses(Key{.root = 0, .mode = mode});
    const auto pcs = chordPitchClasses(spelling, chord);
    return std::ranges::all_of(pcs,
                               [&](int pc) { return std::ranges::find(scale, pc) != scale.end(); });
}
} // namespace

std::array<int, 7> scaleIntervals(Mode mode)
{
    switch (mode)
    {
    case Mode::phrygian:
        return {0, 1, 3, 5, 7, 8, 10};
    case Mode::aeolian:
        return {0, 2, 3, 5, 7, 8, 10};
    case Mode::dorian:
        return {0, 2, 3, 5, 7, 9, 10};
    case Mode::mixolydian:
        return {0, 2, 4, 5, 7, 9, 10};
    case Mode::ionian:
        return {0, 2, 4, 5, 7, 9, 11};
    }
    return {};
}

std::optional<ChordSymbol> parseDegree(std::string_view text)
{
    for (const auto& numeral : kNumerals)
    {
        if (!text.starts_with(numeral.text))
        {
            continue;
        }
        const bool minor = numeral.text.front() >= 'a';
        const auto quality = parseQuality(text.substr(numeral.text.size()), minor);
        if (!quality)
        {
            return std::nullopt;
        }
        return ChordSymbol{.degree = numeral.degree, .quality = *quality};
    }
    return std::nullopt;
}

int chordRootPc(const Key& key, const ChordSymbol& chord)
{
    return (key.root + scaleIntervals(key.mode).at(static_cast<std::size_t>(chord.degree - 1))) %
           12;
}

std::vector<int> chordIntervals(Quality quality)
{
    switch (quality)
    {
    case Quality::maj:
        return {0, 4, 7};
    case Quality::min:
        return {0, 3, 7};
    case Quality::min7:
        return {0, 3, 7, 10};
    case Quality::min9:
        return {0, 3, 7, 10, 14};
    case Quality::min11:
        return {0, 3, 7, 10, 14, 17};
    case Quality::maj7:
        return {0, 4, 7, 11};
    case Quality::maj9:
        return {0, 4, 7, 11, 14};
    case Quality::dom7:
        return {0, 4, 7, 10};
    case Quality::dom9:
        return {0, 4, 7, 10, 14};
    case Quality::dom13:
        return {0, 4, 7, 10, 14, 21};
    case Quality::sus4:
        return {0, 5, 7};
    case Quality::six:
        return {0, 4, 7, 9};
    case Quality::add9:
        return {0, 4, 7, 14};
    }
    return {};
}

std::vector<int> chordPitchClasses(const Key& key, const ChordSymbol& chord)
{
    const int root = chordRootPc(key, chord);
    std::vector<int> pcs;
    for (const int interval : chordIntervals(chord.quality))
    {
        pcs.push_back((root + interval) % 12);
    }
    std::ranges::sort(pcs);
    return pcs;
}

std::vector<int> scalePitchClasses(const Key& key)
{
    std::vector<int> pcs;
    for (const int interval : scaleIntervals(key.mode))
    {
        pcs.push_back((key.root + interval) % 12);
    }
    std::ranges::sort(pcs);
    return pcs;
}

std::string chordName(const Key& key, const ChordSymbol& chord)
{
    std::string name{rootName(chordRootPc(key, chord))};
    name += qualitySuffix(chord.quality);
    return name;
}

// NOLINTNEXTLINE(modernize-use-string-view): the spec returns std::string for PlacedChord::feel.
std::string feelWord(const ChordSymbol& chord, std::size_t slot, std::size_t count)
{
    switch (chord.degree)
    {
    case 1:
        return "home";
    case 2:
    case 4:
        return "lift";
    case 3:
        return "bright";
    case 5:
        return (count >= 4 && slot == count - 1) ? "pull back" : "tension";
    case 6:
        return "warm";
    default:
        return "push";
    }
}

int swingPermille(Groove groove)
{
    switch (groove)
    {
    case Groove::straight:
        return 500;
    case Groove::swing54:
        return 540;
    case Groove::ukShuffle58:
        return 580;
    case Groove::heavy62:
        return 620;
    }
    return 500;
}

std::vector<Mode> relatedModes(Mode mode)
{
    switch (mode)
    {
    case Mode::aeolian:
        return {Mode::phrygian, Mode::dorian};
    case Mode::phrygian:
    case Mode::dorian:
        return {Mode::aeolian};
    case Mode::ionian:
        return {Mode::mixolydian};
    case Mode::mixolydian:
        return {Mode::ionian};
    }
    return {};
}

ProgressionFit checkProgression(Mode mode, std::span<const ChordSymbol> chords)
{
    const Key spelling{.root = 0, .mode = mode};
    auto candidates = relatedModes(mode);
    bool borrowed = false;
    for (const auto& chord : chords)
    {
        if (fitsMode(mode, chord, spelling))
        {
            continue;
        }
        borrowed = true;
        std::erase_if(candidates, [&](Mode m) { return !fitsMode(m, chord, spelling); });
        if (candidates.empty())
        {
            return {.ok = false, .borrowedFrom = std::nullopt};
        }
    }
    if (!borrowed)
    {
        return {.ok = true, .borrowedFrom = std::nullopt};
    }
    return {.ok = true, .borrowedFrom = candidates.front()};
}
} // namespace md

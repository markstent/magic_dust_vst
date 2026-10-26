#include "md/names.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <utility>

namespace md
{
namespace
{
using std::string_view_literals::operator""sv;

template <typename E, std::size_t N> using Table = std::array<std::pair<std::string_view, E>, N>;

template <typename E, std::size_t N> std::string_view nameIn(const Table<E, N>& table, E value)
{
    const auto it = std::ranges::find(table, value, &std::pair<std::string_view, E>::second);
    return it == table.end() ? ""sv : it->first;
}

template <typename E, std::size_t N>
std::optional<E> parseIn(const Table<E, N>& table, std::string_view text)
{
    const auto it = std::ranges::find(table, text, &std::pair<std::string_view, E>::first);
    return it == table.end() ? std::nullopt : std::optional<E>(it->second);
}

constexpr std::array<std::string_view, 12> kRoots{
    "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B",
};
constexpr Table<Mode, 5> kModes{{
    {"phrygian", Mode::phrygian},
    {"aeolian", Mode::aeolian},
    {"dorian", Mode::dorian},
    {"mixolydian", Mode::mixolydian},
    {"ionian", Mode::ionian},
}};
constexpr Table<BassPattern, 8> kBassPatterns{{
    {"rolling-offbeat", BassPattern::rollingOffbeat},
    {"offbeat", BassPattern::offbeat},
    {"octave-bounce", BassPattern::octaveBounce},
    {"garage-skip", BassPattern::garageSkip},
    {"sustained", BassPattern::sustained},
    {"riff", BassPattern::riff},
    {"pedal", BassPattern::pedal},
    {"call-response", BassPattern::callResponse},
}};
constexpr Table<MelodyMode, 4> kMelodyModes{{
    {"vocal-chop", MelodyMode::vocalChop},
    {"hook", MelodyMode::hook},
    {"riff", MelodyMode::riff},
    {"sparse", MelodyMode::sparse},
}};
constexpr Table<Family, 5> kFamilies{{
    {"mk-dub", Family::mkDub},
    {"soulful", Family::soulful},
    {"bassline", Family::bassline},
    {"festival", Family::festival},
    {"ny-groove", Family::nyGroove},
}};
constexpr Table<Style, 6> kStyles{{
    {"uk-funky", Style::ukFunky},
    {"mk-dub", Style::mkDub},
    {"soulful", Style::soulful},
    {"bassline", Style::bassline},
    {"festival", Style::festival},
    {"ny-groove", Style::nyGroove},
}};

char lower(char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); }
} // namespace

std::string_view rootName(int root) { return kRoots.at(static_cast<std::size_t>(root)); }

std::optional<int> parseRoot(std::string_view text)
{
    constexpr std::array<std::pair<char, int>, 7> letters{
        std::pair{'a', 9}, std::pair{'b', 11}, std::pair{'c', 0}, std::pair{'d', 2},
        std::pair{'e', 4}, std::pair{'f', 5},  std::pair{'g', 7},
    };
    if (text.empty() || text.size() > 2)
    {
        return std::nullopt;
    }
    const auto* const letter =
        std::ranges::find(letters, lower(text.front()), &std::pair<char, int>::first);
    if (letter == letters.end())
    {
        return std::nullopt;
    }
    int root = letter->second;
    if (text.size() == 2)
    {
        if (text.back() != '#' && lower(text.back()) != 'b')
        {
            return std::nullopt;
        }
        root += text.back() == '#' ? 1 : -1;
    }
    return (root + 12) % 12;
}

std::string keyName(const Key& key)
{
    std::string name(rootName(key.root));
    std::ranges::transform(name, name.begin(), lower);
    return name + " " + std::string(modeName(key.mode));
}

std::string_view modeName(Mode mode) { return nameIn(kModes, mode); }
std::optional<Mode> parseMode(std::string_view text) { return parseIn(kModes, text); }

std::string_view bassPatternName(BassPattern pattern) { return nameIn(kBassPatterns, pattern); }
std::optional<BassPattern> parseBassPattern(std::string_view text)
{
    return parseIn(kBassPatterns, text);
}

std::string_view melodyModeName(MelodyMode mode) { return nameIn(kMelodyModes, mode); }
std::optional<MelodyMode> parseMelodyMode(std::string_view text)
{
    return parseIn(kMelodyModes, text);
}

std::string_view familyName(Family family) { return nameIn(kFamilies, family); }
std::optional<Family> parseFamily(std::string_view text) { return parseIn(kFamilies, text); }

std::string_view styleName(Style style) { return nameIn(kStyles, style); }
std::optional<Style> parseStyle(std::string_view text) { return parseIn(kStyles, text); }
} // namespace md

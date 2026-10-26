#include "args.hpp"

#include "md/ranges.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <optional>
#include <utility>

namespace md
{
namespace
{
using std::string_view_literals::operator""sv;

template <typename K, typename T, std::size_t N>
std::optional<T> lookup(const std::array<std::pair<K, T>, N>& names, K text)
{
    for (const auto& [name, value] : names)
    {
        if (name == text)
        {
            return value;
        }
    }
    return std::nullopt;
}

std::optional<int> parseInt(std::string_view text, int low, int high)
{
    // Digits only, so stoi cannot see a sign, a space or trailing junk.
    const bool digits = !text.empty() && text.size() <= 6 &&
                        std::ranges::all_of(text, [](char c) { return std::isdigit(c) != 0; });
    if (!digits)
    {
        return std::nullopt;
    }
    const int value = std::stoi(std::string(text));
    if (value < low || value > high)
    {
        return std::nullopt;
    }
    return value;
}

// A note letter then an optional # or b, any case: a, c#, Bb.
std::optional<int> parseKey(std::string_view text)
{
    constexpr std::array<std::pair<char, int>, 7> letters{
        std::pair{'a', 9}, std::pair{'b', 11}, std::pair{'c', 0}, std::pair{'d', 2},
        std::pair{'e', 4}, std::pair{'f', 5},  std::pair{'g', 7},
    };
    if (text.empty() || text.size() > 2)
    {
        return std::nullopt;
    }
    const auto letter = static_cast<char>(std::tolower(static_cast<unsigned char>(text.front())));
    auto root = lookup(letters, letter);
    if (!root)
    {
        return std::nullopt;
    }
    if (text.size() == 2)
    {
        const auto accidental =
            static_cast<char>(std::tolower(static_cast<unsigned char>(text.back())));
        if (text.back() != '#' && accidental != 'b')
        {
            return std::nullopt;
        }
        *root += text.back() == '#' ? 1 : -1;
    }
    return (*root + 12) % 12;
}

constexpr std::array<std::pair<std::string_view, Style>, 6> kStyles{
    std::pair{"uk-funky"sv, Style::ukFunky},  std::pair{"mk-dub"sv, Style::mkDub},
    std::pair{"soulful"sv, Style::soulful},   std::pair{"bassline"sv, Style::bassline},
    std::pair{"festival"sv, Style::festival}, std::pair{"ny-groove"sv, Style::nyGroove},
};
constexpr std::array<std::pair<std::string_view, Groove>, 4> kGrooves{
    std::pair{"straight"sv, Groove::straight},
    std::pair{"swing"sv, Groove::swing54},
    std::pair{"uk-shuffle"sv, Groove::ukShuffle58},
    std::pair{"heavy"sv, Groove::heavy62},
};
constexpr std::array<std::pair<std::string_view, Flavour>, 3> kFlavours{
    std::pair{"simple"sv, Flavour::simple},
    std::pair{"soulful"sv, Flavour::soulful},
    std::pair{"jazzy"sv, Flavour::jazzy},
};

std::string bad(std::string_view flag, std::string_view value)
{
    return "bad value '" + std::string(value) + "' for " + std::string(flag);
}

// Returns an error message, or an empty string when the flag was applied.
std::string apply(CliOptions& o, std::string_view flag, std::string_view v)
{
    auto number = [&](int low, int high, int& target)
    {
        const auto n = parseInt(v, low, high);
        if (!n)
        {
            return bad(flag, v);
        }
        target = *n;
        return std::string{};
    };
    auto named = [&](const auto& names, auto& target)
    {
        const auto found = lookup(names, v);
        if (!found)
        {
            return bad(flag, v);
        }
        target = *found;
        return std::string{};
    };
    if (flag == "--style")
    {
        return named(kStyles, o.settings.style);
    }
    if (flag == "--groove")
    {
        return named(kGrooves, o.settings.groove);
    }
    if (flag == "--flavour")
    {
        return named(kFlavours, o.settings.flavour);
    }
    if (flag == "--key")
    {
        const auto root = parseKey(v);
        if (!root)
        {
            return bad(flag, v);
        }
        o.settings.keyRoot = *root;
        return {};
    }
    if (flag == "--seed")
    {
        int seed = 0;
        auto error = number(0, kSeedMax, seed);
        o.seed = static_cast<std::uint32_t>(seed);
        return error;
    }
    if (flag == "--mood")
    {
        return number(0, 100, o.settings.mood);
    }
    if (flag == "--energy")
    {
        return number(0, 100, o.settings.energy);
    }
    if (flag == "--tempo")
    {
        return number(kTempoMin, kTempoMax, o.tempo);
    }
    if (flag == "--bars")
    {
        const auto n = parseInt(v, 4, 16);
        if (!n || (*n != 4 && *n != 8 && *n != 16))
        {
            return bad(flag, v) + " (use 4, 8 or 16)";
        }
        o.settings.bars = *n;
        return {};
    }
    if (flag == "--out")
    {
        o.out = v;
        return {};
    }
    return "unknown option '" + std::string(flag) + "'";
}
} // namespace

std::string keyName(const Key& key)
{
    constexpr std::array<std::string_view, 12> notes{
        "c", "c#", "d", "eb", "e", "f", "f#", "g", "ab", "a", "bb", "b",
    };
    constexpr std::array<std::string_view, 5> modes{
        "phrygian", "aeolian", "dorian", "mixolydian", "ionian",
    };
    return std::string(notes.at(static_cast<std::size_t>(key.root))) + " " +
           std::string(modes.at(static_cast<std::size_t>(key.mode)));
}

std::variant<CliOptions, std::string> parseArgs(std::span<const std::string_view> args)
{
    CliOptions options;
    for (std::size_t i = 0; i < args.size(); i += 2)
    {
        if (i + 1 == args.size())
        {
            return "missing value for " + std::string(args.subspan(i).front());
        }
        const auto pair = args.subspan(i, 2);
        if (auto error = apply(options, pair.front(), pair.back()); !error.empty())
        {
            return error;
        }
    }
    if (options.out.empty())
    {
        return "missing --out";
    }
    return options;
}
} // namespace md

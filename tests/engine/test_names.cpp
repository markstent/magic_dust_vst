#include "md/names.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <stdexcept>
#include <string>

using namespace md;

TEST_CASE("root names use the fixed spelling and parse back", "[names]")
{
    constexpr std::array<std::string_view, 12> expected = {
        "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B",
    };
    for (int root = 0; root < 12; ++root)
    {
        CHECK(rootName(root) == expected.at(static_cast<std::size_t>(root)));
        CHECK(parseRoot(rootName(root)) == root);
    }
    REQUIRE_THROWS_AS(rootName(12), std::out_of_range);
}

TEST_CASE("roots parse in any case and any enharmonic spelling", "[names]")
{
    CHECK(parseRoot("a") == 9);
    CHECK(parseRoot("BB") == 10);
    CHECK(parseRoot("db") == 1);
    CHECK(parseRoot("a#") == 10);
    CHECK(parseRoot("cb") == 11);
    CHECK_FALSE(parseRoot("").has_value());
    CHECK_FALSE(parseRoot("h").has_value());
    CHECK_FALSE(parseRoot("c##").has_value());
    CHECK_FALSE(parseRoot("cx").has_value());
}

TEST_CASE("key names are lowercase and spelled like the chord names", "[names]")
{
    CHECK(keyName({9, Mode::aeolian}) == "a aeolian");
    CHECK(keyName({10, Mode::dorian}) == "bb dorian");
    CHECK(keyName({3, Mode::phrygian}) == "eb phrygian");
    CHECK(keyName({8, Mode::mixolydian}) == "ab mixolydian");
    CHECK(keyName({1, Mode::ionian}) == "c# ionian");
}

TEST_CASE("every name round trips and unknown text parses to nothing", "[names]")
{
    for (const auto mode :
         {Mode::phrygian, Mode::aeolian, Mode::dorian, Mode::mixolydian, Mode::ionian})
    {
        CHECK(parseMode(modeName(mode)) == mode);
    }
    for (int i = 0; i <= static_cast<int>(BassPattern::callResponse); ++i)
    {
        const auto pattern = static_cast<BassPattern>(i);
        CHECK_FALSE(bassPatternName(pattern).empty());
        CHECK(parseBassPattern(bassPatternName(pattern)) == pattern);
    }
    for (int i = 0; i <= static_cast<int>(MelodyMode::sparse); ++i)
    {
        const auto mode = static_cast<MelodyMode>(i);
        CHECK_FALSE(melodyModeName(mode).empty());
        CHECK(parseMelodyMode(melodyModeName(mode)) == mode);
    }
    for (int i = 0; i <= static_cast<int>(Family::nyGroove); ++i)
    {
        const auto family = static_cast<Family>(i);
        CHECK_FALSE(familyName(family).empty());
        CHECK(parseFamily(familyName(family)) == family);
    }
    for (int i = 0; i <= static_cast<int>(Style::nyGroove); ++i)
    {
        const auto style = static_cast<Style>(i);
        CHECK_FALSE(styleName(style).empty());
        CHECK(parseStyle(styleName(style)) == style);
    }
    CHECK(bassPatternName(BassPattern::callResponse) == "call-response");
    CHECK(melodyModeName(MelodyMode::pluck) == "pluck");
    CHECK(styleName(Style::ukFunky) == "uk-funky");
    CHECK_FALSE(parseMode("Aeolian").has_value());
    CHECK_FALSE(parseBassPattern("offbeat ").has_value());
    CHECK_FALSE(parseMelodyMode("").has_value());
    CHECK_FALSE(parseFamily("uk-funky").has_value());
    CHECK_FALSE(parseStyle("trance").has_value());
}

#include "args.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

namespace
{
std::variant<md::CliOptions, std::string> parse(std::initializer_list<std::string_view> args)
{
    const std::vector<std::string_view> list(args);
    return md::parseArgs(list);
}

md::CliOptions ok(std::initializer_list<std::string_view> args)
{
    const auto result = parse(args);
    REQUIRE(std::holds_alternative<md::CliOptions>(result));
    return std::get<md::CliOptions>(result);
}

std::string error(std::initializer_list<std::string_view> args)
{
    const auto result = parse(args);
    REQUIRE(std::holds_alternative<std::string>(result));
    return std::get<std::string>(result);
}
} // namespace

TEST_CASE("defaults apply when only style, seed and out are given")
{
    const auto o = ok({"--style", "mk-dub", "--seed", "48213", "--out", "idea.mid"});
    CHECK(o.settings.style == md::Style::mkDub);
    CHECK(o.seed == 48213);
    CHECK(o.tempo == 124);
    CHECK(o.out == "idea.mid");
    const md::Settings defaults;
    CHECK(o.settings.keyRoot == defaults.keyRoot);
    CHECK(o.settings.mood == defaults.mood);
    CHECK(o.settings.energy == defaults.energy);
    CHECK(o.settings.groove == defaults.groove);
    CHECK(o.settings.flavour == defaults.flavour);
    CHECK(o.settings.bars == defaults.bars);
}

TEST_CASE("keys accept sharps, flats and any case")
{
    CHECK(ok({"--key", "c#", "--out", "x"}).settings.keyRoot == 1);
    CHECK(ok({"--key", "bb", "--out", "x"}).settings.keyRoot == 10);
    CHECK(ok({"--key", "A", "--out", "x"}).settings.keyRoot == 9);
    CHECK(ok({"--key", "Cb", "--out", "x"}).settings.keyRoot == 11);
}

TEST_CASE("every other option parses")
{
    const auto o = ok({
        "--groove",
        "uk-shuffle",
        "--flavour",
        "jazzy",
        "--mood",
        "0",
        "--energy",
        "100",
        "--bars",
        "16",
        "--tempo",
        "128",
        "--out",
        "x",
    });
    CHECK(o.settings.groove == md::Groove::ukShuffle58);
    CHECK(o.settings.flavour == md::Flavour::jazzy);
    CHECK(o.settings.mood == 0);
    CHECK(o.settings.energy == 100);
    CHECK(o.settings.bars == 16);
    CHECK(o.tempo == 128);
}

TEST_CASE("every style and groove name is accepted")
{
    CHECK(ok({"--style", "uk-funky", "--out", "x"}).settings.style == md::Style::ukFunky);
    CHECK(ok({"--style", "soulful", "--out", "x"}).settings.style == md::Style::soulful);
    CHECK(ok({"--style", "bassline", "--out", "x"}).settings.style == md::Style::bassline);
    CHECK(ok({"--style", "festival", "--out", "x"}).settings.style == md::Style::festival);
    CHECK(ok({"--style", "ny-groove", "--out", "x"}).settings.style == md::Style::nyGroove);
    CHECK(ok({"--groove", "straight", "--out", "x"}).settings.groove == md::Groove::straight);
    CHECK(ok({"--groove", "swing", "--out", "x"}).settings.groove == md::Groove::swing54);
    CHECK(ok({"--groove", "heavy", "--out", "x"}).settings.groove == md::Groove::heavy62);
}

TEST_CASE("bad input returns an error naming the bad value")
{
    using Catch::Matchers::ContainsSubstring;
    CHECK_THAT(error({"--style", "mk-dub"}), ContainsSubstring("--out"));
    CHECK_THAT(error({"--wat", "1", "--out", "x"}), ContainsSubstring("--wat"));
    CHECK_THAT(error({"--seed", "100000", "--out", "x"}), ContainsSubstring("100000"));
    CHECK_THAT(error({"--bars", "5", "--out", "x"}), ContainsSubstring("'5'"));
    CHECK_THAT(error({"--mood", "101", "--out", "x"}), ContainsSubstring("101"));
    CHECK_THAT(error({"--key", "h", "--out", "x"}), ContainsSubstring("'h'"));
    CHECK_THAT(error({"--style", "trance", "--out", "x"}), ContainsSubstring("trance"));
    CHECK_THAT(error({"--tempo", "59", "--out", "x"}), ContainsSubstring("59"));
    CHECK_THAT(error({"--tempo", "201", "--out", "x"}), ContainsSubstring("201"));
    CHECK_THAT(error({"--out"}), ContainsSubstring("--out"));
}

TEST_CASE("key names use the chord spelling in lowercase")
{
    CHECK(md::keyName({9, md::Mode::aeolian}) == "a aeolian");
    CHECK(md::keyName({10, md::Mode::dorian}) == "bb dorian");
    CHECK(md::keyName({3, md::Mode::phrygian}) == "eb phrygian");
    CHECK(md::keyName({8, md::Mode::mixolydian}) == "ab mixolydian");
    CHECK(md::keyName({1, md::Mode::ionian}) == "c# ionian");
}

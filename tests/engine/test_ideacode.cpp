#include "md/ideacode.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cctype>
#include <stdexcept>
#include <string>

using namespace md;

namespace
{
constexpr std::string_view kAlphabet = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

// Builds a code from raw fields so tests can force out-of-range values.
std::string rawCode(unsigned version, unsigned seed, unsigned keyRoot, unsigned style,
                    unsigned mood, unsigned energy, unsigned groove, unsigned flavour,
                    unsigned bars, unsigned bank)
{
    std::string bits;
    const auto put = [&](unsigned value, int width)
    {
        for (int i = width - 1; i >= 0; --i)
        {
            bits += (((value >> static_cast<unsigned>(i)) & 1U) != 0U) ? '1' : '0';
        }
    };
    put(version, 2);
    put(seed, 17);
    put(keyRoot, 4);
    put(style, 3);
    put(mood, 7);
    put(energy, 7);
    put(groove, 2);
    put(flavour, 2);
    put(bars, 2);
    put(bank, 16);
    bits.append(3, '0');
    std::string code;
    for (std::size_t i = 0; i < bits.size(); i += 5)
    {
        code += kAlphabet.at(std::stoul(bits.substr(i, 5), nullptr, 2));
    }
    return code;
}

void checkRoundTrip(std::uint32_t seed, const Settings& s)
{
    const auto code = encodeIdea(seed, s, 0x12345);
    REQUIRE(code.size() == 13);
    REQUIRE(code.find_first_not_of(kAlphabet) == std::string::npos);
    const auto decoded = decodeIdea(code);
    REQUIRE(decoded.has_value());
    const auto d = decoded.value_or(DecodedIdea{});
    CHECK(d.seed == seed);
    CHECK(d.bankVersion16 == 0x2345);
    CHECK(d.settings.style == s.style);
    CHECK(d.settings.groove == s.groove);
    CHECK(d.settings.flavour == s.flavour);
    CHECK(d.settings.bars == s.bars);
    CHECK(d.settings.mood == s.mood);
    CHECK(d.settings.energy == s.energy);
    CHECK(d.settings.keyRoot == s.keyRoot);
}
} // namespace

TEST_CASE("idea codes round trip every enum value", "[ideacode]")
{
    for (int style = 0; style <= static_cast<int>(Style::nyGroove); ++style)
    {
        for (int groove = 0; groove <= static_cast<int>(Groove::heavy62); ++groove)
        {
            for (int flavour = 0; flavour <= static_cast<int>(Flavour::jazzy); ++flavour)
            {
                Settings s;
                s.style = static_cast<Style>(style);
                s.groove = static_cast<Groove>(groove);
                s.flavour = static_cast<Flavour>(flavour);
                checkRoundTrip(48213, s);
            }
        }
    }
}

TEST_CASE("idea codes round trip numeric fields", "[ideacode]")
{
    for (const int bars : {4, 8, 16})
    {
        for (const int level : {0, 37, 100})
        {
            for (int root = 0; root < 12; ++root)
            {
                for (const std::uint32_t seed : {0U, 48213U, 99999U})
                {
                    Settings s;
                    s.bars = bars;
                    s.mood = level;
                    s.energy = 100 - level;
                    s.keyRoot = root;
                    checkRoundTrip(seed, s);
                }
            }
        }
    }
}

TEST_CASE("decodeIdea is case-insensitive", "[ideacode]")
{
    auto code = encodeIdea(48213, Settings(), 1);
    for (auto& c : code)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    CHECK(decodeIdea(code).has_value());
}

TEST_CASE("decodeIdea rejects malformed codes", "[ideacode]")
{
    const auto good = encodeIdea(48213, Settings(), 1);
    CHECK_FALSE(decodeIdea("").has_value());
    CHECK_FALSE(decodeIdea(good.substr(0, 12)).has_value());
    CHECK_FALSE(decodeIdea(good + "0").has_value());
    auto withU = good;
    withU.at(5) = 'U';
    CHECK_FALSE(decodeIdea(withU).has_value());
}

TEST_CASE("decodeIdea rejects bad fields", "[ideacode]")
{
    CHECK(decodeIdea(rawCode(0, 1, 9, 0, 30, 50, 2, 2, 1, 1)).has_value());
    CHECK_FALSE(decodeIdea(rawCode(1, 1, 9, 0, 30, 50, 2, 2, 1, 1)).has_value());
    CHECK_FALSE(decodeIdea(rawCode(0, 100000, 9, 0, 30, 50, 2, 2, 1, 1)).has_value());
    CHECK_FALSE(decodeIdea(rawCode(0, 1, 12, 0, 30, 50, 2, 2, 1, 1)).has_value());
    CHECK_FALSE(decodeIdea(rawCode(0, 1, 9, 6, 30, 50, 2, 2, 1, 1)).has_value());
    CHECK_FALSE(decodeIdea(rawCode(0, 1, 9, 0, 101, 50, 2, 2, 1, 1)).has_value());
    CHECK_FALSE(decodeIdea(rawCode(0, 1, 9, 0, 30, 101, 2, 2, 1, 1)).has_value());
    CHECK_FALSE(decodeIdea(rawCode(0, 1, 9, 0, 30, 50, 2, 3, 1, 1)).has_value());
    CHECK_FALSE(decodeIdea(rawCode(0, 1, 9, 0, 30, 50, 2, 2, 3, 1)).has_value());
}

TEST_CASE("decodeIdea rejects non-zero padding bits", "[ideacode]")
{
    auto code = encodeIdea(48213, Settings(), 1);
    const auto last = kAlphabet.find(code.back());
    for (std::size_t pad = 1; pad < 8; ++pad)
    {
        code.back() = kAlphabet.at(last | pad);
        CHECK_FALSE(decodeIdea(code).has_value());
    }
}

TEST_CASE("encodeIdea rejects out-of-range input", "[ideacode]")
{
    const auto encode = [](std::uint32_t seed, const Settings& s)
    { return encodeIdea(seed, s, 1); };
    CHECK_THROWS_AS(encode(100000, Settings()), std::invalid_argument);
    Settings s;
    s.keyRoot = 12;
    CHECK_THROWS_AS(encode(1, s), std::invalid_argument);
    s = Settings();
    s.keyRoot = -1;
    CHECK_THROWS_AS(encode(1, s), std::invalid_argument);
    s = Settings();
    s.mood = 101;
    CHECK_THROWS_AS(encode(1, s), std::invalid_argument);
    s = Settings();
    s.mood = -1;
    CHECK_THROWS_AS(encode(1, s), std::invalid_argument);
    s = Settings();
    s.energy = 101;
    CHECK_THROWS_AS(encode(1, s), std::invalid_argument);
    s = Settings();
    s.energy = -1;
    CHECK_THROWS_AS(encode(1, s), std::invalid_argument);
    s = Settings();
    s.bars = 6;
    CHECK_THROWS_AS(encode(1, s), std::invalid_argument);
}

TEST_CASE("idea code format is pinned", "[ideacode]")
{
    // Golden value: changing the code layout breaks every saved idea.
    CHECK(encodeIdea(48213, Settings(), 0x1234) == "2Y5B43SJJH4D0");
}

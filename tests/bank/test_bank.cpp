#include "md/bank.hpp"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_exception.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using Catch::Matchers::ContainsSubstring;

namespace
{
std::string readFile(const fs::path& path)
{
    std::ifstream in(path);
    std::stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

std::vector<fs::path> fixtures(const std::string& dir)
{
    std::vector<fs::path> files;
    for (const auto& item : fs::directory_iterator(fs::path(MD_BANK_FIXTURES) / dir))
    {
        files.push_back(item.path());
    }
    std::ranges::sort(files);
    return files;
}

std::string validText()
{
    return readFile(fs::path(MD_BANK_FIXTURES) / "valid" / "mk-dub-starter-01.json");
}

std::string replaced(std::string text, const std::string& from, const std::string& to)
{
    const auto at = text.find(from);
    REQUIRE(at != std::string::npos);
    return text.replace(at, from.size(), to);
}
} // namespace

TEST_CASE("every valid fixture parses")
{
    const auto files = fixtures("valid");
    REQUIRE(files.size() >= 2);
    for (const auto& file : files)
    {
        CAPTURE(file.filename().string());
        CHECK_NOTHROW(md::parseEntry(readFile(file)));
    }
}

TEST_CASE("every invalid fixture throws BankError naming the entry")
{
    const auto files = fixtures("invalid");
    REQUIRE(files.size() >= 13);
    for (const auto& file : files)
    {
        CAPTURE(file.filename().string());
        if (file.stem() == "nan-swing")
        {
            // Not parseable JSON, so no id can be named.
            CHECK_THROWS_AS(md::parseEntry(readFile(file)), md::BankError);
            continue;
        }
        CHECK_THROWS_MATCHES(
            md::parseEntry(readFile(file)), md::BankError,
            Catch::Matchers::MessageMatches(ContainsSubstring(file.stem().string())));
    }
}

TEST_CASE("theory faults name the progression")
{
    const auto files = fixtures("invalid-theory");
    REQUIRE(files.size() == 2);
    for (const auto& file : files)
    {
        CAPTURE(file.filename().string());
        CHECK_THROWS_MATCHES(md::parseEntry(readFile(file)), md::BankError,
                             Catch::Matchers::MessageMatches(ContainsSubstring("/progressions/0")));
    }
}

TEST_CASE("parallel entries skip the chord rule")
{
    const auto text = replaced(validText(), R"(["i7", "iv7"])", R"(["i7", "VImaj7", "II"])");
    CHECK_NOTHROW(md::parseEntry(text));
    CHECK_THROWS_AS(md::parseEntry(replaced(text, "\"parallel\": true", "\"parallel\": false")),
                    md::BankError);
}

TEST_CASE("invalid JSON text is a BankError")
{
    CHECK_THROWS_AS(md::parseEntry("{"), md::BankError);
    CHECK_THROWS_AS(md::parseEntry("[]"), md::BankError);
}

TEST_CASE("the spec example parses to the expected entry")
{
    const auto entry = md::parseEntry(validText());
    CHECK(entry.id == "mk-dub-starter-01");
    CHECK(entry.family == md::Family::mkDub);
    CHECK(entry.confidence == md::Confidence::placeholder);
    CHECK(entry.tempoMin == 122);
    CHECK(entry.tempoMax == 126);
    CHECK(entry.modes == std::vector<md::Mode>{md::Mode::aeolian, md::Mode::dorian});
    REQUIRE(entry.progressions.size() == 1);
    const auto& progression = entry.progressions.at(0);
    CHECK(progression.chords ==
          std::vector<md::ChordSymbol>{{.degree = 1, .quality = md::Quality::min7},
                                       {.degree = 4, .quality = md::Quality::min7}});
    CHECK(progression.barsPerChord == std::vector<int>{2, 2});
    CHECK(progression.weightMilli == 1000);
    CHECK(entry.voicingStyle == md::VoicingStyle::organStab);
    CHECK(entry.parallel);
    CHECK(entry.topNoteLow == 67);
    CHECK(entry.topNoteHigh == 76);
    REQUIRE(entry.chordRhythms.size() == 1);
    CHECK(entry.chordRhythms.at(0).hits.size() == 4);
    REQUIRE(entry.melodyRhythms.size() == 1);
    CHECK(entry.melodyRhythms.at(0).bars == 2);
    CHECK(entry.bassPatterns ==
          std::vector<md::BassPattern>{md::BassPattern::offbeat, md::BassPattern::octaveBounce});
    CHECK(entry.melodyModes == std::vector<md::MelodyMode>{md::MelodyMode::pluck});
    CHECK(entry.swingPermille == 560);
    CHECK(entry.tags == std::vector<std::string>{"mk-ish", "peak time"});
}

TEST_CASE("an array barsPerChord is kept as written")
{
    const auto entry =
        md::parseEntry(readFile(fs::path(MD_BANK_FIXTURES) / "valid" / "soulful-array-bars.json"));
    CHECK(entry.family == md::Family::soulful);
    CHECK(entry.progressions.at(0).barsPerChord == std::vector<int>{2, 2, 4});
}

TEST_CASE("makeBank rejects duplicate ids")
{
    const auto entry = md::parseEntry(validText());
    CHECK_THROWS_AS(md::makeBank({entry, entry}), md::BankError);
}

TEST_CASE("the bank version ignores entry order and tracks content")
{
    const auto a = md::parseEntry(validText());
    const auto b = md::parseEntry(replaced(validText(), "mk-dub-starter-01", "mk-dub-starter-02"));
    CHECK(md::makeBank({a, b}).version == md::makeBank({b, a}).version);
    CHECK(md::makeBank({a, b}).entries.front().id == "mk-dub-starter-01");

    const auto base = md::makeBank({a}).version;
    for (const auto& [from, to] : std::map<std::string, std::string>{
             {"0.56", "0.57"},
             {"peak time", "peak"},
             {"Two-chord", "Three-chord"},
             {"122", "121"},
         })
    {
        CAPTURE(from);
        CHECK(md::makeBank({md::parseEntry(replaced(validText(), from, to))}).version != base);
    }
}

TEST_CASE("the bundled bank covers every family")
{
    const auto bank = md::loadBundledBank();
    struct Totals
    {
        std::size_t progressions = 0;
        std::size_t chordRhythms = 0;
        std::size_t melodyRhythms = 0;
    };
    std::map<md::Family, Totals> byFamily;
    for (const auto& entry : bank.entries)
    {
        auto& totals = byFamily[entry.family];
        totals.progressions += entry.progressions.size();
        totals.chordRhythms += entry.chordRhythms.size();
        totals.melodyRhythms += entry.melodyRhythms.size();
        CHECK(entry.confidence == md::Confidence::placeholder);
    }
    CHECK(byFamily.size() == 5);
    for (const auto& [family, totals] : byFamily)
    {
        CAPTURE(static_cast<int>(family));
        CHECK(totals.progressions >= 6);
        CHECK(totals.chordRhythms >= 6);
        CHECK(totals.melodyRhythms >= 6);
    }
    CHECK(bank.version == md::loadBundledBank().version);
}

TEST_CASE("bundled melody rhythms span 3 to 10 hits per family")
{
    std::map<md::Family, std::pair<std::size_t, std::size_t>> range;
    for (const auto& entry : md::loadBundledBank().entries)
    {
        auto& [low, high] = range.try_emplace(entry.family, 99, 0).first->second;
        for (const auto& cell : entry.melodyRhythms)
        {
            low = std::min(low, cell.hits.size());
            high = std::max(high, cell.hits.size());
        }
    }
    for (const auto& [family, minMax] : range)
    {
        CAPTURE(static_cast<int>(family));
        CHECK(minMax.first == 3);
        CHECK(minMax.second == 10);
    }
}

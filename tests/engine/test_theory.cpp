#include "internal.hpp"
#include "md/theory.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <optional>
#include <vector>

using namespace md;

namespace
{
using Ints = std::vector<int>;

ChordSymbol sym(const char* text)
{
    const auto parsed = parseDegree(text);
    REQUIRE(parsed.has_value());
    return parsed.value_or(ChordSymbol{.degree = 0, .quality = Quality::maj});
}

ProgressionFit fit(Mode mode, std::initializer_list<const char*> degrees)
{
    std::vector<ChordSymbol> chords;
    for (const char* d : degrees)
    {
        chords.push_back(sym(d));
    }
    return checkProgression(mode, chords);
}
} // namespace

TEST_CASE("scaleIntervals", "[theory]")
{
    using A = std::array<int, 7>;
    CHECK(scaleIntervals(Mode::aeolian) == A{0, 2, 3, 5, 7, 8, 10});
    CHECK(scaleIntervals(Mode::dorian) == A{0, 2, 3, 5, 7, 9, 10});
    CHECK(scaleIntervals(Mode::phrygian) == A{0, 1, 3, 5, 7, 8, 10});
    CHECK(scaleIntervals(Mode::ionian) == A{0, 2, 4, 5, 7, 9, 11});
    CHECK(scaleIntervals(Mode::mixolydian) == A{0, 2, 4, 5, 7, 9, 10});
}

TEST_CASE("parseDegree accepts the bank vocabulary", "[theory]")
{
    CHECK(sym("i7") == ChordSymbol{1, Quality::min7});
    CHECK(sym("iv9") == ChordSymbol{4, Quality::min9});
    CHECK(sym("i11") == ChordSymbol{1, Quality::min11});
    CHECK(sym("VImaj7") == ChordSymbol{6, Quality::maj7});
    CHECK(sym("VImaj9") == ChordSymbol{6, Quality::maj9});
    CHECK(sym("IIImaj7") == ChordSymbol{3, Quality::maj7});
    CHECK(sym("VII") == ChordSymbol{7, Quality::maj});
    CHECK(sym("VII6") == ChordSymbol{7, Quality::six});
    CHECK(sym("IV9") == ChordSymbol{4, Quality::dom9});
    CHECK(sym("v7") == ChordSymbol{5, Quality::min7});
    CHECK(sym("V7") == ChordSymbol{5, Quality::dom7});
    CHECK(sym("Isus4") == ChordSymbol{1, Quality::sus4});
    CHECK(sym("IIIadd9") == ChordSymbol{3, Quality::add9});
    CHECK(sym("i") == ChordSymbol{1, Quality::min});
}

TEST_CASE("parseDegree rejects anything else", "[theory]")
{
    for (const char* bad : {"viii", "imaj7", "x7", "", "I13"})
    {
        CHECK_FALSE(parseDegree(bad).has_value());
    }
}

TEST_CASE("chord names and pitch classes", "[theory]")
{
    const Key a{.root = 9, .mode = Mode::aeolian};
    CHECK(chordName(a, sym("i9")) == "Am9");
    CHECK(chordName(a, sym("iv9")) == "Dm9");
    CHECK(chordName(a, sym("VImaj7")) == "Fmaj7");
    CHECK(chordName(a, sym("v9")) == "Em9");
    CHECK(chordName(a, sym("IV9")) == "D9");
    CHECK(chordName(a, sym("VII")) == "G");
    CHECK(chordName(a, sym("VII6")) == "G6");
    CHECK(chordName(Key{.root = 3, .mode = Mode::aeolian}, sym("i7")) == "Ebm7");
    CHECK(chordPitchClasses(a, sym("i9")) == Ints{0, 4, 7, 9, 11});
    CHECK(chordRootPc(a, sym("VI")) == 5);
}

TEST_CASE("scalePitchClasses is sorted", "[theory]")
{
    CHECK(scalePitchClasses(Key{.root = 9, .mode = Mode::aeolian}) == Ints{0, 2, 4, 5, 7, 9, 11});
}

TEST_CASE("chordIntervals", "[theory]")
{
    CHECK(chordIntervals(Quality::maj) == Ints{0, 4, 7});
    CHECK(chordIntervals(Quality::min11) == Ints{0, 3, 7, 10, 14, 17});
    CHECK(chordIntervals(Quality::sus4) == Ints{0, 5, 7});
    CHECK(chordIntervals(Quality::add9) == Ints{0, 4, 7, 14});
}

TEST_CASE("feel words", "[theory]")
{
    CHECK(feelWord(sym("i7"), 0, 4) == "home");
    CHECK(feelWord(sym("ii7"), 1, 4) == "lift");
    CHECK(feelWord(sym("iv7"), 1, 4) == "lift");
    CHECK(feelWord(sym("VImaj7"), 1, 4) == "warm");
    CHECK(feelWord(sym("IIImaj7"), 1, 4) == "bright");
    CHECK(feelWord(sym("VII"), 1, 4) == "push");
    CHECK(feelWord(sym("v7"), 1, 4) == "tension");
    CHECK(feelWord(sym("v7"), 3, 4) == "pull back");
    CHECK(feelWord(sym("v7"), 1, 2) == "tension");
}

TEST_CASE("feel words in an 8-chord progression", "[theory]")
{
    CHECK(feelWord(sym("V7"), 7, 8) == "pull back");
    CHECK(feelWord(sym("V7"), 3, 8) == "tension");
}

TEST_CASE("swingPermille", "[theory]")
{
    CHECK(swingPermille(Groove::straight) == 500);
    CHECK(swingPermille(Groove::swing54) == 540);
    CHECK(swingPermille(Groove::ukShuffle58) == 580);
    CHECK(swingPermille(Groove::heavy62) == 620);
}

TEST_CASE("relatedModes", "[theory]")
{
    using M = std::vector<Mode>;
    CHECK(relatedModes(Mode::aeolian) == M{Mode::phrygian, Mode::dorian});
    CHECK(relatedModes(Mode::phrygian) == M{Mode::aeolian});
    CHECK(relatedModes(Mode::dorian) == M{Mode::aeolian});
    CHECK(relatedModes(Mode::ionian) == M{Mode::mixolydian});
    CHECK(relatedModes(Mode::mixolydian) == M{Mode::ionian});
}

TEST_CASE("checkProgression", "[theory]")
{
    SECTION("one chord borrowed from dorian")
    {
        const auto r = fit(Mode::aeolian, {"i9", "IV9", "VImaj7", "v9"});
        CHECK(r.ok);
        CHECK(r.borrowedFrom == Mode::dorian);
    }
    SECTION("fully diatonic")
    {
        const auto r = fit(Mode::aeolian, {"i7", "iv7"});
        CHECK(r.ok);
        CHECK_FALSE(r.borrowedFrom.has_value());
    }
    SECTION("ok in aeolian, fails in dorian")
    {
        CHECK(fit(Mode::aeolian, {"i7", "VImaj7", "IIImaj7", "VII"}).ok);
        CHECK_FALSE(fit(Mode::dorian, {"i7", "VImaj7", "IIImaj7", "VII"}).ok);
    }
    SECTION("borrowed from phrygian")
    {
        const auto r = fit(Mode::aeolian, {"i7", "vii7"});
        CHECK(r.ok);
        CHECK(r.borrowedFrom == Mode::phrygian);
    }
    SECTION("two borrow sources fail")
    {
        CHECK_FALSE(fit(Mode::aeolian, {"i9", "IV9", "vii7"}).ok);
    }
    SECTION("fits no related mode") { CHECK_FALSE(fit(Mode::aeolian, {"i7", "IImaj7"}).ok); }
}

TEST_CASE("flavour climbs the chord ladders within the key", "[theory][flavour]")
{
    const Key aeolian{.root = 9, .mode = Mode::aeolian};
    const auto flavoured = [&](const char* degree, Flavour flavour)
    { return detail::applyFlavour(aeolian, std::nullopt, sym(degree), flavour); };
    const auto chord = [](int degree, Quality quality)
    { return ChordSymbol{.degree = degree, .quality = quality}; };

    for (const char* degree :
         {"i", "i7", "i9", "i11", "VImaj9", "VII", "V9", "IIIadd9", "Isus4", "IV6"})
    {
        CHECK(flavoured(degree, Flavour::soulful) == sym(degree));
    }

    CHECK(flavoured("i9", Flavour::simple) == chord(1, Quality::min7));
    CHECK(flavoured("i11", Flavour::simple) == chord(1, Quality::min7));
    CHECK(flavoured("VImaj9", Flavour::simple) == chord(6, Quality::maj7));
    CHECK(flavoured("V9", Flavour::simple) == chord(5, Quality::dom7));
    CHECK(flavoured("IIIadd9", Flavour::simple) == chord(3, Quality::maj));
    CHECK(flavoured("i7", Flavour::simple) == sym("i7"));

    CHECK(flavoured("i7", Flavour::jazzy) == chord(1, Quality::min11));
    CHECK(flavoured("i9", Flavour::jazzy) == chord(1, Quality::min11));
    CHECK(flavoured("VII", Flavour::jazzy) == chord(7, Quality::dom9));
    CHECK(flavoured("v7", Flavour::jazzy) == sym("v7"));
    CHECK(flavoured("Isus4", Flavour::jazzy) == sym("Isus4"));
}

TEST_CASE("dom13 has its own intervals, name and pitch classes", "[theory][flavour]")
{
    const Key key{.root = 9, .mode = Mode::aeolian};
    const ChordSymbol dom13{.degree = 5, .quality = Quality::dom13};
    CHECK(chordName(key, dom13) == "E13");
    CHECK(chordPitchClasses(key, dom13) == Ints{1, 2, 4, 6, 8, 11});
}

TEST_CASE("flavour climbs on the borrowed scale and stops at its edge", "[theory][flavour]")
{
    const Key dorian{.root = 9, .mode = Mode::dorian};
    const Key mixolydian{.root = 9, .mode = Mode::mixolydian};
    const Key aeolian{.root = 9, .mode = Mode::aeolian};
    const auto chord = [](int degree, Quality quality)
    { return ChordSymbol{.degree = degree, .quality = quality}; };
    const auto jazzy = [](const Key& key, std::optional<Mode> borrowed, const ChordSymbol& c)
    { return detail::applyFlavour(key, borrowed, c, Flavour::jazzy); };

    // Dm7 and Dm9 hold F natural, which only A aeolian has.
    CHECK(jazzy(dorian, Mode::aeolian, chord(4, Quality::min)) == chord(4, Quality::min9));
    CHECK(jazzy(dorian, std::nullopt, chord(4, Quality::min)) == chord(4, Quality::min));
    // Bm7 fits dorian, Bm9 needs C# which neither scale has.
    CHECK(jazzy(dorian, Mode::aeolian, chord(2, Quality::min)) == chord(2, Quality::min7));

    CHECK(jazzy(aeolian, std::nullopt, chord(3, Quality::maj)) == chord(3, Quality::maj9));
    CHECK(jazzy(mixolydian, std::nullopt, chord(1, Quality::dom9)) == chord(1, Quality::dom13));
    CHECK(jazzy(mixolydian, std::nullopt, chord(1, Quality::dom7)) == chord(1, Quality::dom13));
}

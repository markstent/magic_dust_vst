#include "md/bank.hpp"

#include "md/names.hpp"
#include "md/ranges.hpp"
#include "md/theory.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <optional>
#include <span>
#include <string>
#include <utility>

namespace md::embedded
{
std::span<const std::string_view> bundledEntries();
} // namespace md::embedded

namespace md
{
namespace
{
using Json = nlohmann::json;

constexpr std::uint64_t kFnvOffset = 14695981039346656037ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

template <class E> using Names = std::initializer_list<std::pair<std::string_view, E>>;

constexpr Names<Confidence> kConfidences{
    {"placeholder", Confidence::placeholder},
    {"estimated", Confidence::estimated},
    {"verified", Confidence::verified},
};
constexpr Names<int> kSourceKinds{
    {"hand-authored", 0},
    {"analysis", 1},
    {"learned-clip", 2},
    {"licensed-pack", 3},
};
constexpr Names<VoicingStyle> kVoicings{
    {"close", VoicingStyle::close},
    {"drop-2", VoicingStyle::drop2},
    {"organ-stab", VoicingStyle::organStab},
    {"piano-spread", VoicingStyle::pianoSpread},
};

template <class E> std::optional<E> lookup(Names<E> names, std::string_view text)
{
    const auto it = std::ranges::find(names, text, &std::pair<std::string_view, E>::first);
    return it == names.end() ? std::nullopt : std::optional<E>(it->second);
}

std::optional<int> parseSourceKind(std::string_view text) { return lookup(kSourceKinds, text); }
std::optional<Confidence> parseConfidence(std::string_view text)
{
    return lookup(kConfidences, text);
}
std::optional<VoicingStyle> parseVoicingStyle(std::string_view text)
{
    return lookup(kVoicings, text);
}

std::uint64_t fnv1a(std::uint64_t hash, std::string_view bytes)
{
    for (const char c : bytes)
    {
        hash = (hash ^ static_cast<unsigned char>(c)) * kFnvPrime;
    }
    return hash;
}

bool validId(std::string_view id)
{
    bool segmentEmpty = true;
    for (const char c : id)
    {
        if (c == '-')
        {
            if (segmentEmpty)
            {
                return false;
            }
            segmentEmpty = true;
        }
        else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
        {
            segmentEmpty = false;
        }
        else
        {
            return false;
        }
    }
    return !segmentEmpty;
}

std::string at(const std::string& path, std::size_t index)
{
    return path + "/" + std::to_string(index);
}

// Checks one JSON value at a time and names the entry and path of any fault.
class Reader
{
  public:
    void setId(std::string id) { id_ = std::move(id); }

    [[noreturn]] void fail(const std::string& path, const std::string& what) const
    {
        std::string message = id_.empty() ? "" : id_ + ": ";
        message += (path.empty() ? "/" : path) + ": " + what;
        throw BankError(message);
    }

    // Every field is required and no others are allowed.
    void object(const Json& value, const std::string& path,
                std::initializer_list<std::string_view> keys) const
    {
        if (!value.is_object())
        {
            fail(path, "expected an object");
        }
        for (const auto key : keys)
        {
            if (!value.contains(std::string(key)))
            {
                fail(path, "missing \"" + std::string(key) + "\"");
            }
        }
        for (const auto& [key, unused] : value.items())
        {
            if (std::ranges::find(keys, key) == keys.end())
            {
                fail(path, "unknown key \"" + key + "\"");
            }
        }
    }

    [[nodiscard]] std::size_t arraySize(const Json& value, const std::string& path,
                                        std::size_t minItems = 1) const
    {
        if (!value.is_array())
        {
            fail(path, "expected an array");
        }
        if (value.size() < minItems)
        {
            fail(path, "too few items");
        }
        return value.size();
    }

    [[nodiscard]] int integer(const Json& value, const std::string& path, int low, int high) const
    {
        if (!value.is_number_integer())
        {
            fail(path, "expected an integer");
        }
        const auto number = value.get<std::int64_t>();
        if (number < low || number > high)
        {
            fail(path, "out of range " + std::to_string(low) + " to " + std::to_string(high));
        }
        return static_cast<int>(number);
    }

    [[nodiscard]] double number(const Json& value, const std::string& path) const
    {
        if (!value.is_number())
        {
            fail(path, "expected a number");
        }
        return value.get<double>();
    }

    [[nodiscard]] std::string text(const Json& value, const std::string& path, bool nonEmpty) const
    {
        if (!value.is_string())
        {
            fail(path, "expected a string");
        }
        auto result = value.get<std::string>();
        if (nonEmpty && result.empty())
        {
            fail(path, "must not be empty");
        }
        return result;
    }

    [[nodiscard]] bool boolean(const Json& value, const std::string& path) const
    {
        if (!value.is_boolean())
        {
            fail(path, "expected true or false");
        }
        return value.get<bool>();
    }

    // parse maps the text to a value, or to nothing for an unknown name.
    template <class Parse>
    [[nodiscard]] auto choice(const Json& value, const std::string& path, Parse parse) const
    {
        const auto word = text(value, path, false);
        const auto found = parse(word);
        if (!found)
        {
            fail(path, "unknown value \"" + word + "\"");
        }
        return *found;
    }

    template <class Parse>
    [[nodiscard]] auto choices(const Json& value, const std::string& path, Parse parse) const
    {
        std::vector<decltype(choice(value, path, parse))> result;
        const auto count = arraySize(value, path);
        result.reserve(count);
        for (std::size_t i = 0; i < count; ++i)
        {
            result.push_back(choice(value.at(i), at(path, i), parse));
        }
        auto sorted = result;
        std::ranges::sort(sorted);
        if (std::ranges::adjacent_find(sorted) != sorted.end())
        {
            fail(path, "items must be unique");
        }
        return result;
    }

  private:
    std::string id_;
};

std::vector<int> parseBarsPerChord(const Reader& r, const Json& value, const std::string& path,
                                   std::size_t chordCount)
{
    if (!value.is_array())
    {
        std::vector<int> bars(chordCount, r.integer(value, path, 1, kMaxBarsPerChord));
        return bars;
    }
    if (r.arraySize(value, path) != chordCount)
    {
        r.fail(path, "must have one entry per degree");
    }
    std::vector<int> bars;
    bars.reserve(chordCount);
    for (std::size_t i = 0; i < chordCount; ++i)
    {
        bars.push_back(r.integer(value.at(i), at(path, i), 1, kMaxBarsPerChord));
    }
    return bars;
}

std::vector<ChordSymbol> parseDegrees(const Reader& r, const Json& value, const std::string& path)
{
    std::vector<ChordSymbol> chords;
    for (std::size_t i = 0; i < r.arraySize(value, path); ++i)
    {
        const auto word = r.text(value.at(i), at(path, i), false);
        const auto chord = parseDegree(word);
        if (!chord)
        {
            r.fail(at(path, i), "not a chord degree: \"" + word + "\"");
        }
        chords.push_back(*chord);
    }
    return chords;
}

Progression parseProgression(const Reader& r, const Json& value, const std::string& path,
                             std::span<const Mode> modes, bool parallel)
{
    r.object(value, path, {"degrees", "barsPerChord", "weight"});
    Progression progression;
    progression.chords = parseDegrees(r, value.at("degrees"), path + "/degrees");
    progression.barsPerChord = parseBarsPerChord(r, value.at("barsPerChord"),
                                                 path + "/barsPerChord", progression.chords.size());
    const auto weight = r.number(value.at("weight"), path + "/weight");
    if (weight <= 0.0 || weight * 1000.0 > kWeightMaxMilli)
    {
        r.fail(path + "/weight", "must be above 0 and at most 10");
    }
    progression.weightMilli = static_cast<int>(std::lround(weight * 1000.0));
    const auto fits = [&](Mode mode) { return checkProgression(mode, progression.chords).ok; };
    if (!parallel && !std::ranges::any_of(modes, fits))
    {
        r.fail(path, "the progression fits none of the entry's modes");
    }
    return progression;
}

RhythmCell parseCell(const Reader& r, const Json& value, const std::string& path)
{
    r.object(value, path, {"id", "bars", "hits"});
    RhythmCell cell;
    cell.id = r.text(value.at("id"), path + "/id", true);
    cell.bars = r.integer(value.at("bars"), path + "/bars", 1, 2);
    const auto hitsPath = path + "/hits";
    for (std::size_t i = 0; i < r.arraySize(value.at("hits"), hitsPath); ++i)
    {
        const auto& pair = value.at("hits").at(i);
        const auto hitPath = at(hitsPath, i);
        if (r.arraySize(pair, hitPath, 2) != 2)
        {
            r.fail(hitPath, "expected [step, length]");
        }
        const Hit hit{
            .step = r.integer(pair.at(0), hitPath + "/0", 0, kMaxStep),
            .length = r.integer(pair.at(1), hitPath + "/1", 1, kMaxLength),
        };
        if (hit.step + hit.length > cell.bars * kStepsPerBar)
        {
            r.fail(hitPath, "the hit runs past the end of the cell");
        }
        cell.hits.push_back(hit);
    }
    return cell;
}

std::vector<RhythmCell> parseCells(const Reader& r, const Json& value, const std::string& path)
{
    std::vector<RhythmCell> cells;
    const auto count = r.arraySize(value, path);
    cells.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        cells.push_back(parseCell(r, value.at(i), at(path, i)));
        const auto& cell = cells.back();
        if (std::ranges::count(cells, cell.id, &RhythmCell::id) > 1)
        {
            r.fail(at(path, i) + "/id", "duplicate rhythm cell id \"" + cell.id + "\"");
        }
    }
    return cells;
}

void parseSource(const Reader& r, const Json& value, StyleEntry& entry)
{
    r.object(value, "/source", {"kind", "reference", "confidence"});
    static_cast<void>(r.choice(value.at("kind"), "/source/kind", parseSourceKind));
    if (!value.at("reference").is_null())
    {
        static_cast<void>(r.text(value.at("reference"), "/source/reference", false));
    }
    entry.confidence = r.choice(value.at("confidence"), "/source/confidence", parseConfidence);
}

void parseTempo(const Reader& r, const Json& value, StyleEntry& entry)
{
    r.object(value, "/tempo", {"min", "max"});
    entry.tempoMin = r.integer(value.at("min"), "/tempo/min", kTempoMin, kTempoMax);
    entry.tempoMax = r.integer(value.at("max"), "/tempo/max", kTempoMin, kTempoMax);
    if (entry.tempoMin > entry.tempoMax)
    {
        r.fail("/tempo", "min is above max");
    }
}

void parseVoicing(const Reader& r, const Json& value, StyleEntry& entry)
{
    r.object(value, "/voicing", {"style", "parallel", "topNote"});
    entry.voicingStyle = r.choice(value.at("style"), "/voicing/style", parseVoicingStyle);
    entry.parallel = r.boolean(value.at("parallel"), "/voicing/parallel");
    const auto& top = value.at("topNote");
    if (r.arraySize(top, "/voicing/topNote", 2) != 2)
    {
        r.fail("/voicing/topNote", "expected [low, high]");
    }
    entry.topNoteLow = r.integer(top.at(0), "/voicing/topNote/0", kChordTopLow, kChordTopHigh);
    entry.topNoteHigh = r.integer(top.at(1), "/voicing/topNote/1", kChordTopLow, kChordTopHigh);
    if (entry.topNoteLow > entry.topNoteHigh)
    {
        r.fail("/voicing/topNote", "low is above high");
    }
}

void parseTags(const Reader& r, const Json& value, StyleEntry& entry)
{
    for (std::size_t i = 0; i < r.arraySize(value, "/tags", 0); ++i)
    {
        entry.tags.push_back(r.text(value.at(i), at("/tags", i), false));
    }
}

void parseSwing(const Reader& r, const Json& value, StyleEntry& entry)
{
    const auto swing = r.number(value, "/swing");
    if (swing * 1000.0 < kSwingMinPermille || swing * 1000.0 > kSwingMaxPermille)
    {
        r.fail("/swing", "out of range 0.5 to 0.75");
    }
    entry.swingPermille = static_cast<int>(std::lround(swing * 1000.0));
}
} // namespace

StyleEntry parseEntry(std::string_view text)
{
    const Json doc = Json::parse(text, nullptr, false);
    Reader r;
    if (doc.is_discarded())
    {
        r.fail("", "not valid JSON");
    }
    if (doc.is_object() && doc.contains("id") && doc.at("id").is_string())
    {
        r.setId(doc.at("id").get<std::string>());
    }
    r.object(doc, "",
             {
                 "schemaVersion",
                 "id",
                 "family",
                 "name",
                 "source",
                 "tempo",
                 "modes",
                 "progressions",
                 "voicing",
                 "chordRhythms",
                 "melodyRhythms",
                 "bassPatterns",
                 "melodyModes",
                 "swing",
                 "tags",
             });
    static_cast<void>(r.integer(doc.at("schemaVersion"), "/schemaVersion", 1, 1));
    StyleEntry entry{};
    entry.id = r.text(doc.at("id"), "/id", false);
    if (!validId(entry.id))
    {
        r.fail("/id", "must be lowercase words joined by hyphens");
    }
    entry.family = r.choice(doc.at("family"), "/family", parseFamily);
    entry.name = r.text(doc.at("name"), "/name", true);
    parseSource(r, doc.at("source"), entry);
    parseTempo(r, doc.at("tempo"), entry);
    entry.modes = r.choices(doc.at("modes"), "/modes", parseMode);
    parseVoicing(r, doc.at("voicing"), entry);
    for (std::size_t i = 0; i < r.arraySize(doc.at("progressions"), "/progressions"); ++i)
    {
        entry.progressions.push_back(parseProgression(
            r, doc.at("progressions").at(i), at("/progressions", i), entry.modes, entry.parallel));
    }
    entry.chordRhythms = parseCells(r, doc.at("chordRhythms"), "/chordRhythms");
    entry.melodyRhythms = parseCells(r, doc.at("melodyRhythms"), "/melodyRhythms");
    entry.bassPatterns = r.choices(doc.at("bassPatterns"), "/bassPatterns", parseBassPattern);
    entry.melodyModes = r.choices(doc.at("melodyModes"), "/melodyModes", parseMelodyMode);
    parseSwing(r, doc.at("swing"), entry);
    parseTags(r, doc.at("tags"), entry);
    entry.contentHash = fnv1a(kFnvOffset, doc.dump());
    return entry;
}

Bank makeBank(std::vector<StyleEntry> entries)
{
    std::ranges::sort(entries, {}, &StyleEntry::id);
    const auto duplicate = std::ranges::adjacent_find(
        entries, [](const StyleEntry& a, const StyleEntry& b) { return a.id == b.id; });
    if (duplicate != entries.end())
    {
        throw BankError("duplicate entry id: " + duplicate->id);
    }
    std::uint64_t version = kFnvOffset;
    for (const auto& entry : entries)
    {
        std::array<char, 8> bytes{};
        for (std::size_t i = 0; i < bytes.size(); ++i)
        {
            bytes.at(i) = static_cast<char>((entry.contentHash >> (8 * i)) & 0xFFU);
        }
        version = fnv1a(version, std::string_view(bytes.data(), bytes.size()));
    }
    return Bank{.entries = std::move(entries), .version = version};
}

Bank loadBundledBank()
{
    std::vector<StyleEntry> entries;
    for (const auto text : embedded::bundledEntries())
    {
        entries.push_back(parseEntry(text));
    }
    return makeBank(std::move(entries));
}
} // namespace md

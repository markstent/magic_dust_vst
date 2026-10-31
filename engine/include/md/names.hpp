#pragma once

#include "md/style.hpp"
#include "md/types.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace md
{
// The one place that spells names: bank JSON, md-cli flags, chord names and lane patterns.
// The parse functions return nothing for text that is not exactly a name.

// Root names use the fixed spelling C C# D Eb E F F# G Ab A Bb B. Throws std::out_of_range for a
// root outside 0 to 11.
std::string_view rootName(int root);
// A note letter then an optional # or b, any case, any enharmonic spelling: a, c#, Bb, Db.
std::optional<int> parseRoot(std::string_view text);
// Lowercase root and mode: "bb aeolian", "c# dorian".
std::string keyName(const Key& key);

std::string_view modeName(Mode mode);
std::optional<Mode> parseMode(std::string_view text);

std::string_view bassPatternName(BassPattern pattern);
std::optional<BassPattern> parseBassPattern(std::string_view text);

std::string_view melodyModeName(MelodyMode mode);
std::optional<MelodyMode> parseMelodyMode(std::string_view text);

std::string_view familyName(Family family);
std::optional<Family> parseFamily(std::string_view text);

std::string_view styleName(Style style);
std::optional<Style> parseStyle(std::string_view text);
} // namespace md

#pragma once

#include "md/types.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace md
{
std::array<int, 7> scaleIntervals(Mode mode);
std::optional<ChordSymbol> parseDegree(std::string_view text);
int chordRootPc(const Key& key, const ChordSymbol& chord);
std::vector<int> chordIntervals(Quality quality);
// Sorted ascending, 0 to 11.
std::vector<int> chordPitchClasses(const Key& key, const ChordSymbol& chord);
// Sorted ascending, 0 to 11.
std::vector<int> scalePitchClasses(const Key& key);
std::string chordName(const Key& key, const ChordSymbol& chord);
std::string feelWord(const ChordSymbol& chord, std::size_t slot, std::size_t count);
int swingPermille(Groove groove);
std::vector<Mode> relatedModes(Mode mode);

struct ProgressionFit
{
    bool ok;
    std::optional<Mode> borrowedFrom;
};

// Pitch classes are taken relative to root 0, so the result holds in every key.
ProgressionFit checkProgression(Mode mode, std::span<const ChordSymbol> chords);
} // namespace md

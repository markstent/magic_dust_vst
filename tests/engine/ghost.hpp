#pragma once

#include "md/types.hpp"

namespace md::detail
{
// Above this a note is played, not a ghost: a ghost is at most 44 - 8 + 3 = 39 once humanized and a
// played note at least 80 * 85 / 100 - 11 = 57.
constexpr int kGhostCeiling = 55;

// True for a ghost note, as the engine writes it (velocity 40) or after humanize.
inline bool isGhost(const NoteEvent& e) { return e.velocity < kGhostCeiling; }
} // namespace md::detail

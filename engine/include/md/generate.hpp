#pragma once

#include "md/style.hpp"
#include "md/types.hpp"

#include <cstdint>

namespace md
{
// Writes an idea from the bank, deterministically from the seed.
// Throws std::invalid_argument for a seed above kSeedMax, bars other than 4, 8 or 16, a key root
// outside 0 to 11, mood or energy outside 0 to 100, or a bank with no entry for the style.
// Throws std::logic_error listing the violations if the result fails validate(), which is a bug.
Idea generate(const Bank& bank, const Settings& settings, std::uint32_t seed);
} // namespace md

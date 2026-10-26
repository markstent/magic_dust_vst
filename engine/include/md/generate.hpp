#pragma once

#include "md/style.hpp"
#include "md/types.hpp"

#include <cstdint>

namespace md
{
// One random stream seed per lane, so rerolling one lane leaves the others' draws alone.
struct LaneSeeds
{
    std::uint64_t chords;
    std::uint64_t bass;
    std::uint64_t melody;
};

// Each lane's seed for an idea that has not been rerolled: laneSeed(ideaSeed, <lane stream>, 0).
LaneSeeds defaultLaneSeeds(std::uint32_t ideaSeed);

// Writes an idea from the bank, deterministically from the seed.
// Throws std::invalid_argument for a seed above kSeedMax, bars other than 4, 8 or 16, a key root
// outside 0 to 11, mood or energy outside 0 to 100, or a bank with no entry for the style.
// Throws std::logic_error listing the violations if the result fails validate(), which is a bug.
Idea generate(const Bank& bank, const Settings& settings, std::uint32_t seed);
// As above, with each lane drawing only from its own seed. The chords seed also chooses the entry,
// progression and chord rhythm, and the melody follows the chords it is given.
Idea generate(const Bank& bank, const Settings& settings, std::uint32_t seed,
              const LaneSeeds& laneSeeds);
} // namespace md

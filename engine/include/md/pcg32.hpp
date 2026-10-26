#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

namespace md
{
// PCG32 as in the reference pcg_basic.c.
class Pcg32
{
  public:
    Pcg32(std::uint64_t seed, std::uint64_t stream);
    std::uint32_t next();
    // Throws std::invalid_argument for bound 0.
    std::uint32_t below(std::uint32_t bound);
    // Throws std::invalid_argument for a negative weight.
    std::size_t pickWeighted(std::span<const int> weights);

  private:
    std::uint64_t state_ = 0;
    std::uint64_t inc_ = 0;
};

enum class Stream : std::uint8_t
{
    chords,
    bass,
    melody,
};

// Throws std::invalid_argument if slot is 2^24 or more.
std::uint64_t laneSeed(std::uint32_t ideaSeed, Stream stream, std::uint32_t slot);
} // namespace md

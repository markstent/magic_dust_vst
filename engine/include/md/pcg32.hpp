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
    std::uint32_t below(std::uint32_t bound);
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
    humanize,
};

// slot must be below 2^24.
std::uint64_t laneSeed(std::uint32_t ideaSeed, Stream stream, std::uint32_t slot);
} // namespace md

#include "md/pcg32.hpp"

#include <stdexcept>

namespace md
{
namespace
{
std::uint64_t splitmix64(std::uint64_t x)
{
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27U)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31U);
}
} // namespace

// NOLINTNEXTLINE(bugprone-easily-swappable-parameters): (seed, stream) is the reference order.
Pcg32::Pcg32(std::uint64_t seed, std::uint64_t stream) : inc_((stream << 1U) | 1U)
{
    next();
    state_ += seed;
    next();
}

std::uint32_t Pcg32::next()
{
    const std::uint64_t old = state_;
    state_ = (old * 6364136223846793005ULL) + inc_;
    const auto xorshifted = static_cast<std::uint32_t>(((old >> 18U) ^ old) >> 27U);
    const auto rot = static_cast<std::uint32_t>(old >> 59U);
    return (xorshifted >> rot) | (xorshifted << ((~rot + 1U) & 31U));
}

std::uint32_t Pcg32::below(std::uint32_t bound)
{
    if (bound == 0)
    {
        throw std::invalid_argument("Pcg32::below: bound must be positive");
    }
    const std::uint32_t threshold = (~bound + 1U) % bound;
    for (;;)
    {
        const std::uint32_t r = next();
        if (r >= threshold)
        {
            return r % bound;
        }
    }
}

std::size_t Pcg32::pickWeighted(std::span<const int> weights)
{
    std::uint32_t sum = 0;
    for (const int w : weights)
    {
        if (w < 0)
        {
            throw std::invalid_argument("Pcg32::pickWeighted: negative weight");
        }
        sum += static_cast<std::uint32_t>(w);
    }
    if (sum == 0)
    {
        return 0;
    }
    std::uint32_t roll = below(sum);
    for (std::size_t i = 0; i < weights.size(); ++i)
    {
        const auto w = static_cast<std::uint32_t>(weights.subspan(i, 1).front());
        if (roll < w)
        {
            return i;
        }
        roll -= w;
    }
    return weights.size() - 1;
}

std::uint64_t laneSeed(std::uint32_t ideaSeed, Stream stream, std::uint32_t slot)
{
    if (slot >= (1U << 24U))
    {
        throw std::invalid_argument("laneSeed: slot must be below 2^24");
    }
    return splitmix64((static_cast<std::uint64_t>(ideaSeed) << 32U) |
                      (static_cast<std::uint64_t>(stream) << 24U) | slot);
}
} // namespace md

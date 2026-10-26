#include "md/pcg32.hpp"

#include <catch2/catch_test_macros.hpp>

#include <array>
#include <set>

using namespace md;

TEST_CASE("Pcg32 matches the reference pcg32-demo output", "[pcg32]")
{
    Pcg32 rng(42, 54);
    for (const std::uint32_t expected :
         {0xa15c02b7U, 0x7b47f409U, 0xba1d3330U, 0x83d2f293U, 0xbfa4784bU, 0xcbed606eU})
    {
        CHECK(rng.next() == expected);
    }
}

TEST_CASE("Pcg32 below stays in range and covers it", "[pcg32]")
{
    Pcg32 rng(1, 2);
    std::set<std::uint32_t> seen;
    for (int i = 0; i < 10000; ++i)
    {
        const auto v = rng.below(7);
        REQUIRE(v < 7);
        seen.insert(v);
    }
    CHECK(seen.size() == 7);
}

TEST_CASE("Pcg32 pickWeighted respects zero weights", "[pcg32]")
{
    Pcg32 rng(3, 4);
    const std::array<int, 3> weights{0, 1000, 0};
    for (int i = 0; i < 100; ++i)
    {
        CHECK(rng.pickWeighted(weights) == 1);
    }
}

TEST_CASE("laneSeed depends on every argument", "[pcg32]")
{
    const auto base = laneSeed(48213, Stream::bass, 2);
    CHECK(base == laneSeed(48213, Stream::bass, 2));
    CHECK(base != laneSeed(48214, Stream::bass, 2));
    CHECK(base != laneSeed(48213, Stream::melody, 2));
    CHECK(base != laneSeed(48213, Stream::bass, 3));
}

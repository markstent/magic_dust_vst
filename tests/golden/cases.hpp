#pragma once

#include "md/types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

namespace md::golden
{
constexpr int kCaseCount = 50;
constexpr int kTempoBpm = 124;

struct Case
{
    Settings settings;
    std::uint32_t seed;
    std::string fileName;
};

inline Case makeCase(int i)
{
    constexpr std::array<std::array<int, 2>, 5> aura = {
        {{0, 0}, {0, 100}, {100, 0}, {100, 100}, {30, 50}}};
    const auto& [mood, energy] = aura.at(static_cast<std::size_t>(i % 5));

    Case c;
    c.settings.style = static_cast<Style>(i % 6);
    c.settings.keyRoot = i % 12;
    c.settings.mood = mood;
    c.settings.energy = energy;
    c.settings.groove = static_cast<Groove>(i % 4);
    c.settings.flavour = static_cast<Flavour>(i % 3);
    c.settings.bars = i % 10 == 4 ? 4 : (i % 10 == 9 ? 16 : 8);
    c.seed = static_cast<std::uint32_t>((1000 + i * 7919) % 100000);

    char name[24];
    std::snprintf(name, sizeof name, "case-%02d.mid", i);
    c.fileName = name;
    return c;
}
} // namespace md::golden

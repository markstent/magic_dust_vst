#pragma once

#include "md/types.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace md
{
struct DecodedIdea
{
    std::uint32_t seed{};
    Settings settings;
    std::uint16_t bankVersion16{};
};

// 13 Crockford base32 characters; only the low 16 bits of bankVersion are kept.
// Throws std::invalid_argument for an out-of-range seed or setting.
std::string encodeIdea(std::uint32_t seed, const Settings& settings, std::uint64_t bankVersion);
std::optional<DecodedIdea> decodeIdea(std::string_view code);
} // namespace md

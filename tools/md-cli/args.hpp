#pragma once

#include "md/types.hpp"

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <variant>

namespace md
{
struct CliOptions
{
    Settings settings;
    std::uint32_t seed = 48213;
    int tempo = 124;
    std::filesystem::path out;
};

// The string alternative is the error message.
std::variant<CliOptions, std::string> parseArgs(std::span<const std::string_view> args);
} // namespace md

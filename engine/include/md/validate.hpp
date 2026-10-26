#pragma once

#include "md/types.hpp"

#include <string>
#include <vector>

namespace md
{
// One human-readable line per violation; empty means the idea is valid.
std::vector<std::string> validate(const Idea& idea);
} // namespace md

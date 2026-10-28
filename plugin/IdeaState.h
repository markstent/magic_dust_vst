#pragma once

#include <optional>

#include <juce_data_structures/juce_data_structures.h>

#include "md/types.hpp"

// An idea as a value tree, every field kept so a round trip is exact. Seeds are strings, since a
// property cannot hold 64 bits.
juce::ValueTree ideaToTree(const md::Idea& idea);
// Nullopt when the tree is not an idea this build wrote: a missing or out-of-range field.
std::optional<md::Idea> ideaFromTree(const juce::ValueTree& tree);

#pragma once

#include <juce_graphics/juce_graphics.h>

#include "ui/Tokens.h"

namespace md::ui
{

// True once the pointer has moved far enough from where it went down to count as a drag.
inline bool exceedsDragThreshold(juce::Point<int> down, juce::Point<int> now)
{
    return down.getDistanceFrom(now) > tokens::dragThreshold;
}

} // namespace md::ui

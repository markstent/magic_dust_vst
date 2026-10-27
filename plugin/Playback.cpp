#include "plugin/Playback.h"

#include <algorithm>

#include "md/ranges.hpp"

namespace md
{

PlaybackPattern makePattern(const Idea& idea)
{
    PlaybackPattern p{.events = {}, .lengthTicks = idea.settings.bars * kTicksPerBar};
    std::uint8_t channel = 0;
    for (const Lane& lane : idea.lanes)
    {
        for (const NoteEvent& n : lane.events)
        {
            if (lane.muted)
            {
                break;
            }
            const auto note = static_cast<std::uint8_t>(n.pitch);
            p.events.push_back({.tick = n.start,
                                .channel = channel,
                                .note = note,
                                .velocity = static_cast<std::uint8_t>(n.velocity),
                                .on = true});
            p.events.push_back({.tick = n.start + n.length,
                                .channel = channel,
                                .note = note,
                                .velocity = 0,
                                .on = false});
        }
        ++channel;
    }
    std::ranges::stable_sort(p.events, [](const PlayEvent& a, const PlayEvent& b)
                             { return a.tick != b.tick ? a.tick < b.tick : !a.on && b.on; });
    return p;
}

} // namespace md

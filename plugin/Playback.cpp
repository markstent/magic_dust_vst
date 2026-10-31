#include "plugin/Playback.h"

#include <algorithm>

#include "md/midifile.hpp"
#include "md/ranges.hpp"

namespace md
{

namespace
{

void addLane(PlaybackPattern& p, const Idea& idea, LaneId lane, std::uint8_t channel)
{
    for (const NoteEvent& n : performedNotes(idea, lane))
    {
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
}

} // namespace

PlaybackPattern makePattern(const Idea& idea)
{
    PlaybackPattern p{.events = {}, .lengthTicks = idea.settings.bars * kTicksPerBar};
    for (std::uint8_t channel = 0; const Lane& lane : idea.lanes)
    {
        if (!lane.muted)
        {
            addLane(p, idea, static_cast<LaneId>(channel), channel);
        }
        ++channel;
    }
    std::ranges::stable_sort(p.events, [](const PlayEvent& a, const PlayEvent& b)
                             { return a.tick != b.tick ? a.tick < b.tick : !a.on && b.on; });
    return p;
}

} // namespace md

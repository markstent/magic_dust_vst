#include "md/generate.hpp"

#include "internal.hpp"
#include "md/ranges.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <tuple>
#include <vector>

namespace md
{
namespace
{
// The valid pitch nearest the note's: at equal distance a chord tone first, then the lower.
// Valid means no clash even by approach exemption, so the fix stays valid when neighbours move.
int nearestValidPitch(const Idea& idea, const NoteEvent& e)
{
    const auto sounding = detail::soundingPcsAt(idea, e.start);
    int best = e.pitch;
    std::tuple<int, bool, int> bestKey{1000, false, 0};
    for (int pitch = kBassLow; pitch <= kBassHigh; ++pitch)
    {
        NoteEvent candidate = e;
        candidate.pitch = pitch;
        if (!detail::bassPitchOk(idea, candidate, {}))
        {
            continue;
        }
        const bool chordTone = std::ranges::find(sounding, pitch % 12) != sounding.end();
        const std::tuple<int, bool, int> key{std::abs(pitch - e.pitch), !chordTone, pitch};
        if (key < bestKey)
        {
            bestKey = key;
            best = pitch;
        }
    }
    return best;
}
} // namespace

bool refitBass(Idea& idea)
{
    Lane& lane = idea.lanes.at(static_cast<std::size_t>(LaneId::bass));
    bool changed = false;
    // A snapped note is valid without any exemption, so each pass only fixes notes that stay
    // fixed; a pass can only break an approach note whose target moved, so this settles quickly.
    for (bool again = true; again;)
    {
        again = false;
        const std::vector<int> approaches = detail::approachStarts(idea);
        for (NoteEvent& e : lane.events)
        {
            if (!detail::bassPitchOk(idea, e, approaches))
            {
                const int pitch = nearestValidPitch(idea, e);
                again = again || pitch != e.pitch;
                e.pitch = pitch;
            }
        }
        changed = changed || again;
    }
    lane.refitted = lane.refitted || changed;
    return changed;
}
} // namespace md

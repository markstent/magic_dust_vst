#include "md/generate.hpp"

#include "internal.hpp"
#include "md/ranges.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <optional>
#include <tuple>
#include <vector>

namespace md
{
namespace
{
struct PitchRange
{
    int low;
    int high;
};

// The nearest pitch in the range to high that passes valid: a chord tone of the sounding chord
// first, the lower on a tie; a scale tone only when no chord tone passes.
template <class Valid>
int nearestValidPitch(const Idea& idea, const NoteEvent& e, PitchRange range, const Valid& valid)
{
    const auto sounding = detail::soundingPcsAt(idea, e.start);
    int best = e.pitch;
    std::tuple<bool, int, int> bestKey{true, 1000, 0};
    for (int pitch = range.low; pitch <= range.high; ++pitch)
    {
        NoteEvent candidate = e;
        candidate.pitch = pitch;
        if (!valid(candidate))
        {
            continue;
        }
        const bool chordTone = std::ranges::find(sounding, pitch % 12) != sounding.end();
        const std::tuple<bool, int, int> key{!chordTone, std::abs(pitch - e.pitch), pitch};
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
    // Ends because a snapped note is valid without any exemption and never changes again, so each
    // pass only fixes notes that were still failing.
    for (bool again = true; again;)
    {
        again = false;
        const std::vector<int> approaches = detail::approachStarts(idea);
        for (NoteEvent& e : lane.events)
        {
            if (!detail::bassPitchOk(idea, e, approaches))
            {
                // Valid means no clash even by approach exemption, so the fix stays valid when
                // neighbours move.
                const int pitch =
                    nearestValidPitch(idea, e, {.low = kBassLow, .high = kBassHigh},
                                      [&idea](const NoteEvent& candidate)
                                      { return detail::bassPitchOk(idea, candidate, {}); });
                again = again || pitch != e.pitch;
                e.pitch = pitch;
            }
        }
        changed = changed || again;
    }
    lane.refitted = lane.refitted || changed;
    return changed;
}

bool refitMelody(Idea& idea)
{
    Lane& lane = idea.lanes.at(static_cast<std::size_t>(LaneId::melody));
    bool changed = false;
    std::optional<int> previous;
    // One pass is enough: each note is fitted against the previous note as already fitted.
    for (NoteEvent& e : lane.events)
    {
        if (!detail::melodyPitchOk(idea, e) ||
            (previous && !detail::melodyLeapOk(*previous, e.pitch)))
        {
            // Valid includes the leap from the already fitted previous note.
            const int pitch = nearestValidPitch(
                idea, e, {.low = kMelodyLow, .high = kMelodyHigh},
                [&idea, previous](const NoteEvent& candidate)
                {
                    return detail::melodyPitchOk(idea, candidate) &&
                           (!previous || detail::melodyLeapOk(*previous, candidate.pitch));
                });
            changed = changed || pitch != e.pitch;
            e.pitch = pitch;
        }
        previous = e.pitch;
    }
    lane.refitted = lane.refitted || changed;
    return changed;
}
} // namespace md

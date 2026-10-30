#pragma once

#include "md/style.hpp"
#include "md/types.hpp"

#include <cstdint>

namespace md
{
// One random stream seed per lane, so rerolling one lane leaves the others' draws alone.
struct LaneSeeds
{
    std::uint64_t chords;
    std::uint64_t bass;
    std::uint64_t melody;
};

// Each lane's seed for an idea that has not been rerolled: laneSeed(ideaSeed, <lane stream>, 0).
LaneSeeds defaultLaneSeeds(std::uint32_t ideaSeed);

// Writes an idea from the bank, deterministically from the seed. The progression and the chord
// and melody rhythms are invented from models learned from the bank's entries, a fifth of each
// taken as written; a lane whose invented material fails validate takes written material instead.
// Throws std::invalid_argument for a seed above kSeedMax, bars other than 4, 8 or 16, a key root
// outside 0 to 11, mood or energy outside 0 to 100, or a bank with no entry for the style.
// Throws std::logic_error listing the violations if the result fails validate(), which is a bug.
Idea generate(const Bank& bank, const Settings& settings, std::uint32_t seed);
// As above, with each lane drawing only from its own seed. The chords seed also chooses the entry,
// progression and chord rhythm, and the melody follows the chords it is given.
Idea generate(const Bank& bank, const Settings& settings, std::uint32_t seed,
              const LaneSeeds& laneSeeds);

// Which lanes of a previous idea a regeneration keeps.
struct Pins
{
    bool chords = false;
    bool bass = false;
    bool melody = false;
};

// As above, keeping the pinned lanes of the previous idea. A pinned chords lane keeps its entry,
// progression and chord events; a pinned bass or melody keeps its events and is refitted to the
// chords. A key change moves the kept lanes by the shortest interval (-5 to +6 semitones), an
// octave back into range where needed. Returns exactly the unpinned overload when nothing is pinned
// or when the bar count differs from the previous idea's. With everything pinned the idea number is
// kept.
Idea generate(const Bank& bank, const Settings& settings, std::uint32_t seed,
              const LaneSeeds& laneSeeds, const Idea& previous, Pins pins);

// Snaps every bass note that fails the pitch rules against the current chords to the nearest valid
// chord tone of the sounding chord (ties go down), or to the nearest valid scale tone only when no
// chord tone passes. Always in range. Starts, lengths and velocities never move. Sets the bass
// lane's refitted flag and returns true when any pitch changed.
bool refitBass(Idea& idea);

// Snaps every melody note that fails the pitch rules (range, in key or chord, the leap from the
// previous note, the strong-beat rule) to the nearest valid chord tone of the sounding chord (ties
// go down), or to the nearest valid scale tone when no chord tone passes. Starts, lengths and
// velocities never move. Sets the melody lane's refitted flag and returns true when any pitch
// changed.
bool refitMelody(Idea& idea);
} // namespace md

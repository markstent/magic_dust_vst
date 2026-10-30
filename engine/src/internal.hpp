#pragma once

#include "md/generate.hpp"
#include "md/pcg32.hpp"
#include "md/ranges.hpp"
#include "md/style.hpp"
#include "md/types.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace md::detail
{
// Every random draw is Pcg32(<lane seed>, slot) with one fixed slot per use:
//   chords: 0 entry, 1 progression, 2 chord rhythm cell, 3 velocity humanize, 4 turnaround cell,
//           5 ghosts, 6 lengths, 7 inversions
//   bass:   0 velocity humanize
//           1 pattern choice
//           2 pitch variation
//           3 approach notes
//           4 riff motif
//           5 call-and-response picks
//           6 ghosts
//           7 turnaround fill
//   melody: 0 rhythm cell A, 1 rhythm cell B, 2 pitch walk, 3 pluck tones, 4 velocity humanize,
//           5 answer cell, 6 answer pitches,
//           7 timing feel (drawn by performedNotes, not the generator)
// Entry, progression, mode and chord-cell choices belong to the chords lane.
constexpr std::uint32_t kChordEntry = 0;
constexpr std::uint32_t kChordProgression = 1;
constexpr std::uint32_t kChordCell = 2;
constexpr std::uint32_t kChordHumanize = 3;
constexpr std::uint32_t kChordTurnaround = 4;
constexpr std::uint32_t kChordGhost = 5;
constexpr std::uint32_t kChordLength = 6;
constexpr std::uint32_t kChordInversion = 7;
constexpr std::uint32_t kBassHumanize = 0;
constexpr std::uint32_t kBassPattern = 1;
constexpr std::uint32_t kBassPitch = 2;
constexpr std::uint32_t kBassApproach = 3;
constexpr std::uint32_t kBassRiff = 4;
constexpr std::uint32_t kBassCall = 5;
constexpr std::uint32_t kBassGhost = 6;
constexpr std::uint32_t kBassFill = 7;
constexpr std::uint32_t kMelodyCellA = 0;
constexpr std::uint32_t kMelodyCellB = 1;
constexpr std::uint32_t kMelodyWalk = 2;
constexpr std::uint32_t kMelodyPluck = 3;
constexpr std::uint32_t kMelodyHumanize = 4;
constexpr std::uint32_t kMelodyAnswerCell = 5;
constexpr std::uint32_t kMelodyAnswerWalk = 6;
constexpr std::uint32_t kMelodyFeel = 7; // drawn by performedNotes, not the generator

// Energy tiers: the turnaround and the answer phrase from kMidEnergy up; the bass fill and the
// breath above kHighEnergy.
constexpr int kMidEnergy = 30;
constexpr int kHighEnergy = 60;

// A ghost note's velocity, as a percent of the lane base.
constexpr int kGhostVelocity = 40;
// A floor on a varied chord hit's length, half a 16th. Today's 70% minimum never reaches it.
constexpr int kMinChordLength = kTicksPerSixteenth / 2;

// True for the bar (0-based) that ends a phrase and turns the idea around: the last bar, and bar 8
// of a 16-bar idea.
bool isTurnaroundBar(int bars, int bar);
// True for the bar that breathes: bar 4 of each 8 (a 4-bar idea has none).
bool isBreathBar(int bars, int bar);

// An inclusive pitch range.
struct PitchRange
{
    int low;
    int high;
};

// The 16th step of a swung start: an odd step lands in the second half of its pair of 16ths.
int stepOf(int start);

// Odd sixteenth steps land late: start = pairStart + 480 * swingPermille / 1000.
int swung(int start, int swingPermille);

// The chord sounding at the tick, or null when none does.
const PlacedChord* findChord(std::span<const PlacedChord> progression, int tick);
// As findChord, but throws std::logic_error when none sounds.
const PlacedChord& chordAt(std::span<const PlacedChord> progression, int tick);

// The pitch classes sounding for a chord: its symbol's, or its voicing's when the idea is parallel
// (the shape moves with the root and may leave the symbol).
std::vector<int> soundingPitchClasses(const Idea& idea, const PlacedChord& chord);

// The pitch classes sounding at the tick, as soundingPitchClasses; empty when no chord sounds.
std::vector<int> soundingPcsAt(const Idea& idea, int tick);

// A chord change a bass note can lead into: its approach tick and the pitch it leads to.
struct ApproachTarget
{
    int tick;
    int pitch;
};
// The one approach rule, for the generator, validator and refit. The bass must be sorted by start.
std::vector<ApproachTarget> approachTargets(const Idea& idea, std::span<const NoteEvent> bass);
// Start ticks of the bass notes that lead into a chord change (the validator's approach exemption).
std::vector<int> approachStarts(const Idea& idea);
// True when the note's pitch class is in the key scale or is a chord tone of the chord sounding at
// its start (the voicing's, for a parallel idea).
bool pitchInKeyOrChord(const Idea& idea, const NoteEvent& e);
// True when a bass note is a semitone off the chord sounding at its start, outside the pedal and
// approach exemptions.
bool bassClashes(const Idea& idea, const NoteEvent& e, std::span<const int> approaches);
// The pitch rules for a bass note: in range, in key or a chord tone, and no clash. Shared by the
// validator and the refit so they cannot disagree.
bool bassPitchOk(const Idea& idea, const NoteEvent& e, std::span<const int> approaches);

// True when a melody leap from one pitch to the next is within kMaxMelodyLeap or exactly an octave.
bool melodyLeapOk(int from, int to);
// True when a melody note starts on a quarter and its pitch class is not allowed against the chord
// sounding there.
bool melodyStrongBeatClash(const Idea& idea, const NoteEvent& e);
// The pitch rules for one melody note, apart from its leap: in range, in key or a chord tone, and
// no strong-beat clash. Shared by the validator and the refit so they cannot disagree.
bool melodyPitchOk(const Idea& idea, const NoteEvent& e);

// The pitch classes at least 2 semitones from every other one in the set.
std::vector<int> allowedPitchClasses(const std::vector<int>& pcs);

// True when a voicing has an interval below a perfect 4th under MIDI 52 (which rules out 3rds
// under 48 too).
bool isMuddy(const Voicing& voicing);

// Calls f(voice, move) for each pair of voices between two voicings: the top voices pair with each
// other and the rest pair from the bottom, min(size) - 1 of them; a larger chord's extra voices are
// unpaired. voice is the index in a; move is the distance in semitones.
template <class F> void forEachVoicePair(const Voicing& a, const Voicing& b, const F& f)
{
    const std::size_t paired = std::min(a.size(), b.size()) - 1;
    for (std::size_t i = 0; i < paired; ++i)
    {
        f(i, std::abs(a.at(i) - b.at(i)));
    }
    f(a.size() - 1, std::abs(a.back() - b.back()));
}

// Ends each note at the next onset and at endTick, and drops notes left shorter than 1 tick.
// For monophonic lanes whose events are sorted by start.
void clampMonophonic(std::vector<NoteEvent>& events, int endTick);

// Soulful keeps the written chord. Simple steps it down to a triad or 7th (add9 to maj). Jazzy
// climbs up to two steps (min to min11, maj to maj9 or via dom7, dom7 to dom13), each needing every
// note in the key's scale or the borrowed mode's; sus4 and six never change.
ChordSymbol applyFlavour(const Key& key, std::optional<Mode> borrowedFrom, const ChordSymbol& chord,
                         Flavour flavour);

// Tiles the progression's chords from bar 0 and truncates the last at bars. Voicings stay empty.
std::vector<PlacedChord> placeProgression(const Progression& progression, int bars);
void voiceChords(const StyleEntry& entry, const Key& key, std::vector<PlacedChord>& chords);
// The entry's modes that fit the whole progression (all of them for parallel entries), darkest
// first.
std::vector<Mode> fittingModes(const StyleEntry& entry, const Progression& progression);
// Places the progression over the idea's bars in its key, applies its flavour and sets the mode it
// borrows from. Voicings stay empty.
void placeChords(Idea& idea, const Progression& progression);

// The mode groups a progression model learns within.
enum class ModeGroup : std::uint8_t
{
    minor, // aeolian, dorian, phrygian
    major, // ionian, mixolydian
    both,  // an entry with modes in both learns from the two together
};
ModeGroup modeGroup(const StyleEntry& entry);

// A first-order chain over the chord tokens of a family's progressions whose entries have a mode in
// the group. Tokens are indexed in order of first appearance.
struct ProgressionModel
{
    std::vector<ChordSymbol> tokens;
    std::vector<std::vector<int>> transitions; // counts [from][to], with each wrap to the first
    std::vector<int> starts;                   // counts of first chords
    std::vector<std::vector<int>> templates;   // the learned barsPerChord lists
    bool sevenths = false;                     // most learned tokens carry a 7th or a 9th
};

ProgressionModel buildProgressionModel(const Bank& bank, Family family, ModeGroup group);
// The plain diatonic chords of the entry's first mode that the model has not learned: 7ths when
// the model's tokens mostly carry one, otherwise triads. A degree with neither is left out.
std::vector<ChordSymbol> smoothingTokens(const ProgressionModel& model, const StyleEntry& entry);
// Draws a bar template, a first chord and a chain from the model, with smoothing: 0.15 added to
// every move between learned tokens and 0.05 for any move to or from a smoothing token. A chord
// never follows one on the same degree, the wrap included, and a 1-chord template holds a learned
// tonic. The result must fit one of the entry's modes and pass validate at the key root in every
// fitting mode and flavour. Gives up after 24 tries.
std::optional<Progression> inventProgression(const ProgressionModel& model, const StyleEntry& entry,
                                             int keyRoot, Pcg32& rng);
// The weight of a move between states (the model's tokens first, then smoothing tokens), in
// thousandths: 1000 per learned count plus 150 between learned tokens, 50 for any move to or from
// a smoothing token, and 0 for staying on the same degree.
int moveWeight(const ProgressionModel& model, const std::vector<ChordSymbol>& states,
               std::size_t from, std::size_t to);

enum class RhythmKind : std::uint8_t
{
    chords,
    melody,
};

// The (step in the beat, length) of each onset in one 4-step beat of a slice.
using Chunk = std::vector<std::pair<int, int>>;
constexpr int kBeatSteps = 4;
constexpr int kBeats = kStepsPerBar / kBeatSteps;

// The beat chunks of a family's chord or melody cells, cut from their 1-bar slices (a 2-bar cell
// gives two). chunks[b] are the distinct chunks seen at beat b. starts counts the chunks at beat 0
// and next[b][prev][cur] counts a chunk at beat b after prev at beat b - 1 (next[0] is unused).
// The onsets-per-slice range, the least gap between onsets and the mean onsets in a chunk are
// learned too.
struct RhythmModel
{
    std::string name; // "fresh-stabs" or "fresh-hook"
    RhythmKind kind = RhythmKind::chords;
    std::array<std::vector<Chunk>, kBeats> chunks;
    std::vector<int> starts;
    std::array<std::vector<std::vector<int>>, kBeats> next;
    int slices = 0;
    int minOnsets = 0;
    int maxOnsets = 0;
    int minGap = kStepsPerBar;
    double meanChunkOnsets = 0;
};

// The weight of each chunk at a beat given the counts of the chunk before it: 1000 a learned count
// plus 100, scaled by 1 + 0.6 * (energy / 100 - 0.5) * (onsets in the chunk - mean onsets) and
// floored at 0.1 of the weight.
std::vector<int> chunkWeights(const RhythmModel& model, std::size_t beat,
                              const std::vector<int>& counts, int energy);

RhythmModel buildRhythmModel(const Bank& bank, Family family, RhythmKind kind);
// A cell of the given bars made of 1-bar slices, each drawn as four beat chunks: beat 0 from the
// starts, then each beat from the bigram row of the chunk before. A learned count weighs 1000 and
// any chunk seen at that beat gets 100 more. The weight is scaled by 1 + 0.6 * (energy / 100 -
// 0.5) * (onsets in the chunk - mean onsets), floored at 0.1. A slice is kept when its onset count
// is within the learned range with the maximum widened by 1 and is at least 1, it keeps the least
// gap, a chord slice has an onset in steps 0 to 7 and, after the first, a slice differs from the
// one before it. A melody cell needs 2 onsets in all. Lengths are clamped to the next onset and
// the end of the cell. Gives up after 24 tries.
std::optional<RhythmCell> inventRhythm(const RhythmModel& model, int bars, int energy, Pcg32& rng);
// Above kHighEnergy callers move some voicings up one inversion (slot 7): a chord that repeats the
// previous span's symbol and reaches bar 3 or 4 of a 4-bar group (the voicing is one per span, so
// it moves for the whole span), when the new voicing keeps its top note in the entry's range and
// every voice within kMaxVoiceMove of both neighbours. Never for a parallel entry. Voicings must
// already be set.
void invertVoicings(const StyleEntry& entry, std::vector<PlacedChord>& chords,
                    std::uint64_t chordSeed);

// Where a lane's material came from: invented by a model, written by the share that keeps the
// bank's own, or written because the invention gave up.
enum class Source : std::uint8_t
{
    invented,
    written,
    fallback,
};

// The energy rhythms are invented at: the middle of its tier (15 below kMidEnergy, 45 up to
// kHighEnergy, 80 above), so a knob move within a tier keeps the rhythm and only a tier change
// re-invents it, as with the written cells and the variety tiers.
int inventEnergy(int energy);

// One draw in kWrittenShare takes the written material as is.
constexpr std::uint32_t kWrittenShare = 5;

// The models of every family, built from a bank's entries.
struct Models
{
    static constexpr std::size_t kFamilyCount = 5;
    std::array<std::array<ProgressionModel, 3>, kFamilyCount> progressions; // [family][mode group]
    std::array<std::array<RhythmModel, 2>, kFamilyCount> rhythms;           // [family][kind]

    [[nodiscard]] const ProgressionModel& progression(const StyleEntry& entry) const;
    [[nodiscard]] const RhythmModel& rhythm(Family family, RhythmKind kind) const;
};
Models buildModels(const Bank& bank);

// An invented cell whose hit steps differ from every one of the others, or none when inventRhythm
// gives up or kTries cells in a row match one.
std::optional<RhythmCell> inventOther(const RhythmModel& model, int bars, int energy,
                                      const std::vector<std::vector<Hit>>& others, Pcg32& rng);

// The chords lane's progression, drawn from Pcg32(chordSeed, kChordProgression). Without models it
// is a written one, picked by weight. With models one draw in kWrittenShare picks a written one,
// else it is invented in the settings' key, or picked written when the invention gives up.
struct ChosenProgression
{
    Progression progression;
    Source source = Source::written;
};
ChosenProgression chooseProgression(const Models* models, const StyleEntry& entry,
                                    const Settings& settings, std::uint64_t chordSeed);

// A chord cell and the bar-1 hits each turnaround bar of the idea swaps in, in bar order.
struct ChordRhythm
{
    RhythmCell cell;
    std::vector<std::vector<Hit>> turnarounds; // empty when the entry has no other cell
    Source source = Source::written;
};
// The cell is drawn from Pcg32(chordSeed, kChordCell) and the turnarounds from kChordTurnaround.
// Without models the cell is one of the entry's and each turnaround is bar 1 of another of its
// cells. With models one draw in kWrittenShare takes that written rhythm, else the cell is a 1-bar
// invention at inventEnergy and each turnaround another that differs from it; when either
// invention gives up the rhythm falls back to written, the turnarounds drawn from the entry's
// cells.
ChordRhythm chooseChordRhythm(const Models* models, const StyleEntry& entry,
                              const Settings& settings, std::uint64_t chordSeed);

// From kMidEnergy up a turnaround bar swaps in its turnaround hits (when there are any). Above
// kHighEnergy the breath bar drops its last hit, hits of 2 sixteenths or fewer play 70, 85 or 100
// percent of their length, at least 120 ticks (slot 6), and ghost notes are added (slot 5): at most
// one a bar and bars / 2 in all, never in a turnaround bar, each the bar's voicing on an odd 16th,
// 1 sixteenth long at kGhostVelocity.
std::vector<NoteEvent> chordEvents(const std::vector<PlacedChord>& chords,
                                   const ChordRhythm& rhythm, const Settings& settings,
                                   std::uint64_t chordSeed);

// The entry's allowed patterns run sparse to busy and the energy picks along them (slot 1).
BassPattern bassPatternFor(const StyleEntry& entry, int energy, Pcg32& rng);
// Pitch variation draws from Pcg32(bassSeed, kBassPitch), approach notes from kBassApproach, the
// riff motif from kBassRiff and the call-and-response picks from kBassCall. The call-and-response
// pattern reads the chords lane, which must already be in the idea.
// Above kHighEnergy the last bar's steps 8 to 15 become a fill (slot 7) of 3 or 4 sixteenths on
// steps 9, 10, 11, 13 or 14, walking scale tones to the first bass note, before the approach note.
// Velocity is a percent of the lane base: 100, or 85 for approach notes.
std::vector<NoteEvent> bassEvents(const Idea& idea, BassPattern pattern, std::uint64_t bassSeed);

// 1 to 3 of the allowed tones nearest the centre, cycled by the pluck.
// Throws std::logic_error when no tone is allowed.
std::vector<int> pluckPitches(const std::vector<int>& allowedPcs, Pcg32& rng);

// The melody's cells: a plays every 2-bar block but the last of each 8-bar phrase, which plays the
// answer from kMidEnergy up when answers is set, else b.
struct MelodyRhythm
{
    RhythmCell a;
    RhythmCell b;
    RhythmCell answer;
    bool answers = false;
    Source source = Source::written;
};
// Cells a, b and the answer are drawn from Pcg32(melodySeed, kMelodyCellA), kMelodyCellB and
// kMelodyAnswerCell. Without models they are the entry's cells with the hit count nearest
// 3 + 7 * energy / 100: b other than a, and the answer other than a, and other than b too when
// the entry has a third (no answer for an entry of one cell). With models one draw in
// kWrittenShare takes that written rhythm, else each is a 2-bar invention at inventEnergy, b
// differing from a and the answer from both; when any invention gives up the rhythm falls back to
// written.
MelodyRhythm chooseMelodyRhythm(const Models* models, const StyleEntry& entry,
                                const Settings& settings, std::uint64_t melodySeed);
std::vector<NoteEvent> melodyEvents(const StyleEntry& entry, const Idea& idea,
                                    const MelodyRhythm& cells, std::uint64_t melodySeed);

// Which lanes take material invented by the bank's models; the others take the entry's written
// material as generate always did.
struct Invent
{
    bool chords = true;
    bool melody = true;
};
// The idea generateWith wrote fails validate; the message lists the violations.
class InvalidIdea : public std::logic_error
{
  public:
    using std::logic_error::logic_error;
};

// generate with each lane's material as given. Throws InvalidIdea when the idea fails validate.
// generate tries everything invented, then a written melody, then all written, falling back only
// on InvalidIdea.
Idea generateWith(const Bank& bank, const Settings& settings, std::uint32_t seed,
                  const LaneSeeds& laneSeeds, const Idea& previous, Pins pins, Invent invent);
} // namespace md::detail

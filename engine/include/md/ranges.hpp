#pragma once

namespace md
{
constexpr int kTicksPerQuarter = 960;
constexpr int kTicksPerSixteenth = 240;
constexpr int kTicksPerBar = 3840;
constexpr int kPitchClasses = 12;
constexpr int kStepsPerBar = 16;
// A rhythm cell is at most 2 bars: hit steps 0 to 31, lengths 1 to 32.
constexpr int kMaxStep = 31;
constexpr int kMaxLength = 32;
constexpr int kChordLow = 48;
constexpr int kChordHigh = 79;
constexpr int kChordTopLow = 62;
constexpr int kChordTopHigh = 77;
constexpr int kMudBelow = 52;
constexpr int kMudMinInterval = 5;
constexpr int kNoThirdsBelow = 48;
constexpr int kBassLow = 28;
constexpr int kBassHigh = 48;
constexpr int kMelodyLow = 60;
constexpr int kMelodyHigh = 84;
constexpr int kMaxVoiceMove = 7;
constexpr int kMaxMelodyLeap = 7;
constexpr int kTempoMin = 60;
constexpr int kTempoMax = 200;
// The tempo written into dragged MIDI files when the host reports none.
constexpr int kDefaultTempo = 124;
// The MIDI writer's accepted tempo range, wider than the settings tempo range above.
constexpr int kMidiTempoMin = 20;
constexpr int kMidiTempoMax = 999;
constexpr int kSwingMinPermille = 500;
constexpr int kSwingMaxPermille = 750;
constexpr int kWeightMaxMilli = 10000;
constexpr int kMaxBarsPerChord = 8;
constexpr int kSeedMax = 99999;
} // namespace md

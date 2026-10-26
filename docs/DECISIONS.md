# Decisions

| Date | Decision | Reason |
| --- | --- | --- |
| 2026-10-26 | AU and VST3 both ship. | Owner decision. |
| 2026-10-26 | Public repo under AGPLv3. | Owner decision, so JUCE is free to use. |
| 2026-10-26 | Default key is A minor. | Owner decision. |
| 2026-10-26 | Release installer is a `.dmg` containing the signed, notarized `.pkg`. | Owner decision; the spec had `.pkg` only. |
| 2026-10-26 | JUCE 9.0.4 instead of JUCE 8. | JUCE 9 is the maintained line and 9.0.4 meets the two-week rule; JUCE 8 only receives security patches. |
| 2026-10-26 | Bundle id `com.markstent.magicdust`, codes `Mdus` and `Mdst`. | Fixed identifiers for the plugin. |
| 2026-10-26 | Engine uses integer arithmetic only (weights in thousandths, swing in permille). | Goldens match on every architecture. |
| 2026-10-26 | Idea code layout (Task 5) is 13 characters and does not yet carry pins or rerolled lane seeds. | Revisit in Phase 2 when pins exist. |
| 2026-10-26 | Phase 1 generator simplifications: close-position voicings only, chord rhythm cells used as-is (no energy density scaling), flavour stored but not applied, velocity humanize only (no timing humanize), riff and call-and-response bass not yet generated. | Each is replaced in its later phase. |
| 2026-10-26 | Chord rule: every chord is in key for the chosen mode, or borrowed from one related mode with the same home note (phrygian and aeolian, aeolian and dorian, mixolydian and ionian). A progression borrows from at most one mode. The mode is chosen only among the entry's modes where the whole progression passes; an entry with a progression that passes in none of its modes is rejected when the bank loads. Parallel (chord memory) entries are exempt. | Owner decision. |
| 2026-10-26 | Melody rhythms come from a new bank field `melodyRhythms` (rhythm cells only, no pitches). Energy picks the cell whose hit count per 2 bars is nearest `3 + 7 * energy / 100`. | Owner decision. Fills the gap where the spec says melodies use "a rhythm cell from the bank" but the entry format had no such field. |
| 2026-10-26 | Any Mac DAW, not only Ableton. v1 is tested in Ableton Live, Logic Pro or GarageBand, Reaper, Bitwig, Cubase, Studio One and FL Studio. Interface and docs say "your daw" instead of "ableton". Windows stays out of v1. | Owner decision. |
| 2026-10-26 | Sound source: the owner can hear ideas through the built-in sounds or through their own instruments, live, while sprinkling. The plugin declares MIDI output from Phase 0 and sends chords, bass and melody on MIDI channels 1, 2 and 3. How each DAW receives it is settled by the routing spike (Task 3a) before Phase 2's playback plan. The choice is per lane: each lane plays either its built-in sound or sends MIDI to the owner's instrument, default built-in. The control lives on or beside each lane label; its exact look is designed in Phase 5 and added to the visual spec. | Owner decision. Changing MIDI output later can break saved sessions. |
| 2026-10-26 | Bass patterns grow from 5 to 8: rolling offbeat, offbeat, octave bounce, garage skip, sustained, riff, pedal, call and response. Riff is its own syncopated line from scale tones; pedal holds the home root under changing chords; call and response places notes in the gaps between chord hits. The visual spec's bass encoder list grows to match. | Owner decision. |
| 2026-10-26 | Chord names use the fixed spelling C C# D Eb E F F# G Ab A Bb B. | One spelling everywhere keeps output and tests predictable. |
| 2026-10-26 | "pull back" is the feel word for a degree-5 chord in the last slot of a progression of 4 or more chords; all other feel words come from degree. | Keeps feel words consistent with how progressions resolve. |
| 2026-10-26 | Settings groove sets swing; the bank entry's `swing` is stored but unused until Phase 2 decides how they combine. | Defers the combination rule until playback exists. |
| 2026-10-26 | clang-tidy check `cppcoreguidelines-owning-memory` is disabled. | JUCE requires `createEditor` and `createPluginFilter` to return raw owning pointers, so the check fires on every plugin. |

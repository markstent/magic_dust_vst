# Phase 2: Chords, End to End Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The owner loads Magic Dust in their daw, sprinkles ideas, hears the chords on a built-in organ in time with the daw, turns flavour, aura and groove for variety, drags chords and lanes into clips, and sends any lane to their own instrument through a second Magic Dust instance in sender mode.

**Architecture:** The pure C++ engine gains flavour and the four voicing shapes. A JUCE-free playback core in `plugin/` turns an idea into a sorted event list and schedules it against host or internal time. The processor generates on the message thread, publishes immutable patterns through an atomic pointer held by a process-wide link, renders the chords lane on an organ voice and sends all three lanes as MIDI. Further instances in the same process become senders that play one lane from the main instance's pattern.

**Tech Stack:** C++20, JUCE 9.0.4, Catch2 v3, CMake presets (dev, release, asan, tsan, lint).

**Spec:** `docs/spec/magic-dust-spec.md` (sections 4, 6, 8 Phase 2, 9) and `docs/spec/magic-dust-visual-spec.md` (Layout, tokens, typography, materials, components 4 to 9, 13, 14). Owner decisions after the spec are in `docs/DECISIONS.md` (2026-10-27 rows: Phase 1 approval, routing spike, sender mode).

## Global Constraints

- Never use the em dash character in code, comments, docs or interface text; use a plain hyphen.
- Commit messages never name an AI agent as co-author.
- Never hand-edit `CHANGELOG.md` or files marked auto-generated.
- In long Markdown, one sentence per physical line.
- Interface and docs say "your daw", never a specific DAW, except in per-DAW setup steps.
- All interface text is lowercase except chord symbols such as `Am9`.
- Our code compiles with `-Wall -Wextra -Wpedantic -Werror`; clang-format and clang-tidy (LLVM 23, `.llvm-version`) are clean.
- Musical ranges and limits live only in `engine/include/md/ranges.hpp`; colours, sizes and timings only in `ui/Tokens.h`, with the visual spec's token names.
- The engine never reads the clock, never calls `std::rand`, and never lets hash-map order affect a choice; same seed, settings and bank give byte-identical MIDI.
- The audio thread never allocates, locks, blocks or touches files.
- Generation runs on the message thread and stays under 20 ms at p99 (release build).
- Every change ships with tests, written first; a golden test change runs `scripts/update-golden.sh` and waits for the owner's listening approval before the new goldens are committed as approved.
- MIDI channels: chords 1, bass 2, melody 3 (0-based 0, 1, 2 in code).
- Ticks: 960 per quarter note; ideas loop from bar 1.
- Editor logical size 1040 x 740; every position and size comes from the visual spec's Layout tables.
- Every departure from the spec gets a row in `docs/DECISIONS.md` in the task that makes it.

## Rulings made while planning

These are recorded in `docs/DECISIONS.md` by the task named, so the owner can overturn them.

1. Flavour `simple` keeps the bank's written chord qualities (Task 1). The spec says simple is "triads and 7ths", but the owner approved the current as-written sound as simple, and this keeps every existing golden at simple.
2. Soulful's passing sus4 and jazzy's altered last dominant are left out of Phase 2 (Task 1). Each needs a new placement or validation rule; add them if the flavour listening review asks.
3. The entry's `voicing.style` picks the voicing shape; the cost picks the rotation and octave within it; close position is the fallback when the shape has no fitting candidate (Task 2).
4. Chord bubbles show the first four placed chords; with fewer, the rest are hidden (Task 8).
5. While at least one sender sends a lane, the main instance's built-in sound for that lane is silent (Task 6). The explicit per-lane source control arrives in Phase 5 around this rule.
6. A new instance is the main instance when no main instance exists in the process; otherwise it is a sender. Saved state restores the saved mode. Senders follow the oldest main instance. A sender has a "full magic dust" key that turns it into an independent main instance, which keeps the spec's "two instances stay independent" check possible (Task 7).
7. Senders play only while the host transport plays; the main instance's internal play key does not drive senders (Task 7). Upgrade path: publish the internal clock's position through the link.
8. The volume knob, wordmark, grille, encoders, pin, reroll, mute, swap, keep, bottle it and the spells panel stay in Phase 5. The aura pad and the groove and flavour fields come forward into Phase 2 because the owner asked for a way to add variety.

## Review Focus

1. Host tempo changes while playing, and loop wrap at the end of the idea: notes must land on the right sample and no note may hang. Pinned by Task 4's tests.
2. Stopping the transport, or a new pattern arriving mid-note: every sounding note gets its note-off. Pinned by Task 4's and Task 6's tests.
3. Removing the main instance while senders are playing: senders go silent with no crash or use-after-free. Pinned by Task 7's test under asan.
4. A generation failure (an exception from the bank or `generate`): the previous idea stays, nothing crashes, and the screen says so. Pinned by Task 6's test.
5. Reopening a saved set: the idea, mode and sender lane come back exactly. Pinned by Task 6's and Task 7's state round-trip tests.

---

### Task 1: Flavour

**Files:**
- Modify: `engine/include/md/types.hpp` (add `Quality::dom13`), `engine/src/theory.cpp` (intervals and name for dom13), `engine/src/generate.cpp` (apply flavour after the progression is chosen and before voicing), `engine/src/internal.hpp` (declare `applyFlavour`), `tools/md-cli/main.cpp` (help text), `docs/TESTING.md`, `docs/DECISIONS.md`
- Test: `tests/engine/test_theory.cpp`, `tests/engine/test_generate.cpp`

**Interfaces:**
- Produces: `ChordSymbol md::detail::applyFlavour(const Key& key, std::optional<Mode> borrowedFrom, const ChordSymbol& chord, Flavour flavour)`.

Rules:
- Ladders: minor `min -> min7 -> min9 -> min11`; major `maj -> maj7 -> maj9`, where a major triad whose `maj7` would leave the allowed scale tries `dom7` instead; dominant `dom7 -> dom9 -> dom13`. `sus4`, `six` and `add9` never change.
- `simple` returns the chord unchanged. `soulful` climbs one step. `jazzy` climbs two steps.
- A step is taken only when every pitch class of the new quality is in the key scale or the borrowed scale (when `borrowedFrom` is set); otherwise climbing stops.
- `dom13` intervals: `{0, 4, 7, 10, 14, 21}`; name suffix `13` (for example `E13`).

- [ ] **Step 1: Write the failing tests** in `tests/engine/test_theory.cpp`, key A aeolian unless noted: `applyFlavour(i7, simple) == i7`; `applyFlavour(i7, soulful) == i (min9)`; `applyFlavour(i7, jazzy) == i (min11)`; `applyFlavour(VII maj, soulful) == VII dom7` (Gmaj7's F# is out of key); `applyFlavour(VII maj, jazzy) == VII dom9`; `applyFlavour(v min7, soulful) == v min7` (Em9's F# is out of key); `applyFlavour(sus4 chord, jazzy)` unchanged; `chordName({9, aeolian}, {5, dom13}) == "E13"`; `chordPitchClasses` of `dom13` on E is `{1, 2, 4, 6, 8, 11}` sorted.
- [ ] **Step 2: Run** `ctest --preset dev -R theory` and see the new tests fail to compile or fail.
- [ ] **Step 3: Implement** `Quality::dom13` and `applyFlavour`; call it in `generate.cpp` on every placed chord after mode and borrowing are chosen, so `validate()` sees the flavoured symbols. Update `names.cpp` only if it lists qualities.
- [ ] **Step 4: Add generate tests** in `tests/engine/test_generate.cpp`: for every style and seeds 0 to 49, `generate` with `simple` gives byte-identical MIDI to the Phase 1 output (the goldens cover this; assert one style here with `midiexport` bytes before and after setting flavour explicitly to simple); with `jazzy`, at least one idea across those seeds has a chord whose quality differs from simple, and every idea passes `validate()`.
- [ ] **Step 5: Run** `ctest --preset dev` and `ctest --preset dev -R golden`; all pass and the goldens are unchanged.
- [ ] **Step 6: Docs.** Remove "(no effect until Phase 2)" from the md-cli help and the flavour sentence in `docs/TESTING.md`; add one TESTING.md line with an example `--flavour jazzy` command. Add DECISIONS rows for rulings 1 and 2.
- [ ] **Step 7: Commit** `feat(engine): flavour climbs the chord ladders within the key`.

### Task 2: Voicing shapes

**Files:**
- Modify: `engine/src/chords.cpp`, `engine/src/validate.cpp` (only if a shape needs a rule change; none is expected), `docs/DECISIONS.md`
- Test: `tests/engine/test_generate.cpp`, `tests/golden/*.mid` (regenerated)

**Interfaces:**
- Consumes: `StyleEntry::voicingStyle` (`VoicingStyle::close`, `drop2`, `organStab`, `pianoSpread`).
- Produces: no new public names; `voiceChords` uses the entry's shape.

Shapes, each built from the existing close-position stacks (the 4-note or doubled-triad candidates in `candidates`), then filtered by `fits`:
- `close`: unchanged.
- `drop2`: the second-highest note of each 4-note close stack moved down an octave.
- `organStab`: each close stack with its lowest note also doubled an octave up when that stays within range (5 notes); otherwise the close stack.
- `pianoSpread`: the chord root as the lowest note, then the close stack of the remaining tones placed at least a perfect 5th above it.
- When a shape yields no fitting candidate, fall back to close position (and its wide rescue), as today.
- The cost function and tie-breaks stay as they are; the spec's weights (movement 1.0, top-note 0.5) are already the 2:1 ratio in `cost`.

- [ ] **Step 1: Write the failing tests:** for every bundled entry, seeds 0 to 99, each idea passes `validate()`; for an entry with `drop2`, every chord voicing's spread (top minus bottom) is greater than the spread of the close voicing for the same chord at seed 0; for `organStab` ideas, at least one voicing has 5 notes; for `pianoSpread`, at least 3 in 4 voicings across seeds 0 to 99 have the chord root as their lowest pitch class (the rest are close-position fallbacks). A shape no bundled entry uses is tested through an entry built in the test with `makeBank`.
- [ ] **Step 2: Run** `ctest --preset dev -R generate` and see them fail.
- [ ] **Step 3: Implement** the shapes in `chords.cpp`, keeping `candidates` as the single source of stacks.
- [ ] **Step 4: Run** `ctest --preset dev`; everything except the golden tests passes; then `MD_PROPERTY_SEEDS=10000 ctest --preset release -R properties` passes.
- [ ] **Step 5: Run** `scripts/update-golden.sh`; it writes before and after files for the owner. Commit the new goldens in their own commit, `test: regenerate golden MIDI files for voicing shapes`, and note in the ledger that owner approval is pending at the phase gate.
- [ ] **Step 6:** DECISIONS row for ruling 3 and the exact shape definitions above.
- [ ] **Step 7: Commit** `feat(engine): voice chords in the entry's shape`.

### Task 3: Drag files

**Files:**
- Modify: `midiexport/include/md/midifile.hpp`, `midiexport/src/midifile.cpp`
- Test: `tests/midiexport/test_midifile.cpp`

**Interfaces:**
- Produces:
  - `MidiTrack chordSlotTrack(const Idea& idea, std::size_t slot)`: the chord lane notes that start inside placed chord `slot`, shifted so the chord starts at tick 0, clipped to the chord's length, channel 0, track name `Chord`.
  - `std::string dragFileName(const Idea& idea, std::optional<LaneId> lane, std::optional<std::size_t> slot, int tempoBpm)`: `Chords - Am9 D9 Fmaj7 Em9 - 124bpm.mid`, `Bass - Am9 D9 Fmaj7 Em9 - 124bpm.mid`, `Melody - ... .mid`, a single chord `Chord - Am9 - 124bpm.mid`, the whole idea `Magic Dust - Am9 D9 Fmaj7 Em9 - 124bpm.mid`. Chord names are the first four placed chords, space separated, plain hyphens.

- [ ] **Step 1: Write the failing tests:** `chordSlotTrack` on the golden case 0 idea returns notes all within `[0, bars * 3840)` of that chord, with the same pitches and lengths as the lane's notes in that span; slot out of range throws `std::out_of_range`; `dragFileName` returns each of the five exact strings above for a fixed idea (assert the real chord names of that idea).
- [ ] **Step 2: Run** `ctest --preset dev -R midifile`; fail.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** `ctest --preset dev`; pass.
- [ ] **Step 5: Commit** `feat(midiexport): single-chord tracks and drag file names`.

### Task 4: Playback core

A JUCE-free scheduler, so preview timing is tested in plain unit tests.

**Files:**
- Create: `plugin/Playback.h`, `plugin/Playback.cpp` (no JUCE includes), add to `md_plugin_core`
- Test: `tests/plugin/test_playback.cpp` (add to `md_plugin_tests`)

**Interfaces:**
- Produces:
  - `struct PlayEvent { int tick; std::uint8_t channel; std::uint8_t note; std::uint8_t velocity; bool on; };` (velocity 0 for off)
  - `struct PlaybackPattern { std::vector<PlayEvent> events; int lengthTicks; };` sorted by tick, offs before ons at the same tick, built by `PlaybackPattern makePattern(const md::Idea& idea)` from all three lanes with channels 0, 1, 2. Muted lanes are left out.
  - `struct ScheduledEvent { int sampleOffset; PlayEvent event; };`
  - `class SoundingNotes` (fixed `std::array<std::bitset<128>, 16>`): `void apply(const PlayEvent&)`, `template <class F> void releaseAll(F&& emitOff)`.
  - `template <class F> void schedule(const PlaybackPattern& p, double startPpq, double bpm, double sampleRate, int numSamples, F&& emit)`: calls `emit(ScheduledEvent)` for every event whose looped tick falls in the block, in order, with `sampleOffset = floor((eventPpq - startPpq) * 60 / bpm * sampleRate)` clamped to `[0, numSamples - 1]`. The idea loops every `lengthTicks`; `startPpq` may be any non-negative value. Never allocates.
  - `class InternalClock`: `void start()`, `void stop()`, `bool running() const`, `double ppq() const`, `void advance(int numSamples, double bpm, double sampleRate)`; starts from ppq 0.

- [ ] **Step 1: Write the failing tests:**
  - a one-note pattern at tick 480 (an 8th) schedules at sample 11025 for `startPpq 0`, 120 bpm, 44100 Hz, with blocks of 32, 64, 512 and 2048 samples cut consecutively, and also at 48000 and 96000 Hz (24000 and 48000 samples); exactly one note-on in total and within 1 sample of the expected offset;
  - loop wrap: with `lengthTicks = 8 * 3840`, a block starting at ppq 31.9 and ending at 32.1 schedules the note-offs before bar 9 then the tick-0 events, in order;
  - a tempo change between blocks (120 then 128 bpm) keeps each event at the position computed from that block's tempo;
  - `SoundingNotes` after on(60), on(64), off(60) releases exactly 64 on its channel;
  - `makePattern` sorts offs before ons at the same tick and skips a muted lane;
  - `InternalClock` advanced by 44100 samples at 120 bpm is at ppq 2.0.
- [ ] **Step 2: Run** `ctest --preset dev -R playback`; fail.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** `ctest --preset dev`; pass.
- [ ] **Step 5: Commit** `feat(plugin): playback pattern and sample-accurate scheduling`.

### Task 5: Organ voice

**Files:**
- Create: `plugin/Organ.h`, `plugin/Organ.cpp` (add to `md_plugin_core`); constants in `engine/include/md/ranges.hpp` only if musical, otherwise at the top of `Organ.cpp`
- Test: `tests/plugin/test_organ.cpp`

**Interfaces:**
- Produces: `class Organ` wrapping `juce::Synthesiser` with 8 voices: `void prepare(double sampleRate)`, `void noteOn(int note, int velocity)`, `void noteOff(int note)`, `void allNotesOff()`, `void render(juce::AudioBuffer<float>&, int start, int num)` (adds into the buffer), `void setGain(float)` (from the volume parameter, 0 to 1, default 0.8).

Sound: sine partials at harmonics 1, 2, 3, 4 with amplitudes 1, 0.5, 0.25, 0.125 (partials at or above Nyquist are skipped), linear attack 5 ms, linear release 60 ms, per-voice gain `0.2 * velocity / 127 * gain`, same signal on both channels.

- [ ] **Step 1: Write the failing tests:** with no notes, `render` leaves a zeroed buffer exactly zero; one note 69 at velocity 127 and gain 1 at 48000 Hz gives a steady-state RMS (samples 2400 to 7200) of `0.2 * sqrt((1 + 0.25 + 0.0625 + 0.015625) / 2)` = 0.16298 within 2%; after `noteOff` and 60 ms the output is exactly zero; gain 0.5 halves the RMS within 2%; `allNotesOff` silences within 60 ms.
- [ ] **Step 2: Run** `ctest --preset dev -R organ`; fail.
- [ ] **Step 3: Implement** with a phase accumulator per voice (no `std::sin` table needed; `std::sin` is fine, it does not allocate).
- [ ] **Step 4: Run** `ctest --preset dev`; pass.
- [ ] **Step 5: Commit** `feat(plugin): organ audition voice`.

### Task 6: Processor

**Files:**
- Modify: `plugin/PluginProcessor.h`, `plugin/PluginProcessor.cpp`, `.github/workflows/ci.yml`, `docs/DECISIONS.md`
- Create: `plugin/Link.h`, `plugin/Link.cpp` (process-wide pattern publishing; Task 7 extends it with senders)
- Test: `tests/plugin/test_processor.cpp`, `tests/plugin/AllocationGuard.h`, `tests/plugin/AllocationGuard.cpp`

**Interfaces:**
- Consumes: `makePattern`, `schedule`, `SoundingNotes`, `InternalClock` (Task 4); `Organ` (Task 5); `md::loadBundledBank`, `md::generate`, `md::LaneSeeds`.
- Produces:
  - Parameters in a `juce::AudioProcessorValueTreeState` named `params`: `mood` (int 0 to 100, default 30), `energy` (int 0 to 100, default 50), `groove` (choice: straight, swing 54%, uk shuffle 58%, heavy 62%; default straight), `flavour` (choice: simple, soulful, jazzy; default simple), `volume` (float 0 to 1, default 0.8).
  - Message-thread API used by the editor: `const md::Idea* idea() const`, `std::optional<std::string> lastError() const`, `void sprinkle()` (new seed from `juce::Random` on the message thread, regenerate), `void setStyle(md::Style)`, `void setKeyRoot(int)`, `void play()`, `void stop()`, `bool previewPlaying() const`, `double previewPpq() const` (atomic, written by the audio thread), `std::optional<double> hostBpm() const`, `void auditionChord(std::size_t slot)`.
  - `class Link` (singleton via `Link& Link::instance()`): `void publish(const void* owner, std::shared_ptr<const PlaybackPattern>)`, `const PlaybackPattern* current(const void* owner) const` (audio-thread safe: an atomic raw pointer per owner slot, fixed capacity 16 owners), `void retire()` (message thread: frees patterns retired more than 1 s ago), `void remove(const void* owner)`.

Behaviour:
- Generation runs on the message thread only; a 30 Hz `juce::Timer` compares the parameter values with the last generated settings. Groove changes regenerate at once; mood, energy and flavour regenerate once the values have not changed for 150 ms. The seed and lane seeds stay.
- A generation exception keeps the previous idea and sets `lastError()` to "couldn't write that idea"; a bank load failure leaves no idea and sets "the style bank failed to load".
- The audio thread: clears the buffer, reads the host position; when the host plays, schedules from the host ppq; when stopped and the internal clock runs, schedules from it and advances it; on a transport stop, a stop key, or a new pattern pointer, emits note-offs for every sounding note first. It writes every event to the MIDI output on its channel, and plays chords-lane events (channel 0) on the organ unless `Link::sendersFor(chords) > 0` (Task 7; until then always plays). Audition events are queued from the message thread through a single-producer atomic slot and played as one beat of the chord's voicing.
- `isBusesLayoutSupported`: stereo output only, no input.
- State (`getStateInformation`/`setStateInformation`): `schemaVersion` 1, `seed`, `style`, `keyRoot`, `bars`, the three lane seeds as decimal strings, `mode` (`main`; Task 7 adds `sender`), and the APVTS state. Missing fields take the defaults (a Phase 0 state has none).
- CI: the concurrency group becomes `ci-${{ github.head_ref || github.ref_name }}` so a push and its PR share one group.

- [ ] **Step 1: Write the allocation guard:** `AllocationGuard.cpp` replaces global `operator new`/`delete` (all forms) for `md_plugin_tests`; a `thread_local bool` armed by `struct ScopedNoAllocation` makes any allocation call `std::abort()` after printing "allocation on the audio thread". One test proves the guard fires (death is hard in Catch2; instead count allocations in a `thread_local int` and assert the count, and have `ScopedNoAllocation`'s destructor `CHECK(count == 0)`).
- [ ] **Step 2: Write the failing processor tests:**
  - after construction the idea exists and equals `generate(bank, defaults, seed, defaultLaneSeeds(seed))` for the processor's seed;
  - with a fake `juce::AudioPlayHead` playing at 120 bpm from ppq 0, 2 seconds of 512-sample blocks at 48 kHz produce MIDI on channels 1, 2 and 3 matching the idea's events in that span (count and order) and non-silent audio; this runs inside `ScopedNoAllocation` with a pre-sized `MidiBuffer`;
  - stopping the fake transport mid-note emits a note-off for every sounding note in the next block;
  - with the transport stopped, `play()` runs the internal clock and produces the same first-bar events; `stop()` releases them;
  - setting `flavour` to jazzy and pumping the message loop past 150 ms changes the idea's chord qualities but keeps its seed; setting `groove` regenerates on the next timer tick;
  - a state round trip (`getStateInformation` then a fresh processor's `setStateInformation`) restores seed, style, key, lane seeds and parameters, and the regenerated idea's MIDI bytes are identical;
  - `setStateInformation` with garbage bytes and with an empty Phase 0 state leaves a valid default idea;
  - `isBusesLayoutSupported` accepts stereo out and rejects mono out and any input;
  - a forced generation failure (a style whose entries are removed through a test-only bank injection constructor `MagicDustProcessor(md::Bank)`) keeps the previous idea and sets `lastError()`.
- [ ] **Step 3: Run** `ctest --preset dev -L plugin`; fail.
- [ ] **Step 4: Implement** the processor and `Link`.
- [ ] **Step 5: Run** `ctest --preset dev`, `ctest --preset asan`, and the tsan preset's plugin tests; pass. Run `scripts/validate.sh`; pluginval VST3 and AU and auval pass.
- [ ] **Step 6:** Fix the CI concurrency group; actionlint clean. DECISIONS row for ruling 5.
- [ ] **Step 7: Commit** in two commits: `feat(plugin): generate, play and save ideas` and `ci: one concurrency group per branch for push and pull request`.

### Task 7: Sender mode

**Files:**
- Modify: `plugin/Link.h`, `plugin/Link.cpp`, `plugin/PluginProcessor.h`, `plugin/PluginProcessor.cpp`, `docs/DECISIONS.md`
- Test: `tests/plugin/test_sender.cpp`

**Interfaces:**
- Produces:
  - `enum class InstanceMode { main, sender };`
  - Processor: `InstanceMode mode() const`, `md::LaneId senderLane() const`, `void setSenderLane(md::LaneId)`, `void becomeMain()`, `bool senderLinked() const` (a main instance exists).
  - `Link`: `void registerMain(const void*)`, `const void* oldestMain() const`, `void addSender(md::LaneId)`, `void removeSender(md::LaneId)`, `int sendersFor(md::LaneId) const` (atomic counts).

Behaviour:
- A new instance asks `Link::oldestMain()`; none means it registers as main, otherwise it becomes a sender for the first lane with no sender, or bass when all have one.
- A sender generates nothing. Its audio thread reads `Link::current(oldestMain())`; when the host plays, it schedules that pattern and outputs only its lane's events, on that lane's channel; audio output is silent. When the main instance disappears or the pattern changes, it releases sounding notes first.
- Saved state adds `mode` (`main` or `sender`) and `senderLane`; restoring a sender state makes the instance a sender even if it was created as main, unregistering it.
- `becomeMain()` turns a sender into a main instance that generates its own idea from a new seed.
- Patterns stay alive in `Link` for at least 1 s after retirement or after their owner is removed, so a sender never reads freed memory.

- [ ] **Step 1: Write the failing tests:** the first processor is main and the second is a sender for chords, the third for bass, the fourth for melody; a sender for bass, with a fake playhead playing, outputs exactly the main idea's bass events on channel 2 and nothing else; the main instance's organ goes silent for chords while a chords sender exists and plays again after it is destroyed; destroying the main instance while a sender plays produces note-offs and then silence, with no crash under asan; a sender state round trip restores `sender` and the lane; `becomeMain()` makes the instance main with its own idea and `oldestMain()` unchanged.
- [ ] **Step 2: Run** `ctest --preset dev -R sender`; fail.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** `ctest --preset dev`, `ctest --preset asan`, tsan plugin tests, `scripts/validate.sh`; pass.
- [ ] **Step 5:** DECISIONS rows for rulings 6 and 7.
- [ ] **Step 6: Commit** `feat(plugin): further instances send one lane from the main instance`.

### Task 8: Screen

The main editor's visible state, drawn as vectors per the visual spec.

**Files:**
- Create: `ui/fonts/` (Space Grotesk 500 and 700, Space Mono 400 and 700, from the `google/fonts` repository at a pinned commit, with `OFL.txt`), `ui/Fonts.h`, `ui/Fonts.cpp`, `ui/ScreenComponent.h/.cpp`, `ui/TopLine.h/.cpp`, `ui/ChordBubble.h/.cpp`, `ui/LaneView.h/.cpp`, `ui/AuraPad.h/.cpp`, `ui/StatusFields.h/.cpp`, `ui/Readout.h/.cpp`
- Modify: `ui/Tokens.h` (every colour token from the visual spec, plus the Layout positions used here), `plugin/PluginEditor.h/.cpp`, `plugin/CMakeLists.txt` (fonts as `juce_add_binary_data`, ui sources in `md_plugin_core`), `docs/DECISIONS.md`
- Test: `tests/plugin/test_editor.cpp`

**Interfaces:**
- Consumes: processor API from Tasks 6 and 7.
- Produces: components named as in the visual spec's JUCE component map; `MagicDustEditor` owns the scale transform and a 30 Hz timer that refreshes the screen from the processor.

Scope:
- Body material (fill, brushed lines, highlight, border, radius), screen material (fill, bezel, radius) at the Layout coordinates.
- Top line: spell number (seed) in `gold` (not yet a button), key and style as menu buttons (`juce::PopupMenu`; style items are the six style names; no "manage bank" yet), tempo from the host (or "--- bpm" when unknown).
- Chord bubbles: default, playing (gold ring while the playhead is inside that chord) and selected states; ruling 4.
- Lanes: all three rows, label, piano roll, centre line, bar lines every two bars, notes from the engine's real events, gold playhead while playing, selected state.
- Aura pad: frame, crosshair, sparkle at mood and energy, 8% inset, click or drag sets `mood` and `energy`.
- Status fields: aura quadrant name, groove and flavour values with gold chevrons; click and scroll cycle as in component 8.
- Readout line 1 ("stopped" or "bar 3 of 8"); lines 2 and 3 show "selected: chord 2" and "nothing pinned".
- `lastError()` shows in the status line area in `ink-status`.
- Resizable 80% to 150% with a fixed aspect ratio.

- [ ] **Step 1: Write the failing tests:** the editor opens at 1040 x 740; each component's bounds equal the Layout table (bubbles at x 278, 407, 537, 666, y 82, 92 x 92; lane rows at y 198, 264, 330, 492 x 62; aura pad at 272, 432, 70 x 70); an idea with three placed chords shows three visible bubbles; the bubble accessible titles read "chord 1, Am7, home"; a lane's note rectangles map a note at tick 0 of length one bar to x 344 and width 414 / 8; clicking the flavour field cycles simple, soulful, jazzy, simple and sets the parameter; dragging on the aura pad to its top-right corner sets mood and energy to the values at the 8% inset; opening and closing the editor 20 times leaks nothing (JUCE leak detector clean).
- [ ] **Step 2: Run** `ctest --preset dev -R editor`; fail.
- [ ] **Step 3: Implement.** Keep every number in `ui/Tokens.h`.
- [ ] **Step 4: Run** `ctest --preset dev`; pass. Render the editor offscreen at 100% and 150% to PNGs in the SDD workspace and compare side by side with `docs/spec/visual/magic-dust-ui.png` for the parts built; fix any misalignment, clipped text or uneven spacing.
- [ ] **Step 5:** DECISIONS row for ruling 4 and the font source commit.
- [ ] **Step 6: Commit** `feat(ui): screen with chord bubbles, lanes, aura pad and status fields`.

### Task 9: Keys, audition and drag

**Files:**
- Create: `ui/KeyButton.h/.cpp`, `ui/SprinkleKey.h/.cpp`, `plugin/DragFiles.h/.cpp`
- Modify: `ui/ChordBubble.cpp`, `ui/LaneView.cpp`, `plugin/PluginEditor.cpp`, `ui/Tokens.h`
- Test: `tests/plugin/test_editor.cpp`, `tests/plugin/test_dragfiles.cpp`

**Interfaces:**
- Consumes: `chordSlotTrack`, `laneTrack`, `writeMidiFile`, `dragFileName` (Task 3); processor API.
- Produces: `std::optional<juce::File> writeDragFile(const md::Idea&, std::optional<md::LaneId>, std::optional<std::size_t> slot, int tempoBpm)` in `~/Library/Caches/Magic Dust/drag/` (returns nothing on a write failure); `void cleanDragFolder()` removes files older than 24 hours, called once at editor creation.

Scope:
- Play (LED gold while previewing), stop and sprinkle keys at the visual spec positions with the keycap and sprinkle-key materials and the 80 ms pressed motion.
- Bubble click selects and auditions the chord; a lane click selects it.
- Bubble or lane drag after more than 4 px starts `performExternalDragDropOfFiles` with the written file; a write failure shows "couldn't write the midi file" in the status line.
- Keyboard: Tab reaches top line, bubbles, lanes, aura pad, fields, then keys; Space or Enter activates; every control has an accessible title.

- [ ] **Step 1: Write the failing tests:** `writeDragFile` writes the exact bytes of `writeMidiFile` for that track under the expected name; with an unwritable folder it returns nothing; `cleanDragFolder` deletes a file dated 25 hours ago and keeps one dated 1 hour ago; clicking sprinkle changes the seed and the bubbles; clicking play then stop toggles `previewPlaying()`; the key positions equal the Layout table (play x 27, stop 121, sprinkle 227 at 176 wide, y 575); Tab order matches the list above.
- [ ] **Step 2: Run** `ctest --preset dev -R "editor|dragfiles"`; fail.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** `ctest --preset dev`, `ctest --preset asan`, `scripts/validate.sh`; pass. Render offscreen again and review the keys row against the reference.
- [ ] **Step 5: Commit** `feat(ui): play, stop and sprinkle keys, chord audition and drag to your daw`.

### Task 10: Sender screen and owner guide

**Files:**
- Create: `ui/SenderEditor.h/.cpp`
- Modify: `plugin/PluginProcessor.cpp` (`createEditor` picks the editor by mode), `ui/Tokens.h`, `docs/TESTING.md`, `docs/spec/magic-dust-visual-spec.md` (a short "Sender screen" component section), `docs/DECISIONS.md`
- Test: `tests/plugin/test_editor.cpp`

Sender screen, 520 x 240 logical px, body material:
- Title "magic dust sender" in the wordmark style at 27, 27.
- Three keycaps "chords", "bass", "melody" at y 100, 82 px wide, 12 px apart, starting x 27, each with an LED in its lane colour lit on the selected lane.
- A fourth keycap "full magic dust" 24 px to their right that calls `becomeMain()` and swaps the editor for the main editor.
- Status text in `ink-status` at y 200: "sending bass from magic dust" or "no magic dust found - load magic dust on another track first".

- [ ] **Step 1: Write the failing tests:** a sender opens the sender editor at 520 x 240; clicking "melody" sets `senderLane()` to melody and lights its LED; the status text matches `senderLinked()`; "full magic dust" turns the instance main and the editor becomes the main editor.
- [ ] **Step 2: Run** `ctest --preset dev -R editor`; fail.
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** `ctest --preset dev`; pass.
- [ ] **Step 5: Owner guide** in `docs/TESTING.md`, a "Phase 2 in your daw" section: loading Magic Dust and sprinkling; turning flavour, aura and groove; dragging a bubble, a lane, and the md-cli multi-track file (to answer the spec's "bottle it" open question); sending a lane to your own instrument in Ableton Live 12 (load Magic Dust on a second track, pick the lane, then on the instrument track set MIDI From to that track, the second chooser to Magic Dust, Monitor to In; VST3 only in Live); the listening review: 20 ideas per family, rate keep, close or skip, target at least 1 in 3 keep; and the golden before and after files from Task 2.
- [ ] **Step 6:** DECISIONS row for the sender screen design.
- [ ] **Step 7: Commit** `feat(ui): sender screen and the phase 2 owner guide`.

### Phase 2 gate

- [ ] CI green on the branch (lint, test-dev with pluginval and auval, test-release, sanitizers, python).
- [ ] The owner sprinkles 20 ideas per family in their daw, drags favourites into clips, rates each keep, close or skip, and listens to the Task 2 golden changes; results and the golden approval go in `docs/DECISIONS.md`.
- [ ] The owner sends one lane to their own instrument through a sender and confirms it plays in time.

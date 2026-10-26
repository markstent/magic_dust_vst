# Magic Dust Phase 0 and Phase 1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A Magic Dust instrument that loads in Ableton and passes validation (Phase 0), and a tested, deterministic engine plus `md-cli` that writes 8-bar ideas with chords, bass and melody that fit (Phase 1).

**Architecture:** Three pure C++20 static libraries with no JUCE (`md_engine`, `md_bank`, `md_midiexport`), a JUCE 9 plugin shell, and a `md-cli` command-line tool, all built by one CMake project with presets.
The engine uses integer arithmetic only, so output is byte-identical on arm64 and x86_64.
Phase 1 generators are deliberately simple but obey every validation rule; Phases 2 to 4 replace them with the full rules.

**Tech Stack:** C++20, CMake 3.25+, Ninja, JUCE 9.0.4, Catch2 v3.16.1, nlohmann/json v3.12.0, Python 3.12 with uv (schema check), GitHub Actions on macOS.

**Spec:** `docs/spec/magic-dust-spec.md` and `docs/spec/magic-dust-visual-spec.md` (after Task 1; before that, `specs/`).
Executors read the spec sections named in each task.

## Global Constraints

- C++20; our targets compile with `-Wall -Wextra -Wpedantic -Werror`; third-party code excluded.
- Engine code: no floating point in any musical decision, no clock, no `std::rand`, no unordered containers. Compile `md_engine` with `-ffp-contract=off`.
- Ticks: 960 per quarter note, 240 per 16th, 3840 per 4/4 bar.
- Musical ranges live only in `engine/include/md/ranges.hpp`; interface colours only in `ui/Tokens.h`.
- Dependencies pinned to exact tags; only releases at least two weeks old.
- Plugin identity: name "Magic Dust", company "Mark Stent", manufacturer code `Mdus`, plugin code `Mdst`, bundle id `com.markstent.magicdust`, formats AU, VST3, Standalone, instrument with MIDI input.
- Version lives only in the top-level `project()` call: start at `0.1.0`.
- No em dash character (U+2014) anywhere. Plain hyphen.
- Commits: Conventional Commits (`feat:`, `fix:`, `build:`, `ci:`, `docs:`, `test:`), no AI co-author line, never hand-edit `CHANGELOG.md`.
- Markdown: one sentence per line.
- Build dirs `build/<preset>/`; everything under `build/` and every `.venv/` is git-ignored.
- The owner is not a developer: every command they must run is explained in plain words in `docs/TESTING.md`.

## Review Focus

1. Sanitizer or lint builds overwriting the owner's installed plugin: only `dev` and `release` presets copy plugins into `~/Library/Audio/Plug-Ins` (test in Task 2's preset check).
2. Malformed bank JSON (wrong type, unknown key, bad degree, hit past the cell end): the loader throws `md::BankError` naming the entry id and field, never crashes (Task 6 invalid fixtures).
3. A progression whose total bars do not divide the idea length (an 8-bar vamp in a 4-bar idea): the last chord is truncated at the idea end and the idea is still exactly the chosen length (Task 8 test).
4. Swing and chord-end truncation making a note run past the idea end or overlap the next bass note: validation catches it; property tests across every style, aura corner and length prove it never happens (Task 10).
5. A dragged clip shorter than 8 bars because the last note ends early: the end-of-track event sits at exactly the idea length (Task 7 byte test).

---

## Phase 0: Foundations

### Task 1: Repository hygiene and spec copy

**Files:**
- Create: `.gitignore`, `.editorconfig`, `.clang-format`, `.clang-tidy`, `LICENSE`, `docs/DECISIONS.md`, `docs/TESTING.md`
- Move: `specs/magic-dust-spec.md`, `specs/magic-dust-visual-spec.md`, `specs/visual/*` (except `magic-dust-reference copy.html`) to `docs/spec/`, keeping the `visual/` subfolder
- Delete: `specs/` (including the duplicate `magic-dust-reference copy.html`, which is byte-identical)
- Modify: `README.md`

- [ ] **Step 1:** `git mv` the spec files into `docs/spec/`, delete `specs/`, and confirm image links in both spec files still resolve (they use `visual/...`, relative, so they should).
- [ ] **Step 2:** `.gitignore`: `build/`, `.venv/`, `.DS_Store`, `*.xcuserstate`, `.cache/`, `compile_commands.json` at root.
- [ ] **Step 3:** `.clang-format`: `BasedOnStyle: LLVM`, `IndentWidth: 4`, `ColumnLimit: 100`, `BreakBeforeBraces: Allman`, `PointerAlignment: Left`, `SortIncludes: CaseSensitive`.
- [ ] **Step 4:** `.clang-tidy`: `Checks: 'bugprone-*,performance-*,modernize-*,-modernize-use-trailing-return-type,readability-*,-readability-magic-numbers,-readability-identifier-length,-readability-function-cognitive-complexity,cppcoreguidelines-*,-cppcoreguidelines-avoid-magic-numbers,-cppcoreguidelines-pro-bounds-constant-array-index'`, `WarningsAsErrors: '*'`, `HeaderFilterRegex: '(engine|bank|midiexport|plugin|ui|tools)/'`.
- [ ] **Step 5:** `LICENSE`: the GNU AGPLv3 text, fetched with `gh api licenses/agpl-3.0 --jq .body`.
- [ ] **Step 6:** `docs/DECISIONS.md` with a table `Date | Decision | Reason`, seeded with these rows dated 2026-10-26:
  - AU and VST3 both ship (owner).
  - Public repo under AGPLv3 so JUCE is free (owner).
  - Default key A minor (owner).
  - Release installer is a `.dmg` containing the signed, notarized `.pkg` (owner; spec had `.pkg` only).
  - JUCE 9.0.4 instead of JUCE 8: JUCE 9 is the maintained line and 9.0.4 meets the two-week rule; JUCE 8 only receives security patches.
  - Bundle id `com.markstent.magicdust`, codes `Mdus`/`Mdst`.
  - Engine uses integer arithmetic only (weights in thousandths, swing in permille) so goldens match on every architecture.
  - Idea code layout (Task 5) is 13 characters and does not yet carry pins or rerolled lane seeds; revisit in Phase 2 when pins exist.
  - Phase 1 generator simplifications, each replaced in its later phase: close-position voicings only, chord rhythm cells used as-is (no energy density scaling), flavour stored but not applied, velocity humanize only (no timing humanize), riff and call-and-response bass not yet generated.
  - Chord rule (owner): every chord is in key for the chosen mode, or borrowed from one related mode with the same home note (phrygian and aeolian, aeolian and dorian, mixolydian and ionian). A progression borrows from at most one mode. The mode is chosen only among the entry's modes where the whole progression passes; an entry with a progression that passes in none of its modes is rejected when the bank loads. Parallel (chord memory) entries are exempt.
  - Melody rhythms come from a new bank field `melodyRhythms` (rhythm cells only, no pitches), which fills the gap where the spec says melodies use "a rhythm cell from the bank" but the entry format had no such field (owner). Energy picks the cell whose hit count per 2 bars is nearest `3 + 7 * energy / 100`.
  - Any Mac DAW, not only Ableton (owner). v1 is tested in Ableton Live, Logic Pro or GarageBand, Reaper, Bitwig, Cubase, Studio One and FL Studio. Interface and docs say "your daw" instead of "ableton". Windows stays out of v1.
  - Sound source (owner): the owner can hear ideas through the built-in sounds or through their own instruments, live, while sprinkling. The plugin declares MIDI output from Phase 0 (changing it later can break saved sessions) and sends chords, bass and melody on MIDI channels 1, 2 and 3. How each DAW receives it is settled by the routing spike (Task 3a) before Phase 2's playback plan. The choice is per lane (owner): each lane plays either its built-in sound or sends MIDI to the owner's instrument, default built-in. The control lives on or beside each lane label; its exact look is designed in Phase 5 and added to the visual spec.
  - Bass patterns grow from 5 to 8 (owner): rolling offbeat, offbeat, octave bounce, garage skip, sustained, riff, pedal, call and response. Riff is its own syncopated line from scale tones; pedal holds the home root under changing chords; call and response places notes in the gaps between chord hits. The visual spec's bass encoder list grows to match.
  - Chord names use the fixed spelling C C# D Eb E F F# G Ab A Bb B.
  - "pull back" is the feel word for a degree-5 chord in the last slot of a progression of 4 or more chords; all other feel words come from degree.
  - Settings groove sets swing; the bank entry's `swing` is stored but unused until Phase 2 decides how they combine.
- [ ] **Step 7:** `docs/TESTING.md`: plain-words section "Setting up your Mac" listing the tools already installed (Homebrew cmake, ninja, llvm, uv; pluginval in `~/Applications`), then sections to be filled by later tasks ("Building", "Running tests", "Checking the plugin", "Trying it in Ableton").
- [ ] **Step 8:** `README.md`: title "Magic Dust", one sentence from spec section 2, "Work in progress", and a pointer to `docs/spec/`. The product-page README is Phase 7.
- [ ] **Step 9:** Commit: `docs: add spec, decisions log, licence and repo config`.

### Task 2: CMake project, presets and dependencies

**Files:**
- Create: `CMakeLists.txt`, `CMakePresets.json`, `cmake/Dependencies.cmake`, `cmake/Warnings.cmake`, `scripts/check-presets.sh`

**Interfaces:**
- Produces: CMake function `md_target_defaults(<target>)` (warnings as errors, C++20, clang-tidy when `MD_CLANG_TIDY=ON`); options `MD_COPY_PLUGIN` (default OFF), `MD_CLANG_TIDY` (default OFF), `MD_PERF_ASSERT` (default OFF); imported targets `juce::*`, `Catch2::Catch2WithMain`, `nlohmann_json::nlohmann_json`.

- [ ] **Step 1:** Top-level `CMakeLists.txt`: `cmake_minimum_required(VERSION 3.25)`, `project(MagicDust VERSION 0.1.0 LANGUAGES C CXX)`, `CMAKE_CXX_STANDARD 20` required, `CMAKE_EXPORT_COMPILE_COMMANDS ON`, `CMAKE_OSX_DEPLOYMENT_TARGET 11.0` (cache default), `enable_testing()`, include the two cmake files, then `add_subdirectory` for `engine`, `bank`, `midiexport`, `plugin`, `tools/md-cli`, `tests` (later tasks create these; add each line in the task that creates the folder).
- [ ] **Step 2:** `cmake/Dependencies.cmake`: `FetchContent_Declare` with `GIT_TAG` exact: JUCE `9.0.4`, Catch2 `v3.16.1`, nlohmann/json `v3.12.0`, each `GIT_SHALLOW TRUE` and `SYSTEM`. Before pinning JUCE, read its `LICENSE.md` at that tag and confirm it offers AGPLv3; if it does not, stop and report.
- [ ] **Step 3:** `cmake/Warnings.cmake`: `md_target_defaults(target)` adds `-Wall -Wextra -Wpedantic -Werror`, and sets `CXX_CLANG_TIDY` to `/opt/homebrew/opt/llvm/bin/clang-tidy` (overridable cache var `MD_CLANG_TIDY_EXE`) when `MD_CLANG_TIDY` is ON.
- [ ] **Step 4:** `CMakePresets.json` (version 6), all Ninja, binary dir `build/${presetName}`:

| Preset | Build type | Architectures | Extra cache vars |
| --- | --- | --- | --- |
| `dev` | Debug | arm64 | `MD_COPY_PLUGIN=ON` |
| `release` | Release | arm64;x86_64 | `MD_COPY_PLUGIN=ON`, `MD_PERF_ASSERT=ON` |
| `asan` | Debug | arm64 | `CMAKE_CXX_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all` |
| `tsan` | Debug | arm64 | `CMAKE_CXX_FLAGS=-fsanitize=thread` |
| `lint` | Debug | arm64 | `MD_CLANG_TIDY=ON` |

  Matching `buildPresets` and `testPresets` (test presets: `output.outputOnFailure: true`).
- [ ] **Step 5:** `scripts/check-presets.sh`: uses `python3 -c` with `json` to assert `MD_COPY_PLUGIN` is `ON` only in `dev` and `release`; exits 1 otherwise. Register it as a CTest test `presets_copy_guard` in the top-level file.
- [ ] **Step 6:** Run `cmake --preset dev` (it fetches dependencies; first run is slow). Expected: configure succeeds.
- [ ] **Step 7:** Commit: `build: add CMake project, presets and pinned dependencies`.

### Task 3: Empty Magic Dust instrument

**Files:**
- Create: `plugin/CMakeLists.txt`, `plugin/PluginProcessor.h`, `plugin/PluginProcessor.cpp`, `plugin/PluginEditor.h`, `plugin/PluginEditor.cpp`, `ui/Tokens.h`, `tests/CMakeLists.txt`, `tests/plugin/test_processor.cpp`, `scripts/validate.sh`
- Modify: top-level `CMakeLists.txt` (add `plugin`, `tests`), `docs/TESTING.md`

**Interfaces:**
- Produces: class `MagicDustProcessor : juce::AudioProcessor`, class `MagicDustEditor : juce::AudioProcessorEditor` (1040 x 740), header `ui/Tokens.h` with `namespace md::tokens { inline const juce::Colour body { 0xFFDCDDE0 }; }`, JUCE shared-code target `MagicDust` that tests link against, CTest label `plugin`.

- [ ] **Step 1: Write the failing test** `tests/plugin/test_processor.cpp` (Catch2):
  - `TEST_CASE("processor identity")`: `getName() == "Magic Dust"`, `acceptsMidi()`, `producesMidi()`, `hasEditor()`, `getTailLengthSeconds() == 0.0`.
  - `TEST_CASE("processor outputs silence")`: `prepareToPlay(48000, 512)`, stereo buffer of 512 filled with 1.0f, empty `MidiBuffer`, `processBlock`, every sample equals 0.0f.
  - `TEST_CASE("state round trip does not throw")`: `getStateInformation` into a `MemoryBlock`, then `setStateInformation` with it on a fresh processor.
  - Use `juce::ScopedJuceInitialiser_GUI` once per test.
- [ ] **Step 2:** `tests/CMakeLists.txt`: executable `md_plugin_tests` linking `MagicDust` and `Catch2::Catch2WithMain`, registered with `catch_discover_tests(... PROPERTIES LABELS plugin)`. Run `cmake --build --preset dev`. Expected: link fails (no `MagicDust` target).
- [ ] **Step 3:** `plugin/CMakeLists.txt`: `juce_add_plugin(MagicDust ...)` with `PRODUCT_NAME "Magic Dust"`, `COMPANY_NAME "Mark Stent"`, `BUNDLE_ID com.markstent.magicdust`, `PLUGIN_MANUFACTURER_CODE Mdus`, `PLUGIN_CODE Mdst`, `FORMATS AU VST3 Standalone`, `IS_SYNTH TRUE`, `NEEDS_MIDI_INPUT TRUE`, `NEEDS_MIDI_OUTPUT TRUE`, `AU_MAIN_TYPE kAudioUnitType_MusicDevice`, `VST3_CATEGORIES Instrument Generator`, `COPY_PLUGIN_AFTER_BUILD ${MD_COPY_PLUGIN}`. Compile definitions `JUCE_WEB_BROWSER=0`, `JUCE_USE_CURL=0`, `JUCE_VST3_CAN_REPLACE_VST2=0`. Link `juce::juce_audio_utils`, `juce::juce_recommended_config_flags`, `juce::juce_recommended_warning_flags`. Our warning flags and clang-tidy must apply to our sources only, never to JUCE's module sources: build the JUCE modules in a separate static library `md_juce` that the plugin and tests link, and apply `md_target_defaults` to targets holding only our code. Verify with `cmake --build --preset lint` that clang-tidy runs on our files and no JUCE file.
- [ ] **Step 4:** Implement the processor: stereo output bus only, no input bus; `processBlock` clears the buffer; state is an empty `ValueTree("MagicDust")` with property `schemaVersion = 1`, written with `copyXmlToBinary`. Editor: `setSize(1040, 740)`, `paint` fills `md::tokens::body`.
- [ ] **Step 5:** Run `cmake --build --preset dev && ctest --preset dev -L plugin`. Expected: 3 tests pass; the AU and VST3 are copied into `~/Library/Audio/Plug-Ins/Components` and `~/Library/Audio/Plug-Ins/VST3`.
- [ ] **Step 6:** `scripts/validate.sh [build-dir]` (default `build/dev`): runs `~/Applications/pluginval.app/Contents/MacOS/pluginval --strictness-level 10 --validate-in-process --skip-gui-tests --validate <path>` on the built `Magic Dust.vst3` and `Magic Dust.component` under `<build-dir>/plugin/MagicDust_artefacts/<config>/`, then `auval -v aumu Mdst Mdus`. `set -euo pipefail`; exits non-zero if any fails; pluginval path overridable with `PLUGINVAL`. Run it. Expected: all three pass.
- [ ] **Step 7:** Add "Building", "Running tests" and "Checking the plugin" to `docs/TESTING.md`, plain words, each command with one line on what it does.
- [ ] **Step 8:** Commit: `feat: add empty Magic Dust instrument with validation script`.

### Task 3a: Routing spike (throwaway)

A quick experiment to learn how each DAW receives Magic Dust's MIDI. Nothing from it is kept; only the findings are.

- [ ] **Step 1:** On a throwaway branch `spike/midi-routing`, make the Task 3 processor send a one-bar test loop while the host plays: a C3 note on channel 1, an E2 on channel 2, a G4 on channel 3, each on the offbeats, following host position. No tests, no review.
- [ ] **Step 2:** With the owner, in each DAW they have (Ableton Live 12 first, then Reaper, GarageBand or Logic, and any trials installed), record:
  - Can another track receive the plugin's MIDI, and how is it set up?
  - Can that track filter one channel, so three instrument tracks each hear only their lane?
  - Do AU and VST3 behave differently?
  - Does the DAW record the routed MIDI onto the receiving track?
- [ ] **Step 3:** Write the findings as a table in `docs/DECISIONS.md` (DAW, format, receive, channel filter, record, setup steps) and delete the spike branch.
- [ ] **Step 4:** Decide with the owner, from the table, which hosts need a fallback: either three linked Magic Dust instances that each send one lane, or (for Logic) a separate AU MIDI FX variant. Record the decision. It shapes the Phase 2 plan, not this one.

### Task 4: Continuous integration

**Files:**
- Create: `.github/workflows/ci.yml`, `scripts/format-check.sh`

- [ ] **Step 1:** `scripts/format-check.sh`: runs `clang-format --dry-run --Werror` (path from `CLANG_FORMAT`, default `/opt/homebrew/opt/llvm/bin/clang-format`) over every `*.h`, `*.hpp`, `*.cpp` under `engine bank midiexport plugin ui tools tests`. Run it locally. Expected: exit 0.
- [ ] **Step 2:** `.github/workflows/ci.yml`, triggers `push`, `pull_request`, and `schedule: cron '0 3 * * *'`; runner `macos-15`; jobs:
  - `lint`: `brew install llvm ninja`, `scripts/format-check.sh`, `cmake --preset lint && cmake --build --preset lint`.
  - `test-dev`: configure, build, `ctest --preset dev`, install pluginval from its GitHub release, `scripts/validate.sh build/dev`.
  - `test-release`: configure, build, `ctest --preset release`.
  - `sanitizers`: `asan` preset full `ctest`; `tsan` preset `ctest -L plugin`.
  - `python`: `astral-sh/setup-uv` pinned to an exact tag, `uv sync --project tools/bank-builder --locked`, `uv run --project tools/bank-builder ruff check tools` (Task 6 adds the pytest and bank-check steps).
  - Cache `build/*/_deps` keyed on `cmake/Dependencies.cmake`.
  - On `schedule` runs, set `MD_PROPERTY_SEEDS=10000` for the test jobs.
  - Pin every action to a full commit SHA with the version in a comment.
- [ ] **Step 3: STOP and ask the owner** before pushing: the repo `markstent/magic_dust_vst` is private, and private repos get few free macOS CI minutes (macOS counts 10x). Ask whether to make it public now. Do not change visibility without a yes.
- [ ] **Step 4:** Push the branch and watch the run with `gh run watch`. Expected: all jobs green. Fix any red job before continuing.
- [ ] **Step 5:** Commit: `ci: add macOS CI with lint, tests, sanitizers and pluginval`.

### Phase 0 gate

- [ ] CI green, `scripts/validate.sh` passes.
- [ ] The routing spike findings (Task 3a) are recorded.
- [ ] Walk the owner through `docs/TESTING.md` "Trying it in Ableton": Live 12 Settings, Plug-Ins, switch on Audio Units and VST3 system folders, Rescan, create a MIDI track, load "Magic Dust" (both AU and VST3 versions). Expected: a grey 1040 x 740 window, no sound, no crash.
- [ ] Wait for the owner's confirmation before Phase 1.

---

## Phase 1: Engine core

Read spec sections 4, 5 and 9 before starting.

### Task 5: Theory types, ranges, PCG32, seeds and idea codes

**Files:**
- Create: `engine/CMakeLists.txt`, `engine/include/md/ranges.hpp`, `engine/include/md/types.hpp`, `engine/include/md/theory.hpp`, `engine/src/theory.cpp`, `engine/include/md/pcg32.hpp`, `engine/src/pcg32.cpp`, `engine/include/md/ideacode.hpp`, `engine/src/ideacode.cpp`
- Test: `tests/engine/test_theory.cpp`, `tests/engine/test_pcg32.cpp`, `tests/engine/test_ideacode.cpp`
- Modify: top-level and `tests/CMakeLists.txt` (executable `md_engine_tests`, label `unit`)

**Interfaces:**
- Produces, all in `namespace md`:
  - `ranges.hpp`: `constexpr int kTicksPerQuarter = 960, kTicksPerSixteenth = 240, kTicksPerBar = 3840, kChordLow = 48, kChordHigh = 79, kChordTopLow = 62, kChordTopHigh = 77, kMudBelow = 52, kMudMinInterval = 5, kNoThirdsBelow = 48, kBassLow = 28, kBassHigh = 48, kMelodyLow = 60, kMelodyHigh = 84, kMaxVoiceMove = 7, kMaxMelodyLeap = 7, kSeedMax = 99999;`
  - `types.hpp`: `enum class Mode { phrygian, aeolian, dorian, mixolydian, ionian }` (darkest to brightest); `struct Key { int root; Mode mode; }`; `enum class Quality { maj, min, min7, min9, min11, maj7, maj9, dom7, dom9, sus4, six, add9 }`; `struct ChordSymbol { int degree; Quality quality; }` (degree 1 to 7); `using Voicing = std::vector<int>` (sorted MIDI notes); `struct NoteEvent { int pitch, start, length, velocity; auto operator<=>(const NoteEvent&) const = default; }`; `enum class LaneId { chords, bass, melody }`; `enum class Style { ukFunky, mkDub, soulful, bassline, festival, nyGroove }`; `enum class Groove { straight, swing54, ukShuffle58, heavy62 }`; `enum class Flavour { simple, soulful, jazzy }`; `struct Settings { Style style = Style::ukFunky; int keyRoot = 9; int mood = 30; int energy = 50; Groove groove = Groove::ukShuffle58; Flavour flavour = Flavour::soulful; int bars = 8; }` (mood and energy 0 to 100; bars 4, 8 or 16); `struct Lane { LaneId id; std::string pattern; std::vector<NoteEvent> events; bool pinned = false; bool muted = false; std::uint64_t seed = 0; }`; `struct PlacedChord { ChordSymbol symbol; std::string feel; int startBar; int bars; Voicing voicing; }`; `struct Idea { std::uint32_t seed; Settings settings; Key key; std::optional<Mode> borrowedFrom; std::uint64_t bankVersion; std::string entryId; bool parallel = false; std::vector<PlacedChord> progression; std::array<Lane, 3> lanes; }`.
  - `theory.hpp`: `std::array<int, 7> scaleIntervals(Mode)`; `std::optional<ChordSymbol> parseDegree(std::string_view)`; `int chordRootPc(const Key&, const ChordSymbol&)`; `std::vector<int> chordIntervals(Quality)`; `std::vector<int> chordPitchClasses(const Key&, const ChordSymbol&)` (sorted ascending, 0 to 11); `std::vector<int> scalePitchClasses(const Key&)` (sorted); `std::string chordName(const Key&, const ChordSymbol&)`; `std::string feelWord(const ChordSymbol&, std::size_t slot, std::size_t count)`; `int swingPermille(Groove)`; `std::vector<Mode> relatedModes(Mode)`; `struct ProgressionFit { bool ok; std::optional<Mode> borrowedFrom; }`; `ProgressionFit checkProgression(Mode, std::span<const ChordSymbol>)` (pitch classes relative to root 0, so the result holds in every key).
  - `pcg32.hpp`: `class Pcg32 { public: Pcg32(std::uint64_t seed, std::uint64_t stream); std::uint32_t next(); std::uint32_t below(std::uint32_t bound); std::size_t pickWeighted(std::span<const int> weights); };` and `enum class Stream { chords, bass, melody, humanize }`, `std::uint64_t laneSeed(std::uint32_t ideaSeed, Stream, std::uint32_t slot)`.
  - `ideacode.hpp`: `struct DecodedIdea { std::uint32_t seed; Settings settings; std::uint16_t bankVersion16; }`; `std::string encodeIdea(std::uint32_t seed, const Settings&, std::uint64_t bankVersion)`; `std::optional<DecodedIdea> decodeIdea(std::string_view)`.

- [ ] **Step 1: Write the failing tests.**
  - Theory:
    - `scaleIntervals(Mode::aeolian) == {0,2,3,5,7,8,10}`, `dorian == {0,2,3,5,7,9,10}`, `phrygian == {0,1,3,5,7,8,10}`, `ionian == {0,2,4,5,7,9,11}`, `mixolydian == {0,2,4,5,7,9,10}`.
    - `parseDegree("i7") == {1, min7}`, `"iv9" == {4, min9}`, `"i11" == {1, min11}`, `"VImaj7" == {6, maj7}`, `"VImaj9" == {6, maj9}`, `"IIImaj7" == {3, maj7}`, `"VII" == {7, maj}`, `"VII6" == {7, six}`, `"IV9" == {4, dom9}`, `"v7" == {5, min7}`, `"V7" == {5, dom7}`, `"Isus4" == {1, sus4}`, `"IIIadd9" == {3, add9}`, `"i" == {1, min}`; `"viii"`, `"imaj7"`, `"x7"`, `""`, `"I13"` return `nullopt`.
    - With `Key{9, Mode::aeolian}` (A aeolian): names of `i9, iv9, VImaj7, v9` are `"Am9"`, `"Dm9"`, `"Fmaj7"`, `"Em9"`; `IV9` is `"D9"`; `VII` is `"G"`; `VII6` is `"G6"`; `chordPitchClasses(Am9) == {0,4,7,9,11}`.
    - Root names use `C C# D Eb E F F# G Ab A Bb B`: `Key{3, aeolian}` `i7` is `"Ebm7"`.
    - Feel: degree 1 `"home"`; degrees 2 and 4 `"lift"`; 6 `"warm"`; 3 `"bright"`; 7 `"push"`; 5 `"tension"`, except slot 3 of 4 with degree 5 gives `"pull back"`.
    - `swingPermille`: 500, 540, 580, 620.
    - `relatedModes`: aeolian gives `{phrygian, dorian}`, phrygian `{aeolian}`, dorian `{aeolian}`, ionian `{mixolydian}`, mixolydian `{ionian}`.
    - `checkProgression`: `i9 IV9 VImaj7 v9` (the reference's Am9 D9 Fmaj7 Em9) in aeolian is ok, borrowed from dorian; `i7 iv7` in aeolian is ok with nothing borrowed; `i7 VImaj7 IIImaj7 VII` is ok in aeolian and fails in dorian (VImaj7 would be F#maj7 in A dorian); `i7 vii7` in aeolian is ok, borrowed from phrygian (vii7 is G Bb D F in A); `i9 IV9 vii7` in aeolian fails (IV9 needs dorian, vii7 needs phrygian: two sources); `i7 IImaj7` in aeolian fails (B D# F# A# fits no related mode).
  - PCG32: `Pcg32(42, 54)` first six `next()` values are `0xa15c02b7, 0x7b47f409, 0xba1d3330, 0x83d2f293, 0xbfa4784b, 0xcbed606e` (the published `pcg32-demo` output). `below(7)` over 10,000 draws stays in `[0,7)` and hits every value. `pickWeighted({0, 1000, 0})` always returns 1. `laneSeed` differs when any one argument changes (check all three) and is equal for equal arguments.
  - Idea code: round trip for every style, every groove, every flavour, bars 4, 8, 16, mood and energy 0, 37, 100, key roots 0 to 11, seeds 0, 48213, 99999; codes are exactly 13 characters of `0123456789ABCDEFGHJKMNPQRSTVWXYZ`; decoding is case-insensitive; `decodeIdea("")`, a 12-character code, a code containing `U`, and a code whose version bits are not 0 return `nullopt`; out-of-range fields (mood 101 forced via a hand-built code) return `nullopt`.
- [ ] **Step 2:** Run `cmake --build --preset dev && ctest --preset dev -L unit`. Expected: build fails, symbols undefined.
- [ ] **Step 3:** Implement. `md_engine` is a static library with `md_target_defaults` and `-ffp-contract=off`, no dependencies.
  - `checkProgression`: a chord fits a mode when all its pitch classes are in that mode's scale (root spelled from the progression's own mode). Each chord must fit the mode itself or one related mode, and all borrowed chords must share the same related mode.
  - Spelling: root pc = `(key.root + scaleIntervals(mode)[degree-1]) % 12`. Lowercase numeral sets a minor third; suffix maps: lowercase `""` min, `7` min7, `9` min9, `11` min11; uppercase `""` maj, `7` dom7, `9` dom9, `maj7`, `maj9`, `6` six, `add9`, `sus4`. Anything else is invalid.
  - Intervals: maj `{0,4,7}`, min `{0,3,7}`, min7 `{0,3,7,10}`, min9 `{0,3,7,10,14}`, min11 `{0,3,7,10,14,17}`, maj7 `{0,4,7,11}`, maj9 `{0,4,7,11,14}`, dom7 `{0,4,7,10}`, dom9 `{0,4,7,10,14}`, sus4 `{0,5,7}`, six `{0,4,7,9}`, add9 `{0,4,7,14}`.
  - Name suffixes: min `m`, min7 `m7`, min9 `m9`, min11 `m11`, maj empty, maj7 `maj7`, maj9 `maj9`, dom7 `7`, dom9 `9`, sus4 `sus4`, six `6`, add9 `add9`.
  - PCG32 is the reference `pcg32_srandom_r` / `pcg32_random_r`; `below` uses the reference bounded method (threshold rejection); `pickWeighted` draws `below(sum)` and walks the weights.
  - `laneSeed`: `splitmix64((uint64(ideaSeed) << 32) | (uint64(stream) << 24) | slot)`; `slot` must be below 2^24.
  - Idea code bits, most significant first: version 2 (always 0), seed 17, keyRoot 4, style 3, mood 7, energy 7, groove 2, flavour 2, bars 2 (0 = 4, 1 = 8, 2 = 16), bank version low 16 bits; 62 bits padded with 3 zero bits to 65, written as 13 Crockford base32 characters. Decode rejects any field out of range.
- [ ] **Step 4:** Run `ctest --preset dev -L unit`. Expected: all pass.
- [ ] **Step 5:** Commit: `feat(engine): add theory types, PCG32, lane seeds and idea codes`.

### Task 6: Bank schema, loader and starter entries

**Files:**
- Create: `engine/include/md/style.hpp`, `bank/CMakeLists.txt`, `bank/schema.json`, `bank/include/md/bank.hpp`, `bank/src/bank.cpp`, `bank/entries/*.json`, `cmake/EmbedFiles.cmake`, `tools/bank-builder/check_bank.py`
- Test: `tests/bank/test_bank.cpp`, `tests/bank/fixtures/valid/*.json`, `tests/bank/fixtures/invalid/*.json`, `tools/bank-builder/tests/test_check_bank.py`
- Modify: `.github/workflows/ci.yml` (python job: add `pytest tools/bank-builder` and `check_bank.py bank/entries` steps), top-level `CMakeLists.txt`

**Interfaces:**
- Consumes: `Mode`, `ChordSymbol`, `parseDegree` from Task 5.
- Produces:
  - `style.hpp` (engine, no JSON): `enum class Family { mkDub, soulful, bassline, festival, nyGroove }`; `enum class Confidence { placeholder, estimated, verified }`; `enum class VoicingStyle { close, drop2, organStab, pianoSpread }`; `enum class BassPattern { rollingOffbeat, offbeat, octaveBounce, garageSkip, sustained, riff, pedal, callResponse }`; `enum class MelodyMode { vocalChop, hook, riff, sparse }`; `struct Hit { int step; int length; }`; `struct RhythmCell { std::string id; int bars; std::vector<Hit> hits; }`; `struct Progression { std::vector<ChordSymbol> chords; std::vector<int> barsPerChord; int weightMilli; }`; `struct StyleEntry { std::string id; Family family; std::string name; Confidence confidence; int tempoMin, tempoMax; std::vector<Mode> modes; std::vector<Progression> progressions; VoicingStyle voicingStyle; bool parallel; int topNoteLow, topNoteHigh; std::vector<RhythmCell> chordRhythms; std::vector<RhythmCell> melodyRhythms; std::vector<BassPattern> bassPatterns; std::vector<MelodyMode> melodyModes; int swingPermille; std::vector<std::string> tags; std::uint64_t contentHash; }`; `struct Bank { std::vector<StyleEntry> entries; std::uint64_t version; }`; `std::string_view patternName(BassPattern)`, `std::string_view patternName(MelodyMode)` (kebab-case JSON names).
  - `bank.hpp`: `class BankError : public std::runtime_error` (message names entry id when known and the JSON path); `StyleEntry parseEntry(std::string_view json)`; `Bank makeBank(std::vector<StyleEntry>)` (throws `BankError` on duplicate id; version = FNV-1a 64 over `contentHash` values in id order); `Bank loadBundledBank()`.

- [ ] **Step 1:** `bank/schema.json`, JSON Schema 2020-12, matching the spec's entry format exactly, `additionalProperties: false` on every object. Fields and limits: `schemaVersion` const 1; `id` pattern `^[a-z0-9]+(-[a-z0-9]+)*$`; `family` enum `mk-dub soulful bassline festival ny-groove`; `name` non-empty string; `source` `{kind: hand-authored|analysis|learned-clip|licensed-pack, reference: string|null, confidence: placeholder|estimated|verified}`; `tempo {min, max}` integers 60 to 200; `modes` non-empty unique array of `aeolian dorian phrygian ionian mixolydian`; `progressions` non-empty, each `{degrees: non-empty array of strings matching ^(i|ii|iii|iv|v|vi|vii|I|II|III|IV|V|VI|VII)(7|9|11|maj7|maj9|6|add9|sus4)?$, barsPerChord: integer 1 to 8 or array of those, weight: number > 0 and <= 10}`; `voicing {style: close|drop-2|organ-stab|piano-spread, parallel: boolean, topNote: [lo, hi] integers 62 to 77}`; `chordRhythms` and `melodyRhythms` both required and non-empty, sharing one `$defs/rhythmCell`: `{id, bars: 1|2, hits: non-empty array of [step 0..31, length 1..32]}`; `bassPatterns` non-empty unique enum of the eight kebab names (`rolling-offbeat offbeat octave-bounce garage-skip sustained riff pedal call-response`); `melodyModes` non-empty unique enum `vocal-chop hook riff sparse`; `swing` number 0.5 to 0.75; `tags` array of strings.
- [ ] **Step 2: Write fixtures and failing tests.**
  - `fixtures/valid/`: the spec's example entry plus a `melodyRhythms` field (`mk-dub-starter-01.json`), and one with an array `barsPerChord`.
  - `fixtures/invalid-theory/` (C++ only, since the chord rule lives in the engine): `no-fitting-mode.json` (`i7 VImaj7` with modes `["dorian"]` only) and `two-borrow-sources.json` (`i9 IV9 vii7`, modes `["aeolian"]`). Both make `parseEntry` throw `BankError` naming the progression.
  - `fixtures/invalid/`, one fault each, file named after the fault: `unknown-key.json`, `missing-family.json`, `bad-degree.json` (`"viii7"`), `lowercase-maj7.json` (`"imaj7"`), `hit-past-cell.json` (bars 1, hit `[15, 2]`), `bars-mismatch.json` (array `barsPerChord` length differs from `degrees`), `top-note-out-of-range.json` (`[60, 76]`), `swing-too-high.json` (0.8), `schema-version-2.json`, `wrong-type.json` (`"tempo": "fast"`).
  - C++: every valid fixture parses; every invalid fixture throws `BankError` whose message contains the entry id when present; the spec example parses to `family == mkDub`, `progressions[0].chords == {i7, iv7}`, `barsPerChord == {2, 2}` (scalar expanded), `weightMilli == 1000`, `chordRhythms[0].hits.size() == 4`, `swingPermille == 560`, `parallel == true`; `makeBank` with two entries of the same id throws; bank version is stable when entries are given in different orders and changes when any field changes; `loadBundledBank()` has at least 6 progressions and 6 chord rhythms per family in total, and every family is present.
  - Python: `check_bank.py <dir>` exits 0 on `fixtures/valid` and on `bank/entries`, and exits 1 on each invalid fixture, printing the file name. The C++ loader and `check_bank.py` must agree on every fixture.
- [ ] **Step 3:** Run both test suites. Expected: FAIL.
- [ ] **Step 4:** Implement.
  - `parseEntry` validates everything the schema does plus: every progression passes `checkProgression` in at least one of the entry's modes (skipped when `voicing.parallel` is true), `parseDegree` succeeds, `hit.step + hit.length <= bars * 16`, `topNote[0] <= topNote[1]`, `tempo.min <= tempo.max`, array `barsPerChord` length equals `degrees` length. Weight converts to `weightMilli = round(weight * 1000)`, swing to `swingPermille = round(swing * 1000)`. `contentHash` = FNV-1a 64 of `nlohmann::json::parse(json).dump()` (keys sorted by nlohmann's default object type).
  - `cmake/EmbedFiles.cmake`: `md_embed_files(<target> <namespace> <files...>)` writes a generated `.cpp` with each file as a `static constexpr unsigned char[]` (via `file(READ ... HEX)`) and `std::span<const std::string_view> bundledEntries()`; add files to `CMAKE_CONFIGURE_DEPENDS`. `md_bank` links `md_engine` and `nlohmann_json`.
  - `check_bank.py`: stdlib plus `jsonschema` (`Draft202012Validator`), then the same extra checks as `parseEntry` that JSON Schema cannot express (lowercase numerals only take `7`, `9` or `11`; hit end; array length; min not above max), and ids unique across the directory.
- [ ] **Step 5: Author starter entries** in `bank/entries/`, `confidence: "placeholder"`, `source.kind: "hand-authored"`. Per family at least 6 progressions, 6 chord rhythms and 6 melody rhythms across one or more files. Melody rhythm densities per family span 3 to 10 hits per 2 bars, are mostly syncopated (onsets on off-beat 16ths, rests on some downbeats), and MK dub includes dense stuttered vocal-chop cells. Bass pattern lists use only `rolling-offbeat`, `offbeat`, `octave-bounce`, `garage-skip`, `sustained` and `pedal` first, since riff and call-and-response arrive in Phase 3; they may appear later in a list. Include every row of the spec's starter progressions table under its families. Signatures per the spec's family table: MK dub organ-stab parallel voicings with offbeat stabs, vocal-chop melody; Soulful close voicings with longer hits and some pads (`[[0, 16]]`), hook melody; Bassline sparser chord hits, `garage-skip` and `rolling-offbeat` bass; Festival few chords, `[[0, 32]]` pads and driving 16th stabs, `rolling-offbeat` bass; NY groove chopped syncopated stabs and the `i11` vamp. Tempo ranges from the family table. Keep each file under 60 lines.
- [ ] **Step 6:** Run C++ and Python tests. Expected: PASS.
- [ ] **Step 7:** Commit: `feat(bank): add schema, loader and starter entries`.

### Task 7: Standard MIDI File writer

**Files:**
- Create: `midiexport/CMakeLists.txt`, `midiexport/include/md/midifile.hpp`, `midiexport/src/midifile.cpp`
- Test: `tests/midiexport/test_midifile.cpp`

**Interfaces:**
- Consumes: `NoteEvent`, `Idea`, `LaneId`, `chordName` from Task 5.
- Produces: `struct MidiTrack { std::string name; std::vector<NoteEvent> notes; std::vector<std::pair<int, std::string>> markers; }`; `std::vector<std::uint8_t> writeMidiFile(std::span<const MidiTrack> tracks, int tempoBpm, int totalTicks)`; `MidiTrack laneTrack(const Idea&, LaneId)` (names `Chords`, `Bass`, `Melody`; chords track carries chord-name markers at each chord start); `std::vector<MidiTrack> ideaTracks(const Idea&)` (all three lanes).

- [ ] **Step 1: Write the failing tests.**
  - One track named `Test`, one note `{60, 0, 960, 100}`, tempo 120, total 3840 produces exactly:
    `4D 54 68 64 00 00 00 06 00 00 00 01 03 C0` then `4D 54 72 6B 00 00 00 25` then `00 FF 03 04 54 65 73 74`, `00 FF 51 03 07 A1 20`, `00 FF 58 04 04 02 18 08`, `00 90 3C 64`, `87 40 80 3C 00`, `96 40 FF 2F 00`.
  - Two tracks give format 1 with 3 track chunks: track 0 holds only name-less tempo, time signature, markers (`FF 06`) and end of track; tracks 1 and 2 hold `FF 03` names and notes.
  - At equal ticks, note-offs are written before note-ons, then by ascending pitch (two back-to-back notes of the same pitch must not cut each other off).
  - End of track is at `totalTicks` even when the last note ends earlier.
  - Tempo 124 writes `07 61 F8` (483,870 microseconds, integer division of 60,000,000 by 124).
  - `writeMidiFile` throws `std::invalid_argument` for an empty track list, a note ending after `totalTicks`, a pitch or velocity outside 0 to 127 (velocity 0 also invalid), and tempo outside 20 to 999.
- [ ] **Step 2:** Run. Expected: FAIL.
- [ ] **Step 3:** Implement on channel 1, no running status, variable-length deltas. `md_midiexport` links `md_engine` only.
- [ ] **Step 4:** Run. Expected: PASS.
- [ ] **Step 5:** Commit: `feat(midiexport): add Standard MIDI File writer`.

### Task 8: Generator and validation

**Files:**
- Create: `engine/include/md/generate.hpp`, `engine/src/generate.cpp`, `engine/src/chords.cpp`, `engine/src/bass.cpp`, `engine/src/melody.cpp`, `engine/src/internal.hpp`, `engine/include/md/validate.hpp`, `engine/src/validate.cpp`
- Test: `tests/engine/test_validate.cpp`, `tests/engine/test_generate.cpp`

**Interfaces:**
- Consumes: Tasks 5 and 6 types (`Bank`, `StyleEntry`, `Pcg32`, `laneSeed`, theory functions). Tests use `md::loadBundledBank()`, so `md_engine_tests` links `md_bank`.
- Produces: `std::vector<std::string> validate(const Idea&)` (empty means valid; one human-readable line per violation); `Idea generate(const Bank&, const Settings&, std::uint32_t seed)` (throws `std::invalid_argument` for seed above `kSeedMax`, bars not 4/8/16, keyRoot outside 0 to 11, mood or energy outside 0 to 100, or no bank entries for the style; throws `std::logic_error` listing violations if its own output fails `validate`, which is a bug).

- [ ] **Step 1: Write failing validation tests** on hand-built ideas, one per rule, each asserting exactly one violation line: a chord note at 80; a bass note at 27; a melody note at 85; two overlapping bass notes (`start 0 len 480` and `start 240`); a melody note out of key and not in the current chord (A aeolian, chord Am7, pitch 61); a chord voice moving 8 semitones between consecutive voicings with `parallel` off (and no violation with it on); a note ending after `bars * kTicksPerBar`; a progression whose bars do not sum to `bars`. A valid hand-built 8-bar idea gives an empty list.
- [ ] **Step 2: Write failing generator tests:**
  - `generate(bank, Settings{}, 48213)` twice gives identical ideas (compare `ideaTracks` bytes from Task 7); `validate` is empty; `progression` bars sum to 8; each lane has events; key root is 9.
  - A 4-bar idea from an entry whose progression is an 8-bar `i11` vamp has one chord of 4 bars and no note past tick 15360.
  - Every chord voicing has 4 notes, lies in 48 to 79, top note in 62 to 77, and passes the mud rule.
  - Bass: monophonic, in 28 to 48, every note's pitch class is the current chord's root, fifth or (octave-bounce) root, and with `offbeat` and `rolling-offbeat` no onset falls on a beat (`start % 960 == 0`).
  - Melody: in 60 to 84; consecutive leaps at most 7 semitones or exactly 12; every note starting on a quarter-note boundary is a chord tone at least 2 semitones from every other chord tone of the current chord (the no-clash rule); outside vocal-chop mode, the idea's last melody note is a pitch class of the home chord whenever the home chord's tones and the final chord's allowed tones overlap; every 2-bar block's onsets match one of the entry's `melodyRhythms` cells (after swing), blocks 1 to 3 share a rhythm, and at energy 0 the cell has the fewest hits among the entry's cells while at energy 100 it has the most.
  - Chord rule: across 200 seeds per style, every chord-lane pitch class is in the scale of `idea.key.mode` or of `idea.borrowedFrom`, and `checkProgression` passes for the idea's progression, unless `idea.parallel`.
  - Bass `pedal`: every bass note's pitch class is the home root.
  - Swing: a hit at step 3 of bar 1 (the second 16th of the pair starting at tick 480) starts at `480 + 480 * 620 / 1000 = 777` with `Groove::heavy62` and at `720` with `Groove::straight`; even steps never move.
  - Invalid settings throw `std::invalid_argument` (each field once).
- [ ] **Step 3:** Run. Expected: FAIL.
- [ ] **Step 4: Implement `validate`** with the spec's validation rules (section 4) plus progression-bars, bass-overlap and chord-rule checks (chord-lane notes in the scale of `key.mode` or `borrowedFrom`); in-key means pitch class in `scalePitchClasses(key)` or in `chordPitchClasses` of the chord sounding at the note's start; when `idea.parallel` is true the chords lane skips the in-key and voice-movement checks.
- [ ] **Step 5: Implement `generate`** in this order (all randomness from `Pcg32(laneSeed(seed, stream, slot), uint64(stream))`, slot 0 unless stated):
  1. Entries: those whose family matches the style, or all entries for `ukFunky`. Pick one with `pickWeighted` on `progressions` weight sums, verified entries doubled (chords stream).
  2. Progression: `pickWeighted` on the entry's progressions; tile its chords from bar 0, truncating the last at `bars`.
  3. Mode: the entry's modes where `checkProgression` passes (all of them for parallel entries), sorted darkest to brightest; index `mood * n / 101`. Store `borrowedFrom` in the idea.
  4. Voicings: candidates are every rotation of the chord's 4 close-position tones placed in every octave that keeps all notes in 48 to 79 and the top note in the entry's `topNote` range (and in 62 to 77). Tones: drop the fifth first, then the root, when a chord has more than 4; double the root an octave up for triads. Reject muddy candidates. Cost to previous voicing: `2 * sum(|a_i - b_i|) + |top_a - top_b|`, plus 1,000,000 if any voice moves more than 7; first chord's cost is `|top - 70|`. Ties go to the lower top note. Parallel entries: transpose the first voicing by the root movement, shifting octaves to fit the range; if no shift fits, fall back to the cost choice for that chord.
  5. Chord rhythm: pick a cell (chords stream, slot 1); tile it from each chord's start; each hit plays the whole voicing; truncate at the chord end; apply swing to odd steps: `start = pairStart + 480 * swingPermille / 1000`.
  6. Bass: pattern = the entry's first `bassPatterns` value other than `riff` and `callResponse`; `pedal` uses the `offbeat` cell with R always the home chord's root; cells per bar (step, length, pitch role): `rollingOffbeat` `(2,1,R) (3,1,R) (6,1,R) (7,1,R) (10,1,R) (11,1,R) (14,1,R) (15,1,R)`; `offbeat` `(2,2,R) (6,2,R) (10,2,R) (14,2,R)`; `octaveBounce` `(2,1,R) (3,1,O) (6,1,R) (7,1,O) (10,1,R) (11,1,O) (14,1,R) (15,1,O)`; `garageSkip` `(3,1,R) (6,2,R) (11,1,R) (14,2,F)`; `sustained` `(0,16,R)`. R is the chord root placed in 33 to 44, O is R + 12, F is the fifth above R (or below if above 48). Swing applies; truncate each note at the next note's start so the bass stays monophonic.
  7. Melody: mode = entry's first `melodyModes`. Allowed strong-beat tones per chord = chord tones at least 2 semitones from every other chord tone. Rhythm (all modes): the target is `3 + 7 * energy / 100` hits per 2 bars; cell A is picked (melody stream, slot 0) among the entry's `melodyRhythms` whose hits per 2 bars (a 1-bar cell counts twice) are nearest the target; cell B is a different cell picked the same way (slot 1), or A again when the entry has only one; blocks follow A A A B. Non-vocal-chop pitches: pitches walk from the allowed tone nearest 72: each next target moves 1 or 2 scale steps up or down (melody stream, slot 2); on quarter-note steps snap to the nearest allowed strong-beat tone; otherwise snap to the nearest scale tone; reject any choice outside 60 to 84 or leaping more than 7 (try the nearest alternative the other way); the final note snaps to the nearest tone that is both a home-chord pitch class and allowed for the final chord, or to the final chord's nearest allowed tone when there is none. Vocal-chop pitches: pick 1 to 3 allowed tones of the home chord near 72 and cycle through them on the cell's hits; each note re-snapped to the current chord's allowed tones when it is not one. Every note keeps its cell length, truncated at the next onset.
  8. Velocity: `80 + 30 * energy / 100`, plus `below(13) - 6` from the humanize stream per note, clamped 1 to 127.
  9. Run `validate`; throw `std::logic_error` with the violations if any.
- [ ] **Step 6:** Run `ctest --preset dev -L unit`. Expected: PASS.
- [ ] **Step 7:** Commit: `feat(engine): add Phase 1 generator and validation rules`.

### Task 9: `md-cli`

**Files:**
- Create: `tools/md-cli/CMakeLists.txt`, `tools/md-cli/main.cpp`, `tools/md-cli/args.hpp`, `tools/md-cli/args.cpp`
- Test: `tests/cli/test_args.cpp`, CTest entries in `tests/CMakeLists.txt`
- Modify: `docs/TESTING.md`

**Interfaces:**
- Consumes: `generate`, `loadBundledBank`, `ideaTracks`, `laneTrack`, `writeMidiFile`, `encodeIdea`, `chordName`.
- Produces: `struct CliOptions { md::Settings settings; std::uint32_t seed; int tempo; std::filesystem::path out; }`; `std::variant<CliOptions, std::string> parseArgs(std::span<const std::string_view>)` (string is the error message).

- [ ] **Step 1: Write failing tests:** `parseArgs` on `--style mk-dub --seed 48213 --out idea.mid` gives style mkDub, seed 48213, tempo 124, other settings default; `--key c#`, `--key bb`, `--key A` give roots 1, 10, 9; `--groove uk-shuffle`, `--flavour jazzy`, `--mood 0`, `--energy 100`, `--bars 16`, `--tempo 128` parse; missing `--out`, unknown flag, `--seed 100000`, `--bars 5`, `--mood 101`, `--key h`, and `--style trance` each return an error naming the bad value. CTest: `md-cli --seed 48213 --out <tmp>/idea.mid` exits 0 and writes four files; `md-cli --bars 5 --out x.mid` exits 2.
- [ ] **Step 2:** Run. Expected: FAIL.
- [ ] **Step 3:** Implement. `--seed` defaults to 48213. Writes `<out>` (all three lanes, format 1) and `<stem> - chords.mid`, `<stem> - bass.mid`, `<stem> - melody.mid` beside it. Prints, one per line: `spell 48213`, `code <idea code>`, `key a <mode>`, chord names with feel words (`Am9 home | D9 lift | ...`), and the four file paths. Errors print usage to stderr and exit 2; any other exception exits 1 with its message.
- [ ] **Step 4:** Run. Expected: PASS.
- [ ] **Step 5:** `docs/TESTING.md` section "Hearing ideas from md-cli": the exact command for five ideas (seeds 1, 2, 3, 48213, 77777, one per style), where the files land, and how to drag the three single-lane files onto three MIDI tracks in Live with an organ, a bass and a lead.
- [ ] **Step 6:** Commit: `feat(cli): add md-cli to write ideas as MIDI files`.

### Task 10: Property tests, golden tests and the update script

**Files:**
- Create: `tests/engine/test_properties.cpp`, `tests/golden/test_golden.cpp`, `tests/golden/cases.hpp`, `tests/golden/*.mid`, `scripts/update-golden.sh`
- Modify: `tests/CMakeLists.txt`, `docs/TESTING.md`

**Interfaces:**
- Consumes: `generate`, `validate`, `ideaTracks`, `writeMidiFile`, `loadBundledBank`.

- [ ] **Step 1: Property test:** for seeds `0` to `N - 1` (`N` from env `MD_PROPERTY_SEEDS`, default 1000), every `Style`, and aura corners `(mood, energy)` in `(0,0) (0,100) (100,0) (100,100)`, with groove and flavour cycling by seed and bars cycling 4, 8, 16: `validate` is empty and `generate` is deterministic for the first 50 seeds. Record per-idea time with `std::chrono::steady_clock` (tests may read the clock; the engine may not). When compiled with `MD_PERF_ASSERT`, assert the 99th percentile is under 20 ms.
- [ ] **Step 2:** `cases.hpp`: 50 cases built by formula for `i` in 0 to 49: style `i % 6`, seed `(1000 + i * 7919) % 100000`, keyRoot `i % 12`, `(mood, energy)` from `(0,0) (0,100) (100,0) (100,100) (30,50)` at `i % 5`, groove `i % 4`, flavour `i % 3`, bars 4 when `i % 10 == 4`, 16 when `i % 10 == 9`, else 8. File name `case-NN.mid`, tempo 124, all three lanes.
- [ ] **Step 3:** Golden test: each case's bytes equal `tests/golden/case-NN.mid`. With env `MD_UPDATE_GOLDEN=1` it writes the files instead and passes. Path to the golden folder comes from a compile definition.
- [ ] **Step 4:** `scripts/update-golden.sh`: copies current goldens to `build/golden-review/before/`, runs the golden test with `MD_UPDATE_GOLDEN=1`, copies new files to `build/golden-review/after/`, lists which cases changed, and prints: "Listen to before/ and after/ in Live. If you approve, commit tests/golden. If not, run git checkout tests/golden." It never commits.
- [ ] **Step 5:** Run `scripts/update-golden.sh` once to create the first goldens, then `ctest --preset dev`. Expected: all tests pass. Run the 10,000-seed property test once locally under `release`: `MD_PROPERTY_SEEDS=10000 ctest --preset release -R properties`. Expected: PASS.
- [ ] **Step 6:** `docs/TESTING.md`: "When a golden test fails" in plain words.
- [ ] **Step 7:** Commit: `test: add property tests and 50 golden MIDI files`.

### Phase 1 gate

- [ ] CI green on every job, including the nightly 10,000-seed run triggered once by hand with `gh workflow run`.
- [ ] The owner runs the five `md-cli` commands from `docs/TESTING.md`, drags the lanes into Live, and confirms three lanes that fit together.
- [ ] Record what they say (keep, close, skip per idea and any notes) in `docs/DECISIONS.md` as input for Phase 2.
- [ ] Write the Phase 2 plan only after this gate.

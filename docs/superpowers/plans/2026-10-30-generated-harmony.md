# Generated Progressions and Rhythms: Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Sprinkle invents new chord progressions, chord rhythms and melody rhythms in each style, instead of picking them from a short written list.
The written bank entries become the examples the generator learns from.

**Architecture:**
- A new engine module, `engine/src/invent.cpp` with its declarations in `engine/src/internal.hpp`, builds small statistical models when the bank is loaded.
  - The models are built once per bank and cached with it. Find the right place in the bank or engine load path.
  - Each model is pure, deterministic code with no allocation concerns. Generation runs off the audio thread.
- **Progressions:** a first-order Markov chain over chord degrees, with chord qualities and bar lengths.
  - It is learned from every progression of the family that shares the chosen entry's mode group.
  - It is smoothed so unseen moves between diatonic chords can happen.
  - A result is accepted only when it fits the entry's modes and voices validly.
- **Rhythms:** a per-16th-step onset model, with length and density limits.
  - It is learned from the family's chord rhythm cells and, separately, from its melody rhythm cells.
- The entry is still chosen as today. It sets voicing style, modes, tempo, bass patterns and melody modes.
- Only the progression and the rhythm cells come from the models.
- If the models fail to produce a valid result within a fixed number of tries, generation falls back to the entry's written material, so every idea is still guaranteed to be valid.

**Tech Stack:** C++20, Catch2, CMake presets, md-cli.

**Spec:** The owner's request of 2026-10-30. They want real generation, not recycling: "generate the progressions themselves", learned from the style.

## Global Constraints

- **Determinism.** Every draw is `Pcg32(<lane seed>, slot)`.
  - The progression uses the existing chords progression slot 1.
  - The chord rhythm uses the existing chord rhythm cell slot 2.
  - The melody rhythms use the existing melody slots 0 and 1, with the answer cell at slot 5.
  - Reroll isolation between lanes still holds.
- **Validity.** Every idea still passes `validate`, and the property test passes over every style, flavour, energy, groove and bar count.
- **Pins and variety.** Pins and the variety rules keep working.
  - The turnaround draws an alternative generated cell from the same rhythm model, at slot 4.
  - The melody answer is a generated cell that differs from A and B.
- **Docs.** No em dash. Long Markdown has one sentence per line.
- **Commits.** Never add Co-Authored-By or an agent name.
- **Before each commit,** dev tests, lint and format must pass.

## Model rules

### Mode group

- The minor group is aeolian, dorian and phrygian. The major group is ionian and mixolydian.
- A family's model for an entry learns from every progression in that family whose entry shares at least one mode group with the chosen entry.

### Progression model

- **States** are degree tokens exactly as the bank writes them: numeral, case and quality suffix, for example `i9` or `VImaj7`.
- **Transitions** count each consecutive pair in every learned progression, plus the wrap from the last chord back to the first.
- **Smoothing.**
  - Add 0.15 to every pair between tokens seen in the family's learned set.
  - Also allow plain diatonic tokens of the entry's first mode that the family has not used, at 0.05 weight.
    - These are the seven degrees in the family's most common quality style: 7ths if most learned tokens carry a 7 or 9, otherwise triads.
  - This is what makes new movement possible.
- **Start.** Draw the first chord from the learned start distribution, the first chord of each progression, with the same smoothing.
- **Length and bars.**
  - Draw a bar template from the family's learned `barsPerChord` patterns, expanded to per-chord lists.
  - The template sets the number of chords.
- **Chain.**
  - Draw each next chord from the transition row.
  - Never repeat the same token twice in a row, unless the template has 1 chord.
- **Closing.** The wrap from the last chord back to the first must have a learned or smoothed weight above zero. If it does not, redraw.
- **Acceptance.**
  - The progression must fit at least one of the entry's modes (`checkProgression`).
  - It must voice validly under all three flavours (`voiceChords` and validation of the chord lane).
  - Try up to 24 times, then fall back to a written progression picked as today.
- **Weight toward the written.** With probability 0.2, use a written progression as is. Keeping a share of the classic moves makes the style recognisable.

### Rhythm model (chord cells and melody cells, each separately)

- **Learning.**
  - Express every learned cell as 1-bar slices of 16 steps. Split 2-bar cells into 2 slices.
  - For each step, learn the onset probability.
  - For each onset, learn a length histogram in steps.
  - Learn the slice density range: the minimum and maximum onsets per bar across the learned cells.
- **Generating a cell** of 1 bar, or 2 bars for melody, made from two 1-bar slices:
  1. Draw onsets step by step from the learned probabilities.
  2. Every onset probability is multiplied by `0.7 + 0.6 × energy / 100`, then capped at 0.95.
  3. Accept the slice only if its onset count lies within the learned density range, widened by 1 on each side, and has at least 1 onset.
  4. Give each onset a length drawn from the histogram, clamped to the gap before the next onset.
  5. Try up to 24 times, then fall back to a written cell.
- **Melody.** The melody's 2-bar cell must differ from a straight copy of slice 1 in slice 2, so the second bar answers the first.
- **Names.**
  - A generated cell is named `fresh` plus a short tag, for example `fresh-stabs` for chords and `fresh-hook` for melody.
  - The screen shows the name as it shows a written cell id today.
- **Written share.** With probability 0.2, use a written cell as is, the same as for progressions.

## Review Focus

1. **Novelty is real.** Over 500 seeds per family, at least 60% of progressions are not in the bank verbatim, and at least 15 distinct progressions appear. Chord rhythm cells show the same novelty. Task 1 and Task 2 test this.
2. **It still sounds like the style.** Every generated progression uses only tokens seen in the family, or plain diatonic smoothing tokens. Smoothing tokens appear in under 15% of chords. Task 1 tests this.
3. **No invalid ideas.** The property test runs across every tier and setting with zero failures. The fallback is taken in under 2% of generations, and the report records the measured rate.
4. **Pins.** Pinned chords keep their generated progression and rhythm through sprinkles, key changes and energy changes. The existing pin tests stay green.
5. **Speed.** Generating an idea stays under 5 ms in release on average over 1000 ideas. The models are built once per bank, never per idea. Task 3 measures this.

---

### Task 1: Progression model

**Files:** create `engine/src/invent.cpp`, add declarations to `engine/src/internal.hpp`, add `tests/engine/test_invent.cpp`, and update the CMake entries.

**Interfaces:**
- `struct ProgressionModel`, built from the learned progressions of a family and mode group.
- `ProgressionModel buildProgressionModel(const Bank&, Family, ModeGroup)`.
- `std::optional<Progression> inventProgression(const ProgressionModel&, const StyleEntry&, Pcg32&)`. It returns nullopt after 24 failed tries, and the caller falls back.

**Steps:**
- [ ] Write the failing tests:
  - every invented progression fits a mode of the entry
  - tokens are only learned tokens or smoothing tokens
  - smoothing tokens make up under 15% of chords
  - novelty is at least 60% with at least 15 distinct progressions over 500 seeds per family
  - there are no immediate repeats
  - results are deterministic
- [ ] Run the tests and watch them fail.
- [ ] Implement, run the tests and commit: `feat(engine): learn and invent chord progressions per style`.

### Task 2: Rhythm model

**Files:** the same as Task 1.

**Interfaces:**
- `struct RhythmModel`.
- `RhythmModel buildRhythmModel(const Bank&, Family, RhythmKind kind)`, where `RhythmKind` is chords or melody.
- `std::optional<RhythmCell> inventRhythm(const RhythmModel&, int bars, int energy, Pcg32&)`.

**Steps:**
- [ ] Write the failing tests:
  - the density stays within the learned range ±1
  - lengths never overlap the next onset
  - energy 100 gives more onsets on average than energy 0
  - novelty is at least 60% of cells not verbatim in the bank
  - melody slice 2 differs from slice 1
  - results are deterministic
- [ ] Implement and commit: `feat(engine): learn and invent chord and melody rhythms per style`.

### Task 3: Use them in generate

**Files:** `engine/src/generate.cpp`, `engine/src/chords.cpp`, `engine/src/melody.cpp`, and wherever the models get cached with the bank.

**Steps:**
- [ ] Write the failing tests:
  - generated ideas use invented material, with the written share near 20%
  - the variety turnaround and the melody answer use invented alternates
  - pinned chords keep invented material
  - the property test passes
  - the fallback rate is under 2%
  - timing is under 5 ms per idea in release, measured in a `[.bench]` test whose numbers go in the report
- [ ] Implement it, regenerate the goldens (all lanes change, as expected) and commit the goldens separately: `feat(engine): sprinkle invents progressions and rhythms`.

### Task 4: Listening pack and docs

- [ ] Write `~/Desktop/magic-dust/invented-review/<family>/` with 12 ideas per family. Mix moods 20 and 80 and energies 30 and 75. Name each file with its seed and the invented progression, for example `soulful-s3-Imaj9-iii7-vi9-IVmaj7.mid`.
- [ ] Also write `progressions.txt` listing 50 invented progressions per family, so the owner can scan the variety.
- [ ] Update the docs:
  - `docs/DECISIONS.md`: progressions and rhythms are invented from learned models, with the 20% written share and the fallback.
  - `docs/TESTING.md`: an "Invented ideas in your daw" section.
  - `README.md`: the "What it is" and features text, so it says ideas are invented in the style.
- [ ] Run the dev, asan and tsan suites, lint, format and `scripts/validate.sh build/dev`, then commit: `docs: invented progressions and rhythms`.

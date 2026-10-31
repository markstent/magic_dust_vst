# magic_dust_vst

## Target

- Magic Dust: AU and VST3 instrument plugin for macOS, built with JUCE.
- Must work in any Mac DAW. Main test host: Ableton Live 12. Release testing also covers Logic Pro or GarageBand, Reaper, Bitwig, Cubase, Studio One and FL Studio.
- Interface and docs say "your daw", never a specific DAW, except in per-DAW setup guides.
- Each lane plays through its built-in sound or through the user's own instrument, chosen per lane (MIDI out: chords channel 1, bass 2, melody 3).
- In Live, a lane reaches the user's instrument through another Magic Dust instance in sender mode (one per lane), because Live merges plugin MIDI to one channel. See `docs/DECISIONS.md`.
- Mac only for v1; keep code portable for a later Windows build.
- The full product spec is in `docs/spec/` (`magic-dust-spec.md`, `magic-dust-visual-spec.md`, `visual/`). Read the relevant section before working on any part.
- Departures from the spec go in `docs/DECISIONS.md`.
- Public open-source repo under AGPLv3 (required for free JUCE use).

## Release (do once the product is built)

- Semantic versioning (`MAJOR.MINOR.PATCH`). One source of truth for the version, used by the build, the plugin, and the installer.
- Each release is a git tag `vX.Y.Z` with a GitHub Release that has the installer attached and auto-generated release notes.
- Installer: a `.dmg` containing a `.pkg` that installs the AU to `/Library/Audio/Plug-Ins/Components` and the VST3 to `/Library/Audio/Plug-Ins/VST3`. No raw plugin download.
- Sign the plugin and `.dmg` with Developer ID Application, sign the `.pkg` with Developer ID Installer, notarize and staple both. Certificates and notarization credentials live in GitHub Actions secrets, never in the repo.
- The release build and installer are produced by CI (GitHub Actions) on tag push, not by hand.
- The README is a product page: shields (version, build status, platform, license), screenshots, a short feature list, install steps, Ableton setup, and build-from-source instructions.
- Keep a LICENSE file.

## Model use

- Opus 5.5 (`claude-opus-5-5`) does the thinking: design, specs, root-cause debugging, and reviewing every result before it is called done.
- Sonnet (`implementer` subagent, `model: sonnet`) is the normal model: writes code and tests from an exact spec.
- Haiku (`runner` subagent, `model: haiku`) handles bash commands and simple tasks: running tests and builds, installs, git housekeeping, collecting output, file moves, mechanical edits.
- If those agent types are not loaded, use a general-purpose subagent with the same `model` value.

Minimizing token cost is a priority:

- Always pick the cheapest model that can do the task well: Haiku first, then Sonnet, Opus only for thinking and review.
- Opus does not run commands, read build or test logs, or explore the codebase itself. Delegate to Haiku and ask for a short summary back.
- Give subagents tight, specific prompts (exact files, exact task, expected output) so they do not wander.
- Ask subagents to return only what matters: pass/fail, errors, diffs. Never full logs.
- Opus reviews diffs, not whole files.
- Batch related small tasks into one subagent call instead of many.
- Do a task directly on Opus only when writing the spec would cost more than doing it.

## Code

- Write the simplest code that does the job.
- No speculative features, abstractions, config, or dependencies.
- Prefer the standard library and existing code over new code.
- Small functions, clear names, no dead code, no commented-out code.
- Handle errors explicitly at boundaries (file I/O, host/plugin API, user input).
- Never allocate, lock, or block on the audio thread.

## Tests

- Every change ships with tests. Bug fixes start with a failing test that reproduces the bug.
- Write the test first (red, green, refactor).
- Tests are fast, deterministic, and independent. Flaky tests get fixed, not skipped.
- DSP code is tested against known inputs and expected outputs with an explicit tolerance.

## Engineering practice

- Lint, format, and tests must pass before any commit. Fix failures you find even if you did not cause them.
- One logical change per commit, with a message saying why.
- Work on a branch, never directly on `main`.
- Review every diff before calling work done.

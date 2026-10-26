# Testing

## Setting up your Mac

These tools are already installed.
Homebrew provides cmake, ninja, llvm (clang-format and clang-tidy) and uv.
pluginval lives in `~/Applications`.

## Building

Run these from the project folder in Terminal.
The first build downloads JUCE and takes several minutes.
Later builds only rebuild what changed.

- `cmake --preset dev` sets up the everyday build.
- `cmake --build --preset dev` builds Magic Dust and copies it to your plug-in folders, so your daw can find it.
- `cmake --preset lint && cmake --build --preset lint` builds again with the strict code checker switched on.

## Running tests

- `ctest --preset dev` runs every automatic test and prints how many passed.
- `ctest --preset dev -L plugin` runs only the tests for the plug-in itself.

All tests should say "Passed".
Anything marked "Failed" is a bug to report.

## Checking the plugin

- `scripts/validate.sh` runs the industry checkers on the built plug-in.
  It runs pluginval on the VST3 and the AU, then Apple's `auval` on the AU.
  Every check should end in a pass.

If `auval` says it cannot find Magic Dust right after a new build, macOS may be remembering an old list of plug-ins.
Run `killall -9 AudioComponentRegistrar` and then run the script again.

## Hearing ideas from md-cli

`md-cli` writes Magic Dust ideas as MIDI files, so you can hear them in your daw before the plug-in is finished.

To open Terminal, press Cmd+Space, type `Terminal` and press Return.
Then type `cd ` (with a space after it), drag the Magic Dust project folder from Finder into the Terminal window, and press Return.
Next run `cmake --build --preset dev`, which builds md-cli (and the plug-in) so it is ready to run.
Then run these five commands, one idea for each style:

```
build/dev/tools/md-cli/md-cli --style uk-funky --seed 1 --out ~/Desktop/magic-dust/uk-funky-1.mid
build/dev/tools/md-cli/md-cli --style mk-dub --seed 2 --out ~/Desktop/magic-dust/mk-dub-2.mid
build/dev/tools/md-cli/md-cli --style soulful --seed 3 --out ~/Desktop/magic-dust/soulful-3.mid
build/dev/tools/md-cli/md-cli --style bassline --seed 48213 --out ~/Desktop/magic-dust/bassline-48213.mid
build/dev/tools/md-cli/md-cli --style festival --seed 77777 --out ~/Desktop/magic-dust/festival-77777.mid
```

Each command prints the spell number, the idea code, the key, the chords and where the files went.
The files land in the `magic-dust` folder on your Desktop, which is created if it is missing.
Every idea gives four files, for example for `uk-funky-1.mid`:

- `uk-funky-1.mid` has chords, bass and melody together.
- `uk-funky-1 - chords.mid` has only the chords.
- `uk-funky-1 - bass.mid` has only the bass.
- `uk-funky-1 - melody.mid` has only the melody.

The same seed and settings always give the same idea.
Change the seed (0 to 99999) for a new idea.
Run `build/dev/tools/md-cli/md-cli --help` to see the other options: key, bars, mood, energy, groove, flavour and tempo.

### Dragging the files into your daw

This works the same in any daw.
The steps below use the common names, so look for the matching buttons in yours.

1. Make three MIDI tracks.
2. Load an organ on the first track, a bass on the second and a lead on the third.
3. Drag `... - chords.mid` from Finder onto the first track, `... - bass.mid` onto the second and `... - melody.mid` onto the third.
4. Set the project tempo to the tempo you used (124 unless you passed `--tempo`), then press play.

## Trying it in Ableton

To be filled in by a later task.

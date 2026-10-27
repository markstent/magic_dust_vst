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
- `cmake --preset release && cmake --build --preset release` builds the release version the long check needs.
  Run it once before the next command.
- `MD_PROPERTY_SEEDS=10000 ctest --preset release -R properties` runs the long check that generates thousands of ideas and tests each one.

All tests should say "Passed".
Anything marked "Failed" is a bug to report.

## When a golden test fails

The golden test keeps 50 approved MIDI files in `tests/golden`.
It makes the same 50 ideas again and checks that they match those files note for note.
If it fails, it means the music changed: some idea now sounds different from the one you approved.

1. Run `scripts/update-golden.sh` from the project folder.
   It lists which cases changed and puts the old and new files in `build/golden-review/before/` and `build/golden-review/after/`.
2. Drag the same case from `before/` and `after/` into your daw and listen to both.
   Listen for changed chords, a different bass line, a melody that moved, or notes that are now too high, too low, too busy or off the beat.
3. If the new version is the one you want, commit `tests/golden`.
4. If it is not, run `git checkout tests/golden` to go back to the approved files, and report it as a bug.

The script never commits anything, so nothing changes for good until you commit.

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
Then run these six commands, one idea for each style:

```
build/dev/tools/md-cli/md-cli --style uk-funky --seed 1 --out ~/Desktop/magic-dust/uk-funky-1.mid
build/dev/tools/md-cli/md-cli --style mk-dub --seed 2 --out ~/Desktop/magic-dust/mk-dub-2.mid
build/dev/tools/md-cli/md-cli --style soulful --seed 3 --out ~/Desktop/magic-dust/soulful-3.mid
build/dev/tools/md-cli/md-cli --style bassline --seed 48213 --out ~/Desktop/magic-dust/bassline-48213.mid
build/dev/tools/md-cli/md-cli --style festival --seed 77777 --out ~/Desktop/magic-dust/festival-77777.mid
build/dev/tools/md-cli/md-cli --style ny-groove --seed 5 --out ~/Desktop/magic-dust/ny-groove-5.mid
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
Flavour is soulful by default, which keeps the chords as written; simple is plainer (triads and 7ths) and jazzy is richer (9ths, 11ths and 13ths where they stay in key).
For example, `build/dev/tools/md-cli/md-cli --flavour jazzy` makes a richer version of the same idea.

### Dragging the files into your daw

This works the same in any daw.
The steps below use the common names, so look for the matching buttons in yours.

1. Make three MIDI tracks.
2. Load an organ on the first track, a bass on the second and a lead on the third.
3. Drag `... - chords.mid` from Finder onto the first track, `... - bass.mid` onto the second and `... - melody.mid` onto the third.
4. Set the project tempo to the tempo you used (124 unless you passed `--tempo`), then press play.

## Trying it in your daw

These steps are for Ableton Live 12.

1. Build Magic Dust with `cmake --build --preset dev`, which copies it to your plug-in folders.
2. Open Live and go to Settings, then Plug-Ins.
3. Switch on both Audio Units and VST3 system folders.
4. Press Rescan.
5. Make a MIDI track.
6. Find Magic Dust in the Plug-Ins browser and drag it onto the track.

Other daws need the same rescan before they show Magic Dust.

## Phase 2 in your daw

The sending steps below are for Ableton Live 12; the rest works the same in any daw.
Build first with `cmake --build --preset dev` and rescan your plug-ins as above.

### Loading Magic Dust and sprinkling

1. Make a MIDI track and drag Magic Dust onto it.
2. Press play (the key under the screen) to hear the idea.
   Press your daw's own play instead and Magic Dust follows it.
3. Press sprinkle for a new idea.
   Every press gives a different one.

### Turning flavour, aura and groove

1. Click the flavour field to step through simple, soulful and jazzy.
   Soulful is the chords as written, simple is plainer and jazzy is richer.
2. Drag on the aura pad.
   Left to right is mood, bottom to top is energy.
3. Click the groove field to step through the grooves, or scroll over it.
4. After each change, sprinkle again and listen to whether the idea matches what you set.

### Balancing the lanes

1. Press play.
2. Drag the small slider under each lane's name (chords, bass, melody) and hear that lane's built-in sound get quieter and louder.
3. Double-click a slider to put it back to full.
4. Check the MIDI you drag out does not change with the levels.

### Dragging an idea out

1. Drag a chord bubble onto a MIDI track.
   You get that one chord.
2. Drag a lane onto a MIDI track.
   You get that lane.
3. From the files `md-cli` wrote (see "Hearing ideas from md-cli"), drag the one without a lane name, for example `uk-funky-1.mid`, onto an empty area of your daw.
   This file holds chords, bass and melody together.
4. Write down whether your daw split it into three tracks, kept it as one track, or refused it.
   This answers the open question about "bottle it".

### Sending a lane to your own instrument

Use the VST3 version of Magic Dust here.
The AU version cannot send MIDI to other tracks in Ableton Live, so the VST3 is the one to load.
The AU and the VST3 are separate and cannot see each other, so load both the main Magic Dust and the sender as VST3.

1. Load Magic Dust (VST3) on a MIDI track.
   This is the main one, with the full screen.
2. Make a second MIDI track and load Magic Dust (VST3) on it too.
   It opens as "magic dust sender".
   A new sender starts on chords, the first lane nobody sends, so the main one's organ goes quiet at once.
3. Click the lane you want to send: chords, bass or melody.
   The main one's built-in sound for any lane being sent is silent while it is sent.
   The light on the key shows your pick, and the status line says "sending bass from magic dust" (or the lane you picked).
   If it says "no magic dust found", load Magic Dust on another track first.
4. Make a third track and load your own instrument on it.
5. On that instrument track, set MIDI From to the sender track.
6. Set the second chooser under it to Magic Dust.
7. Set Monitor to In.
8. Press play in your daw.
   The sender plays only while your daw plays, and it stays in time with the main Magic Dust.
   The sender itself makes no sound.
9. Click "full magic dust" on a sender to turn it into a full Magic Dust of its own with its own idea.

### The listening review

1. Sprinkle 20 ideas in each of the families: uk funky, mk dub, soulful, bassline, festival and ny groove.
   To change family, click the style name on the screen's top line.
2. Drag the ones you like into clips in your daw.
3. Rate every idea keep, close or skip.
4. The target is at least 1 keep in every 3 ideas.
   Write down the ones that miss, with the style and seed, so they can be fixed.

### The changed reference ideas

Phase 2 changed some of the 50 reference ideas: flavour now changes the simple and jazzy ones, and each style now voices its chords in its own shape.
The old and new versions are in the `golden-review` folder in the `magic-dust` folder on your Desktop, in `before` and `after`, with the changed cases listed in `changed.txt`.
Drag the same case from both folders into your daw and listen to the before, then the after.
Say which after files you are happy with and which sound worse, and why.

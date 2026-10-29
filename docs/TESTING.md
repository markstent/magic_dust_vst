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
2. Press the play key under the screen to hear the idea.
   Press your daw's own play instead and Magic Dust follows it.
3. Press the sprinkle key for a new idea.
   Every press gives a different one.
4. Click an empty part of the plugin window, then press the spacebar.
   The preview starts, and the spacebar stops it again.
5. Start your daw's own transport and press stop in Magic Dust, then the spacebar.
   The bottom line shows "FOLLOWING YOUR DAW - USE ITS TRANSPORT" for 2 seconds and your daw keeps playing.
   Stop only stops Magic Dust's own preview, so stop the transport in your daw (in Live, its own stop key).

### Turning the knobs

1. Drag the flavour knob up and down to step through simple, soulful and jazzy.
   Soulful is the chords as written, simple is plainer and jazzy is richer.
2. Drag the mood knob and the energy knob.
   Mood runs from dark to bright and energy from low to high.
3. Drag the groove knob to step through the grooves, or scroll over it.
4. After each change, sprinkle again and listen to whether the idea matches what you set.

### Balancing the lanes

1. Press play.
2. Drag each lane's level knob (chords, bass, melody) in the knob panel and hear that lane's built-in sound get quieter and louder.
3. Double-click a level knob to put it back to full.
4. Check the MIDI you drag out does not change with the levels.

### Muting lanes

1. Press play.
2. Press the MUTE keys for bass, then for melody, in the key row.
   Their LEDs light, their rows on the display fade, and the bass and the lead drop out while the chords play on.
3. Check the MIDI out of those lanes stops too: with a bass sender (see below) loaded on another track and your own instrument on it, the instrument goes quiet.
4. Press the keys again and hear each lane come back at its next note.
5. Check the level knobs did not move.

### Pinning parts

1. Press sprinkle until you have an idea you like, then press play.
2. Press the PIN key under CHORDS.
   Its LED lights purple, a PINNED tag appears beside the chords row, and the line at the bottom of the display says "CHORDS PINNED".
   The sound does not change when you press the key.
3. Press sprinkle a few times.
   The chords stay the same every time, while the bass and melody change to fit them.
4. Press the PIN key under CHORDS again to unpin it, then press the PIN key under BASS.
5. Press sprinkle a few times.
   The bass keeps its rhythm, but its notes move to follow the new chords, while the chords and melody change.
6. Turn the FLAVOUR knob, pick another style and move the KEY menu.
   The bass stays pinned through all three, and it follows the new key.
7. Mute the bass with its MUTE key.
   The bass goes quiet and stays pinned.
   Unmute it and it plays again.
8. Pin all three parts and press sprinkle.
   Nothing changes and the notes stay the same.

### Saving and reopening with pins

1. Pin the bass and press sprinkle until the bass has changed notes to follow the chords.
2. Save your daw's project, close it and open it again.
3. Open Magic Dust.
   The bass PIN key is lit, and the idea sounds the same as before you saved, with the same bass notes.
4. Press sprinkle and check the bass keeps its rhythm.

### Moving a part by octaves

1. Press play.
2. Press the down key of the rocker under the BASS mute key.
   The window reads -1 and the bass drops an octave, the sub bass sounding lower while the chords and melody stay put.
3. Press it again for -2, where the down key dims and stops answering, then press the up key to climb back.
4. With a bass sender loaded on another track and your own instrument on it, check the instrument follows the new octave.
5. Press the down key to -1, then drag the bass lane onto a MIDI track and check its notes sit an octave below the ones you hear at 0.
6. Check the display's rows did not move.

### Dragging an idea out

1. Drag a chord cell onto a MIDI track.
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
   It opens as the sender view, with "MAGIC DUST SENDER" at the top.
   A new sender starts on chords, the first lane nobody sends, so the main one's organ goes quiet at once.
3. Click the lane you want to send: chords, bass or melody.
   The main one's built-in sound for any lane being sent is silent while it is sent.
   The LED above the key shows your pick, and the status window says "SENDING BASS FROM MAGIC DUST" (or the lane you picked, in capitals).
   If it says "NO MAGIC DUST FOUND", load Magic Dust on another track first.
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

## Phase 3 in your daw

Phase 3 gives each idea a proper bass line.
Low energy gives a sparse bass and high energy gives a busy one.
Above energy 50 the bass also leans into each chord change with a short approach note, except the sustained and pedal patterns.
Rerolling the bass changes its root, octave and fifth.
There are 8 bass patterns now, including a riff and a call-and-response that plays in the gaps between chord hits.

### Dropping the review files into your daw

1. Open the `phase-3-review` folder in the `magic-dust` folder on your Desktop.
   It has one folder for each family: mk-dub, soulful, bassline, festival and ny-groove.
2. Each folder has 20 files named like `mk-dub-s3-e30-offbeat.mid`.
   That is the family, the seed, the energy and the bass pattern.
   Seeds 1 to 10 are at energy 30 and seeds 11 to 20 are at energy 80.
3. Drag the files you want to hear into your daw, one per clip.
   Each file holds chords, bass and melody on separate tracks.
   The file maker could not leave the melody out, so mute the melody track while you listen to the bass.
4. Put a bass sound on the bass track and a chord sound on the chords track.

### Rating the bass

1. Listen to each idea and rate it keep, close or skip, on the bass only.
2. The target is at least 1 keep in every 3 ideas.
   Write down the ones that miss, with the family and seed, so they can be fixed.
3. Check that the bass never fights the chords.
   A note that rubs against a chord is a miss, except a short approach note that leads into the next chord.
4. Compare the energy 30 files with the energy 80 files.
   Check that the higher energy adds movement and approach notes.
5. Check that rerolling the bass changes it, using the files described under Rerolling the bass below.

### The new patterns

1. Open the `new-patterns` folder inside `phase-3-review`.
   It has 10 riff files and 10 call-and-response files, from uk funky, bassline and ny groove, the families that play them.
   Half are at energy 30 and half at energy 80.
2. Rate each one keep, close or skip on the bass, the same way as above.
3. For a riff, check that the same short phrase repeats every 2 bars and every chord gets some bass.
4. For a call-and-response, check that the bass plays in the gaps between chord hits and does not talk over them.

### Rerolling the bass

1. Open the `reroll` folder inside `phase-3-review`.
   It has 5 ideas, each as 3 files with the same chords and melody and a different bass.
   The files are named like `mk-dub-s2-bass1-offbeat.mid`, where `bass1`, `bass2` and `bass3` are the three bass rerolls.
2. Drag the 3 files of one idea onto 3 clips side by side and play them one after the other.
   Check that the chords and melody sound the same and only the bass changes.
   Check that the rerolls differ, in the root, the octave or the fifth, and sometimes in the pattern.
3. The reroll key itself comes in Phase 5.

### The changed reference ideas

Phase 3 changed 45 of the 50 reference ideas, all in the bass.
The chords and melody are the same as before.
The old and new versions are in the `golden-review-phase-3` folder in the `magic-dust` folder on your Desktop, in `before` and `after`.
`changed.txt` lists each changed case with its style, energy and bass pattern.
Drag the same case from both folders into your daw and listen to the before, then the after.
Say which after files you are happy with and which sound worse, and why.

## New patterns in your daw

The bank now has a third starter entry in every family, and some of them are in brighter modes.

### Listening to the new patterns

1. Open the `new-patterns-review` folder in the `magic-dust` folder on your Desktop.
   It has a folder for each family: mk dub, soulful, bassline, festival and ny groove.
2. Each folder has 10 ideas from the new entry of that family, all at energy 60.
   Half are at mood 20 and half are at mood 80.
   The files are named like `soulful-s5-m80-ionian.mid`, with the seed, the mood and the mode.
3. Drag the files you want to hear into your daw, one per clip.
   Each file holds chords, bass and melody on separate tracks.
4. Put a bass sound on the bass track, a chord sound on the chords track and a melody sound on the melody track.
5. Rate every idea keep, close or skip.
6. Say which ones sound brighter than the older entries, and which ones still sound alike.
   Write down the misses with the file name, so the entry can be fixed.

### The changed reference ideas

The new entries changed 40 of the 50 reference ideas, because the seeds now pick different entries.
The old and new versions are in the `golden-review-bank` folder in the `magic-dust` folder on your Desktop, in `before` and `after`.
`changed.txt` lists each changed case with its style, energy and the entry it now picks.
Drag the same case from both folders into your daw and listen to the before, then the after.
Say which after files you are happy with and which sound worse, and why.

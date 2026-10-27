# Studio Console Look Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Reskin the Magic Dust editor as the owner's approved studio console: desk-metal faceplate, walnut cheeks, cream backlit display, black knobs and cream keys.

**Architecture:** Same component tree and timer-driven `refresh()` as today.
Positions stay as logical-pixel constants in `ui/Tokens.h` and the editor keeps scaling with `setTransform`.
Every knob is one `md::ui::Knob` (a `juce::Slider` subclass with a `SliderParameterAttachment`), so drag, wheel, keyboard, double-click reset, host gestures and accessibility come from JUCE.
Controls that do not exist in code yet (lane pattern knobs, pin, reroll, mute, swap, keep, bottle it, spells panel, toast, sync and midi LEDs) are Phase 5: this plan gives them positions in the visual spec but builds none of them, so their slots are plain faceplate for now.

**Tech Stack:** JUCE 9.0.4, C++20, Catch2 tests in `tests/plugin/test_editor.cpp`, CMake presets `dev`, `asan`, `tsan`, lint preset, `scripts/validate.sh`.

**Spec:** `docs/superpowers/specs/2026-10-27-studio-console-design.md` (approved), mockup `docs/spec/visual/studio-console-mockup.html`.
The current `docs/spec/magic-dust-visual-spec.md` is rewritten section by section as the tasks land; behaviour it describes that this plan does not mention is unchanged.

## Global Constraints

- Window 1040 x 600 logical px, resizable 0.8x to 1.5x with a fixed aspect ratio (as today, new height).
- Colours, verbatim from the design: faceplate gradient `#6F7A86` to `#5D6773`; legends and ticks `#F3F1EA`; display gradient `#FBF0CF` (centre) to `#E5CF98` (edge) inside a 3 px `#26292D` bezel; ink `#2A2418`; lane caps chords `#7B6BD6`, bass `#D6527C`, melody `#3FB6C4`; amber LED on `#FFCF4A`, off `#4E4A3C`; sprinkle `#D84A3A` to `#A8301F`; keys `#F0ECE0` to `#C9C2AD` with a 2 px `#2C2F33` shadow below; walnut `#3D2516`, `#4B2E1B`, `#35200F`; knob body `#4B4B4B` to `#141414`.
- Focus ring: 2 px `#2A2418` on cream surfaces, 2 px `#F3F1EA` on metal.
- Panel legends are spaced capitals; chord symbols keep their case; accessible titles stay lowercase.
- Tagline is exactly "HOUSE MUSIC IDEA ENGINE".
- Fonts are OFL and bundled through the `MdFonts` binary data target.
- Vector drawing only, no bitmaps. No allocation, locks or blocking on the audio thread (UI-only work).
- No em dash anywhere. Long Markdown: one sentence per line.
- Commits: one logical change each, message says why, never a Co-Authored-By line or any agent name.
- Before each commit: dev tests, clang-format and the lint preset pass.

## Review Focus

1. Resizing to 0.8x and 1.5x: nothing clips, text stays inside its window, aspect ratio holds. Test: render at both scales in the `[.render]` case and assert child bounds stay inside the editor at 0.8x.
2. Host automation of mood, groove, flavour, a level or volume moves the knob without a click. Test in Task 1 (`Knob follows its parameter`).
3. Long text (a long pattern name, an error message) ends in an ellipsis inside its box, never overflowing the display. Test in Task 2.
4. Keyboard-only use: Tab reaches every built control in the design's order and arrows change knobs. Test in Task 3 (focus order) and Task 1 (arrows).
5. An error message replaces the display's bottom line and the readout returns once the error clears. Test in Task 2.

---

### Task 1: Knob, console tokens and the wordmark font

**Files:**
- Create: `ui/Knob.h`, `ui/Knob.cpp`
- Create: `ui/fonts/LibreBaskervilleBold.ttf`, `ui/fonts/OFL-LibreBaskerville.txt` (from the google/fonts repo, `ofl/librebaskerville`)
- Modify: `ui/Tokens.h` (add a `console` colour block with every Global Constraints colour, named `face-top`, `face-bottom`, `legend`, `display-centre`, `display-edge`, `bezel`, `ink`, `cap-chords`, `cap-bass`, `cap-melody`, `led-on`, `led-off`, `sprinkle-top`, `sprinkle-bottom`, `key-top`, `key-bottom`, `key-shadow`, `walnut-a/b/c`, `knob-light`, `knob-dark`, in the file's existing naming style)
- Modify: `ui/Fonts.h`, `ui/Fonts.cpp` (add `Face::serifBold`), `plugin/CMakeLists.txt` (font in `MdFonts` sources, `ui/Knob.cpp` in the ui sources)
- Test: `tests/plugin/test_editor.cpp`

**Interfaces:**
- Produces:
  - `md::ui::Knob(juce::AudioProcessorValueTreeState& params, const juce::String& paramId, KnobLook look)`, a `juce::Slider` subclass (`RotaryVerticalDrag`, `NoTextBox`, rotary range -135 to +135 degrees) that owns its `juce::SliderParameterAttachment`.
  - `struct KnobLook { juce::String legend; juce::StringArray rangeWords; juce::StringArray stepWords; juce::Colour cap; float diameter; std::optional<double> resetValue; };`, where `rangeWords` holds the two end words ("DARK", "BRIGHT") or is empty, `stepWords` holds one word per step for a stepped knob or is empty, `cap` is the centre cap colour, and `resetValue` sets the double-click reset.
  - `juce::Rectangle<int> Knob::faceBounds() const`: the knob circle inside the component, so layout can centre the circle exactly.
  - `Face::serifBold`.

- [ ] **Step 1: Write the failing tests** (in `test_editor.cpp`, tag `[knob]`)
  - `Knob follows its parameter`: build a processor, a `Knob` on `mood`; set the parameter to 0.75 normalised through `setValueNotifyingHost`; pump the message loop; `REQUIRE(knob.getValue() == Approx(75).margin(0.5))`.
  - `Knob writes its parameter`: `knob.setValue(20)`, `REQUIRE(params.getRawParameterValue("mood")->load() == Approx(20))`.
  - `Stepped knob steps once per 16 px`: `Knob` on `groove`; simulate a vertical drag of 16 px upward from the centre; `REQUIRE(groove index == 1)`; a further 32 px gives 3; dragging past the end stays at 3.
  - `Continuous knob covers its range in 160 px`: `mood` from 0, drag up 80 px gives 50 within 1; with shift held, 80 px gives 12.5 within 1.
  - `Arrow keys step a knob`: focus `flavour` at index 1, press up arrow, index 2; down twice, index 0.
  - `Double-click resets`: a `chordsLevel` knob with `resetValue = 1.0`, set to 0.3, double-click, value 1.0.
  - `Knob title and value text`: `mood` title "mood", value text "30"; `groove` value text "straight"; `chordsLevel` value text "100%".
  - `Serif face loads`: `font(Face::serifBold, 20).getTypefaceName()` is "Libre Baskerville".

- [ ] **Step 2: Run them and see them fail** (`ctest --preset dev -R knob`): fails to compile, `ui/Knob.h` missing.

- [ ] **Step 3: Implement `Knob`**
  - The interval is 1 for choice and int parameters. Stepped knobs use 16 px per step, so the drag sensitivity is 16 times the step count. Continuous knobs cover their full range in 160 px. Shift-drag is 4 times finer: override `mouseDrag` if the `Slider` velocity mode can't do it cleanly.
  - The wheel moves 1 step, or 5 units on a 0 to 100 knob, or 0.05 on a 0 to 1 knob.
  - `textFromValueFunction`:
    - choice parameters use the choice name,
    - 0 to 1 parameters use a rounded percent ("80%"),
    - int parameters use the number.
  - The title is the lowercase legend.
  - `paint` draws everything inside the component bounds, and the layout gives each knob a box big enough for all of it:
    - the legend above,
    - the tick scale over 270 degrees: 16 ticks on a continuous knob, or one tick per step with its step word just outside it on a stepped knob,
    - the black body with a radial gradient from `knob-light` to `knob-dark` and a shadow,
    - the cap disc at 52% of the diameter,
    - an off-white pointer,
    - range words under the two ends.

    Put the drawing in `Knob::paint` rather than a LookAndFeel, so it lives in one place.
  - The focus ring is drawn around the knob face while the knob has keyboard focus.

- [ ] **Step 4: Run the `[knob]` tests and the full dev suite: all pass.**

- [ ] **Step 5: Commit** `feat(ui): one knob control for the console look, with its tokens and wordmark font`.

---

### Task 2: Faceplate, header and display

**Files:**
- Create: `ui/Faceplate.h`, `ui/Faceplate.cpp`. It holds `paintFaceplate(juce::Graphics&, juce::Rectangle<float> bounds)`, which paints the metal face, the walnut cheeks and four screws, and is used by both views.
- Modify:
  - `plugin/PluginEditor.cpp`: the main `Body` is 1040 x 600, paints the faceplate, wordmark and tagline, and drops the grille and the old status-line text.
  - `ui/Tokens.h` layout: replace the main-view layout constants with the table below.
  - `ui/ScreenComponent.*` becomes the display. It paints the bezel and cream window and holds the chord cells, lanes and bottom line.
  - `ui/TopLine.*` becomes the header readouts.
  - `ui/ChordBubble.*` becomes chord cells.
  - `ui/LaneView.*`: ink notes, the label column and the selected and muted looks.
  - `ui/Readout.*` becomes the single bottom line.
- Delete: `ui/AuraPad.*` and `ui/StatusFields.*`, with their tests and the aura quadrant names.
- Test: `tests/plugin/test_editor.cpp`.
- Docs: in `docs/spec/magic-dust-visual-spec.md`, rewrite Layout, Colour tokens, Typography, Materials and components 1 to 9 for the console.

**Interfaces:**
- Consumes: `Knob` and `KnobLook` from Task 1.
- Produces:
  - `paintFaceplate`, used by Task 4's sender view.
  - Component IDs: `display`, `spellReadout`, `styleMenu`, `keyMenu`, `tempoReadout`, `cell0` to `cell3`, `lane0` to `lane2`, `roll0` to `roll2`, `bottomLine`, `moodKnob`, `energyKnob`, `grooveKnob`, `flavourKnob`.

**Layout** (logical px, window space, x, y, width, height):

| Element | Rect | Notes |
| --- | --- | --- |
| Cheeks | (0, 0, 26, 600) and (1014, 0, 26, 600) | Vertical walnut stripes 3, 4 and 2 px wide, repeating `walnut-a`, `walnut-b` and `walnut-c`, with an inner shadow 3 px wide on the face side. |
| Face | (26, 0, 988, 600) | |
| Screws | 13 px circles at (34, 8), (993, 8), (34, 579) and (993, 579) | Each has a radial gradient from `#E2E2E2` to `#6D6D6D` and a 2 px slot in `#444444` at 35 degrees. |
| Wordmark | x 54, centred on y 44 | "MAGIC DUST", `serifBold` 30 px, letter spacing 0.18 em, `legend`. |
| Tagline | 18 px after the wordmark, centred on y 44 | "HOUSE MUSIC IDEA ENGINE", Space Grotesk 500 11 px, spacing 0.22 em, `#DFE3E8`. |
| Readout legends | y 24, 12 px tall, centred over each window | "SPELL", "STYLE", "KEY" and "TEMPO", Space Grotesk 500 9.5 px, spacing 0.2 em, `legend`. |
| Spell window | (574, 38, 84, 26) | Shows the seed, for example "0417". Not clickable yet (the spells panel is Phase 5). |
| Style window | (670, 38, 124, 26) | Opens the style menu, as today. |
| Key window | (806, 38, 84, 26) | Opens the key menu, as today. Shows, for example, "A MIN". |
| Tempo window | (902, 38, 84, 26) | The host bpm with one decimal, or "124.0". Read-only. |
| Readout windows | | Cream gradient as for the display, a 2 px `bezel` border, a 2 px radius and an inner glow. Text is Space Mono Bold 15 px in `ink`, centred, and ends in an ellipsis if it doesn't fit. |
| Display bezel | (54, 86, 672, 306) | A 3 px `bezel` border, a 5 px radius, and the radial cream gradient inside. |
| Chord cells | x = 166 + i * 137, y 98, 137 x 48 | The cell is drawn inset by 4 px. The name is Space Mono Bold 14 px in `ink`, and the feel word is Space Mono 10 px in `ink` at 60%, below it. |
| Lane rows | x 62, y 154, 222 and 290, 656 x 64 | |
| Lane label column | x 66, 96 wide | The lane name is Space Grotesk 700 10 px, spacing 0.14 em, capitals. The pattern name sits below it in Space Mono 10 px, ending in an ellipsis. Both are `ink`. |
| Roll | x 166, row y + 4, 548 x 56 | Bar lines every bar, 1 px, `ink` at 18%. Notes are `ink` rectangles with a 1 px radius. The playhead is 1 px `ink`. |
| Bottom line | (66, 360, 648, 20) | Space Mono 11 px, `ink`, capitals except chord symbols, ending in an ellipsis. It shows "BAR 3 OF 8 - SELECTED: CHORD 2 - NOTHING PINNED", or "STOPPED - ..." when stopped. While there's an error, it shows the error in place of the readout. |
| Knob group | 2 x 2 in x 750 to 986, y 86 to 392 | Column centres at x 809 and 927, knob diameter 56. Row one has the legend at y 100 and the face at y 116 to 172. Row two has the legend at y 246 and the face at y 262 to 318. |
| Mood | top left | "MOOD", "DARK" and "BRIGHT", continuous. |
| Energy | top right | "ENERGY", "LOW" and "HIGH", continuous. |
| Groove | bottom left | "GROOVE", stepped, with step words "STR", "54", "58" and "62". |
| Flavour | bottom right | "FLAVOUR", stepped, with step words "SIMPLE", "SOULFUL" and "JAZZY". |

Chord cell states: pinned is an `ink` fill with cream text (Phase 5 sets it, so there's no trigger yet). Playing is an `ink` bar 3 px tall and 40 px wide, centred 4 px under the name. Selected is a 1.5 px `ink` outline with a 3 px radius. Focused is the cream focus ring.
Lane states: a selected row gets an `ink` band at 8% behind it. A muted row (Phase 5) is drawn at 35%.

- [ ] **Step 1: Rewrite the geometry and behaviour tests first.**
  - Replace the old positions (screen, top line, bubbles, lanes, rolls, aura pad, status fields, readout) with the table above. Use `boundsOf` and component IDs, and use `Knob::faceBounds()` for knob centres.
  - Editor opens at 1040 x 600.
  - Chord cell titles keep "chord 1, Am7, home".
  - The tempo window reads "124.0" with no host tempo.
  - Clicking the key window opens its menu, as the current test does.
  - `bottom line shows an error, then the readout`: set an error through the existing file-error path, refresh, and the text equals the error. Clear it, refresh, and the text starts with "STOPPED".
  - `long pattern name ellipsizes`: a lane whose pattern is 60 characters long draws inside its label column. Assert the label text bounds are no wider than 96.
  - Remove the aura pad and status field tests.
  - Keep and update the 4 px click versus 5 px drag tests and the drag-error test for cells and lanes.
- [ ] **Step 2: Run the tests and see them fail.**
- [ ] **Step 3: Implement.**
  - Moving the knobs replaces the aura pad and fields. The processor's existing debounce already regenerates 150 ms after mood, energy or flavour stop changing, so check that no extra code is needed.
- [ ] **Step 4: Check the build.** Run the full dev suite and the lint preset. Then run the `[.render]` editor case with `MD_RENDER_DIR` set to the SDD workspace's `renders/` folder.
- [ ] **Step 5: Commit** `feat(ui): console faceplate, header readouts and cream display`.

---

### Task 3: Bottom band: lane levels, keys and volume

**Files:**
- Modify:
  - `ui/LaneView.*`: remove the level slider.
  - `ui/MdLookAndFeel.h`: remove `drawLinearSlider`, and draw the popup menus in cream and ink with Space Mono 13 px.
  - `ui/KeyButton.*`: new key material.
  - `ui/SprinkleKey.*`
  - `plugin/PluginEditor.cpp`: place the bottom band, and add the three level knobs and the volume knob.
  - `ui/Tokens.h`
- Test: `tests/plugin/test_editor.cpp`.
- Docs: in the visual spec, rewrite components 10 to 16 and give the Phase 5 slots their positions.

**Interfaces:**
- Consumes: `Knob`, `paintFaceplate` and the Task 2 layout.
- Produces: component IDs `level0` to `level2` (now knobs), `volumeKnob`, `playKey`, `stopKey` and `sprinkleKey`.

**Layout:**

| Element | Rect or position | Notes |
| --- | --- | --- |
| Lane group panel | (54, 420, 372, 146) | Fill `#000000` at 12%, a 1 px inner line of `#FFFFFF` at 12%, and a 4 px radius. |
| Lane columns | x 54 + i * 124, 124 wide | The legends "CHORDS", "BASS" and "MELODY" are centred on y 436. |
| Pattern knob slot | centre (col + 38, 488), 44 px | Phase 5, so left empty. |
| Level knob | centre (col + 92, 488), 34 px | Legend "LEVEL", the lane's cap colour, reset 1.0, bound to `chordsLevel`, `bassLevel` or `melodyLevel`. |
| Keys | 36 x 24, top y 464 | The LED is 6 px, centred 6 px above the key. The legend sits below at y 496, in Space Grotesk 500 9.5 px, spacing 0.2 em. |
| Key x positions | | PLAY 450 and STOP 498. The Phase 5 slots are PIN 538, REROLL 586, MUTE 634, SWAP 682, KEEP 722 and BOTTLE IT 770. |
| Sprinkle | (818, 454, 84, 44) | The `sprinkle` gradient, with "SPRINKLE" in white inside it, Space Grotesk 700 10 px, spacing 0.18 em. Its LED sits above at y 442. |
| Volume | centre (950, 472), 52 px | "VOLUME", "MIN" and "MAX", reset 0.8, bound to `volume`. |

The key material uses the `key-top` to `key-bottom` gradient, with a 3 px radius and a 2 px `key-shadow` below. A pressed key moves down 1 px and its shadow drops to 1 px.
The play LED is amber and lit while the preview plays. Stop has no LED.
Keep the existing press animation timing.

- [ ] **Step 1: Write the tests.**
  - Update the key position tests.
  - `level knobs bind their lanes`: the three knobs carry the right parameter IDs, and setting `bassLevel` moves `level1`.
  - `volume knob binds volume`: double-clicking resets it to 0.8.
  - `focus order`: Tab from the wordmark goes through the readout windows (style, key), then cells, lanes, mood, energy, groove, flavour, level0 to level2, play, stop, sprinkle and volume. The wordmark joins the order in Task 4. Until then, start from the style window.
  - Keep the play, stop and sprinkle tests and the key press animation test.
  - Remove the level slider tests.
- [ ] **Step 2: Run the tests and see them fail.**
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Check the build.** Run the full dev suite and lint, then render the editor PNGs.
- [ ] **Step 5: Commit** `feat(ui): console bottom band with lane level and volume knobs`.

---

### Task 4: About panel, sender view and the final spec pass

**Files:**
- Create: `ui/AboutPanel.h`, `ui/AboutPanel.cpp`.
- Modify:
  - `plugin/PluginEditor.cpp`: the wordmark is a focusable button that opens the about panel.
  - `ui/SenderEditor.*`: console look.
  - `ui/Tokens.h`
- Delete: `ui/BodyMaterial.*`.
- Test: `tests/plugin/test_editor.cpp`.
- Docs:
  - In the visual spec, rewrite the Sender screen, About panel, Motion, Accessibility and JUCE component map sections.
  - In `docs/DECISIONS.md`, add rows for these:
    - panel text in capitals reverses the lowercase rule,
    - the grille, spectrum strip and aura names are removed,
    - two knobs replace the aura pad,
    - the window is now 1040 x 600,
    - display notes are drawn in ink.
  - In `docs/TESTING.md`, update the "Phase 2 in your daw" steps to the new control names.

**Interfaces:**
- Consumes: `paintFaceplate`, `KeyButton` and the `console` tokens.
- Produces: component IDs `wordmark`, `aboutPanel`, `aboutLink`, `senderKey-<lane>` and `senderMainKey`.

**About panel:**
- It covers the display's inner rect (57, 89, 666, 300) and uses the display's cream.
- Each line is centred, in this order:
  - "MAGIC DUST" in `serifBold` 26 px.
  - "version " + `JucePlugin_VersionString`, in Space Mono 12 px.
  - "Mark Stent Digital" in Space Grotesk 500 13 px.
  - "open source under AGPLv3" in Space Mono 11 px.
  - The link "github.com/markstent/magic_dust_vst" as a `juce::HyperlinkButton` to `https://github.com/markstent/magic_dust_vst`, in `ink`, Space Mono 11 px, underlined.
  - The text is `ink`, with 12 px between lines.
- It takes focus when it opens. A click outside the link or Escape closes it and gives focus back to the wordmark.
- The wordmark's title is "about magic dust".
- The panel's title is "about magic dust, version X.Y.Z, mark stent digital".

**Sender view** (520 x 240):
- The faceplate fills the whole window, with the cheeks at (0, 0, 26, 240) and (494, 0, 26, 240), and screws 8 px in from each corner of the face.
- "MAGIC DUST" is `serifBold` 20 px at x 50, centred on y 34, with the tagline "SENDER" 14 px after it.
- The keys follow the bottom band key material.
  - The lane keys are 72 x 24 at x 50, 134 and 218, with their top at y 104. Legends "CHORDS", "BASS" and "MELODY" sit below at y 136.
  - Each lane key's LED is in its lane cap colour, lit on the lane being sent.
  - The "FULL MAGIC DUST" key is 120 x 24 at x 322, with its legend below. It has no LED.
- The status window is (50, 176, 420, 28): cream readout window material, with Space Mono 11 px `ink` text that ends in an ellipsis. The status strings are unchanged.

- [ ] **Step 1: Write the tests.**
  - `wordmark opens about`: clicking it makes `aboutPanel` visible, and the panel text contains `JucePlugin_VersionString` and "Mark Stent Digital".
  - Escape closes the panel and focus returns to the wordmark.
  - The link URL equals the GitHub URL.
  - Update the sender geometry and LED tests to the new layout. The existing view swap tests still pass.
  - The focus order now starts at the wordmark.
  - `editor fits at 0.8x`: resize to 832 x 480, and every child's bounds, transformed, sit inside the editor.
- [ ] **Step 2: Run the tests and see them fail.**
- [ ] **Step 3: Implement**, and delete `BodyMaterial`.
- [ ] **Step 4: Check the build.**
  - Run the dev, asan and tsan suites, lint, clang-format and `scripts/validate.sh build/dev`.
  - Render the editor at 1x, 1.5x and 2x, and the sender at 1x and 2x, to the SDD `renders/` folder.
- [ ] **Step 5: Commit** `feat(ui): about panel with the version, and the sender view in the console look`.

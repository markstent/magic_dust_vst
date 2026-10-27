# Magic Dust: visual spec

Version 1.0, 26 October 2026.
Companion to `magic-dust-spec.md`, which covers the product, music engine, architecture, build plan, testing and release.

This document defines exactly how the Magic Dust plugin window looks and behaves.
The visual target is the studio console mockup, `visual/studio-console-mockup.html`.
Where this document and the mockup disagree, the mockup wins for appearance and this document wins for behaviour, and the disagreement goes in `docs/DECISIONS.md`.
The older reference HTML and PNGs in `visual/` are superseded; they stay only as history.

## Files

| File | What it is | How to use it |
| --- | --- | --- |
| `visual/studio-console-mockup.html` | The studio console look in plain HTML, CSS and JavaScript. | The visual target. Open it in a browser and read its CSS for exact values. |
| `visual/magic-dust-reference.html` | The earlier dark look, as a playable reference. | Superseded, history only. |
| `visual/magic-dust-ui.png`, `visual/magic-dust-ui-annotated.png`, `visual/magic-dust-ui-states.png` | Renders of the earlier dark look. | Superseded, history only. |

The note patterns in the mockup are illustrative placeholders.
In the plugin, every note on the screen is drawn from the engine's real output (see "Lanes").

## Rules for building the look in JUCE

- Draw everything as vectors in `paint()` and a custom `LookAndFeel`.
  Never paste the reference images into the plugin.
- The editor's logical size is 1040 x 600 px.
  It is resizable from 80% to 150% with a fixed aspect ratio, using `setResizable`, `setResizeLimits` and the constrainer's `setFixedAspectRatio`.
  Scale the whole component tree with one transform so every measurement in this document stays true at every size.
- Embed Space Grotesk (500 and 700), Space Mono (400 and 700) and Libre Baskerville Bold (the wordmark) as binary data and load them with `juce::Typeface::createSystemTypefaceFor`.
  All of them use the SIL Open Font License; ship the licence text with the plugin.
- Use shading only where this document lists it (see "Materials").
  Everything else is flat.
- Match positions and sizes to within 1 px at 100% scale.
  The UI snapshot tests compare the JUCE editor with approved renders of the JUCE editor itself; use the mockup for the human side-by-side review, because browser and JUCE text rendering differ slightly.
- Panel legends and readouts are spaced capitals, and chord symbols such as `Am9` keep their case.
  Accessible titles stay lowercase.
- Never use the em dash character in any interface text.

## Layout

All coordinates are in logical pixels, measured from the top-left corner of the window.
The look is the studio console in `visual/studio-console-mockup.html`; the numbers below are the approved values.

### Zones

| Zone | x | y | Width | Height | Notes |
| --- | --- | --- | --- | --- | --- |
| Window | 0 | 0 | 1040 | 600 | |
| Cheeks | 0 and 1014 | 0 | 26 | 600 | Walnut stripes |
| Face | 26 | 0 | 988 | 600 | Metal gradient |
| Header | 54 | 24 | 932 | 40 | Wordmark and tagline on the left, readout windows on the right |
| Display bezel | 54 | 86 | 672 | 306 | |
| Knob group | 750 | 86 | 236 | 306 | A 2 x 2 group |

### Positions inside the zones

| Element | Position and size |
| --- | --- |
| Screws | 13 px circles at (34, 8), (993, 8), (34, 579) and (993, 579). |
| Wordmark | Starts at x = 54, centred on y = 44. The tagline starts 18 px after it. |
| Readout legends | y = 24, 12 px tall, centred over each window. |
| Idea, style, key and tempo windows | At y = 38, 26 px tall: x = 574 (84 wide), 670 (124), 806 (84) and 902 (84). |
| Chord cells | 137 x 48 px at x = 166 + i * 137, y = 98. The cell is drawn inset by 4 px. |
| Lane rows | 656 x 64 px at x = 62, y = 154, 222 and 290. The label column starts at x = 66 and is 96 px wide. The piano roll is 548 x 56 px at x = 166, 4 px below the row's top. |
| Bottom line | 648 x 20 px at (66, 360). |
| Knobs | Diameter 56 px. Column centres at x = 809 and 927. Faces at y = 144 to 200 (mood, energy) and y = 290 to 346 (groove, flavour), which centres the block of legends, ticks and words in the group box. |
| Keys | Play at (560, 492) and stop at (620, 492), 48 x 32 px, in a transport panel at (450, 420, 448, 146). Sprinkle at (692, 484), 96 x 48 px. Each lane column also has a mute key at (column x + 72, 496), 36 x 24 px. |
| Spells panel | 340 px wide, 14 px from the display's left inner edge and 40 px below its top inner edge. |
| Toast | Centred horizontally on the display, 96 px above its bottom inner edge. |

## Colour tokens

Use these names in code (the `console` block of `ui/Tokens.h`).

| Token | Value | Used for |
| --- | --- | --- |
| `face-top`, `face-bottom` | `#6F7A86`, `#5D6773` | Faceplate gradient |
| `legend` | `#F3F1EA` | Legends, tick scales, the wordmark, focus ring on metal |
| `display-centre`, `display-edge` | `#FBF0CF`, `#E5CF98` | Display and readout window gradient |
| `bezel` | `#26292D` | Display and readout window border |
| `ink` | `#2A2418` | Text, notes, bar lines, playhead and focus ring on cream |
| `cap-chords`, `cap-bass`, `cap-melody` | `#7B6BD6`, `#D6527C`, `#3FB6C4` | Lane knob caps and pinned LEDs |
| `led-on`, `led-off` | `#FFCF4A`, `#4E4A3C` | Amber LEDs |
| `sprinkle-top`, `sprinkle-bottom` | `#D84A3A`, `#A8301F` | Sprinkle key |
| `key-top`, `key-bottom`, `key-shadow` | `#F0ECE0`, `#C9C2AD`, `#2C2F33` | Other keys, with a 2 px shadow below |
| `walnut-a`, `walnut-b`, `walnut-c` | `#3D2516`, `#4B2E1B`, `#35200F` | Cheek stripes |
| `knob-light`, `knob-dark` | `#4B4B4B`, `#141414` | Knob body gradient |
| `knob-cap` | `#8A8F96` | Cap of the mood, energy, groove, flavour and volume knobs |
| `display-glow` | `#AA6E1E` at 45% | Inner glow of the display and the about panel |
| `readout-glow` | `#FFBE5A` at 60% | Inner glow of the readout windows and the sender status window |

Other fixed colours: the tagline is `#DFE3E8`, and the screws run from `#E2E2E2` to `#6D6D6D` with a `#444444` slot.

## Typography

| Role | Font | Size | Weight | Colour |
| --- | --- | --- | --- | --- |
| Wordmark "MAGIC DUST" | Libre Baskerville | 30 px, letter spacing 0.16 em | Bold | `legend` |
| Tagline "HOUSE MUSIC IDEA ENGINE" | Space Grotesk | 11 px, letter spacing 0.18 em | 500 | `#DFE3E8` |
| Readout legends | Space Grotesk | 9.5 px, letter spacing 0.2 em | 500 | `legend` |
| Readout windows | Space Mono | 15 px | 700 | `ink` |
| Chord name in a cell | Space Mono | 14 px | 700 | `ink` |
| Feel word in a cell | Space Mono | 10 px | 400 | `ink` at 60% |
| Lane name | Space Grotesk | 10 px, letter spacing 0.14 em, capitals | 700 | `ink` |
| Pattern name | Space Mono | 10 px, capitals | 400 | `ink` |
| Bottom line | Space Mono | 11 px | 400 | `ink` |
| Knob legend | Space Grotesk | 10 px, letter spacing 0.12 em | 700 | `legend` |
| Knob words | Space Grotesk | 8 px | 500 | `legend` |
| Key label | Space Grotesk | 9.5 px, letter spacing 0.2 em, capitals | 500 | `legend` |
| Sprinkle title | Space Grotesk | 10 px, letter spacing 0.18 em | 700 | white |
| Spells panel and toast | Space Mono | 12 px | 400 and 700 | Set in the spells task |

Text that does not fit its box ends in an ellipsis.
The tagline ends at least 16 px before the spell window (x = 574), which is why its spacing and the wordmark's are slightly tighter than the mockup.

## Materials

These are the only places shading is allowed.

| Material | Recipe |
| --- | --- |
| Faceplate | Vertical gradient `face-top` to `face-bottom` over the face (x = 26 to 1014). |
| Walnut cheeks | Vertical stripes 3, 4 and 2 px wide, repeating `walnut-a`, `walnut-b` and `walnut-c`, with a 3 px inner shadow (black at 50% fading to clear) on the face side. |
| Screws | Radial gradient from `#E2E2E2` to `#6D6D6D`, highlight at 35% across and 30% down, with a 2 px slot in `#444444` at 35 degrees. |
| Cream window | A `bezel` border (3 px on the display, 2 px on readout windows), a radial cream gradient from `display-centre` to `display-edge` inside it, and an inner glow along the inside edge. The display has a 5 px radius and the readout windows 2 px. |
| Knob | See the knob control in `ui/Knob.cpp`: black body with a radial gradient from `knob-light` to `knob-dark`, a cap disc at 52% of the diameter, and an off-white pointer. |
| Key | 36 x 24 px, 3 px radius (sprinkle 84 x 44, 5 px), vertical gradient and a hard `key-shadow` below. See "Keys" and "Sprinkle". |
| LED | 6 px circle. Off: `led-off`. On: the LED's colour plus a 6 px glow in the same colour. |

## Components

The numbers match the order of the console's parts, left to right and top to bottom.

### 1. Wordmark

- "MAGIC DUST" in the serif face, with the tagline "HOUSE MUSIC IDEA ENGINE" after it.
- A button that opens the about panel (see "About panel"), with the title "about magic dust".

### 2. Header readouts

- Four cream windows, each with a legend above: "IDEA", "STYLE", "KEY" and "TEMPO".
- Idea shows the idea's seed as four digits, for example "0417", with the accessible title "idea, 0417".
  It is not clickable yet.
- Style and key are buttons that open menus.
  Key reads as, for example, "A MIN".
- Tempo shows the host tempo with one decimal, or "124.0" when the host sends none.
  It is read-only.

### 3. Chord cells

| State | Look |
| --- | --- |
| Default | Name in `ink`, feel word below it in `ink` at 60% |
| Playing | A 3 px tall, 40 px wide `ink` bar centred at the bottom of the cell, under the name and the feel word |
| Selected | A 1.5 px `ink` outline, 3 px radius |
| Focused | A 2 px `ink` focus ring just outside the cell |

- A pinned cell (an `ink` fill with cream text) arrives with pinning.
- Click: select the cell and audition its chord.
- Drag (more than 4 px of movement): drag that chord into your daw as a MIDI clip.

### 4. Lanes

- Three rows: chords, bass and melody.
- Each row has a 96 px label column, with the name and pattern centred in it, and a 548 x 56 px piano roll with a 1 px bar line at every bar in `ink` at 18%, and a 1 px `ink` playhead while playing.
- The label shows the lane name in capitals, with the pattern name below it, ending in an ellipsis if it is too long.
- Notes are `ink` rectangles with a 1 px radius.
  Horizontal position and width come from the note's start and length across the 8 bars.
  Vertical position comes from pitch, with the highest pitch at the top: chords span MIDI 48 to 79, bass 28 to 48, and melody 60 to 84.
- The lane's level is a knob in the lane group below the display (components 8 to 10), not in the row.
- Selected: an `ink` band at 8% behind the row.
- Muted: the whole row is drawn at 35% while the lane's mute key is on.
- Click: select the lane.
  Drag (more than 4 px of movement): drag the lane into your daw as a MIDI clip.

### 5. Mood and energy knobs

- Continuous knobs from 0 to 100, "MOOD" (dark to bright) and "ENERGY" (low to high), on the mood and energy parameters.
- Unpinned parts regenerate 150 ms after the value stops changing.

### 6. Groove and flavour knobs

- Stepped knobs with one printed word per position.
- "GROOVE": "STRAIGHT", "54", "58" and "62".
- "FLAVOUR": "SIMPLE", "SOULFUL" and "JAZZY".
  Changing it regenerates unpinned chords.

### 7. Bottom line

- One line along the bottom of the display: "BAR 3 OF 8 - SELECTED: CHORD 2 - NOTHING PINNED", or "STOPPED - ..." when stopped.
- While there is an error, the line shows the error instead, and the readout returns once it clears.

### 8. Lane group

- A panel at (54, 420, 372, 146): `#000000` at 12%, a 1 px inner line of `#FFFFFF` at 12%, and a 4 px radius.
- Three columns, at x = 54 + i * 124 and 124 px wide, for chords, bass and melody.
- The legends "CHORDS", "BASS" and "MELODY" are centred in their column on y 456, in Space Grotesk 700 10 px.
- Each column has a pattern knob slot, a level knob and a mute key.

### 9. Pattern knob slots (Phase 5)

- Centre at (column x + 38, 508), 44 px across.
- Left empty until Phase 5: draw nothing, so the faceplate shows through.

### 10. Lane level knobs

- Centre at (column x + 42, 508), 34 px across, with the legend "LEVEL", under the lane legend.
- Phase 5 moves it to column x + 92, beside the pattern knob at column x + 38.
- The cap is the lane's colour, and the knob is bound to `chordsLevel`, `bassLevel` or `melodyLevel`.
- Double-click resets to 100%.
- Accessible titles are "chords level", "bass level" and "melody level".

### 10a. Lane mute keys

- Each column has a MUTE key at x = column x + 72, 36 x 24 px, vertically centred on y 508, in the cream key material (3 px radius, 2 px shadow).
- An amber LED 6 px across sits centred 6 px above the key and is lit while the lane is muted.
- The legend "MUTE" is below the key, in the key legend style.
- Click, or Enter while focused, toggles `chordsMute`, `bassMute` or `melodyMute`.
- Accessible titles are "chords mute", "bass mute" and "melody mute".
- A muted lane plays nothing: no built-in sound and no MIDI out.
  A sender follows the main instance, so it goes quiet too.
- Each level knob is followed by its mute key in the focus order.

### 11. Volume

- A knob centred at (950, 508), 52 px across, like the lane level knobs, with the legend "VOLUME" centred on y 456 like the lane legends, and the end words "MIN" and "MAX".
- Bound to `volume`; double-click resets to 80%.

### 12. Transport panel and keys

- A panel at (450, 420, 448, 146), drawn like the lane group, so it sits level with it.
- Play, stop and sprinkle are one cluster, centred in the panel (centre x 674) and on y 508.
  The cluster runs from x 560 to 788.
- Play and stop are 48 x 32 px, 12 px apart: play at (560, 492) and stop at (620, 492).
- Face: the `key-top` to `key-bottom` gradient with a 3 px radius, and a 2 px `key-shadow` below.
- A pressed key moves down 1 px and its shadow drops to 1 px.
- The LED is 6 px, centred over the key with 6 px between them.
  Amber (`led-on`) when lit, `led-off` when not.
- The legend sits 8 px below the key, in capitals, Space Grotesk 500 9.5 px with 0.2 em spacing.
- Play's LED is lit while the preview plays.
- Stop has no LED.
- Neither key has an icon, because none reads cleanly at this size.
- Space does what play does when the preview is stopped, and what stop does when it plays.
- While the daw's transport plays, Magic Dust follows it and a plugin cannot drive the daw's transport.
  Play, stop and Space then leave the daw alone and replace the bottom line with "FOLLOWING YOUR DAW - USE ITS TRANSPORT" for 2 seconds, after which the readout returns.

### 13. Sprinkle

- At (692, 484, 96, 48), 24 px right of stop, centred on y 508, with a 5 px radius.
- Face: the `sprinkle-top` to `sprinkle-bottom` gradient, and a 3 px `key-shadow` below.
- "SPRINKLE" in white inside it, Space Grotesk 700 10 px with 0.18 em spacing.
- It has no LED.
- Writes a new idea, keeping everything pinned, and adds it to the spells list (8 most recent).

### 14. Phase 5 keys

These keys are not built yet.
Phase 5 adds pin, reroll, swap, keep and bottle it inside the transport panel and re-lays it out.
They use the key material.

- Pin, reroll and swap act on the current selection (a chord cell or a lane), as in the table below.
- Keep adds or removes the idea from favourites, with the toast "kept in favourites" or "removed from favourites".
- Bottle it: drag from the key to drop the whole idea into your daw.
  A plain click shows the toast "drag me into your daw: chords, bass and melody".

| Key | On a chord | On a lane | LED |
| --- | --- | --- | --- |
| pin | Pins or unpins the chord | Pins or unpins the lane | lit while the selected item is pinned |
| reroll | Picks a different chord for that slot; unpinned bass and chord lanes follow | Rewrites the lane with a new seed | None |
| swap | Steps to the next ranked alternative; unpinned bass and chord lanes follow | Toast: "select a chord cell to swap it" | None |

Rerolling or swapping a pinned item changes nothing and shows "unpin it first" (reroll) or "unpin the chord to swap it" (swap).

### Sender screen

- Shown by any Magic Dust instance that sends one lane to another track (see `docs/DECISIONS.md`).
  It is 520 x 240 px and scales and resizes like the main window.
- The faceplate fills the whole window: walnut cheeks at (0, 0, 26, 240) and (494, 0, 26, 240), and screws 8 px in from each corner of the face.
- "MAGIC DUST" in the serif face, 20 px, at x 50 and centred on y 34, with the tagline "SENDER" (14 px) 14 px after it.
- Keys in the console key material, 72 x 24 px at x 50, 134 and 218, with their top at y 104.
  Their legends "CHORDS", "BASS" and "MELODY" sit below at y 136.
  Each has an LED above it in its lane cap colour (chords `cap-chords`, bass `cap-bass`, melody `cap-melody`), lit on the lane being sent.
  Clicking a key selects that lane.
- A fourth key, "FULL MAGIC DUST", 120 x 24 px at x 322, with its legend below and no LED.
  Clicking it turns the instance into a full Magic Dust and the window becomes the main window.
- The status window at (50, 176, 420, 28) uses the readout window material, with Space Mono 10 px `ink` text in capitals, 10 px in from the bezel's outer edge, that ends in an ellipsis if it is too long: "SENDING BASS FROM MAGIC DUST" (the chosen lane), or "NO MAGIC DUST FOUND - LOAD MAGIC DUST ON ANOTHER TRACK FIRST" when there is no main instance to follow.
  Its accessible text stays lowercase.

### About panel

- Opened by clicking the wordmark or pressing Enter on it.
  It is a child of the main view, over the display's inner rect (57, 89, 666, 300), in the display's cream.
- Centred lines, in `ink`, 12 px apart:
  - "MAGIC DUST" in the serif face, 26 px,
  - "version " and the plugin's version, Space Mono 12 px,
  - "Mark Stent Digital", Space Grotesk 500, 13 px,
  - "open source under AGPLv3", Space Mono 11 px,
  - the link "github.com/markstent/magic_dust_vst", underlined, Space Mono 11 px, which opens `https://github.com/markstent/magic_dust_vst`.
- The panel takes the keyboard focus when it opens.
  Escape, a click on the panel (except on the link) or a click or focus move anywhere else in the editor closes it, and Escape or a click on the panel gives focus back to the wordmark.
- Its title is "about magic dust, version X.Y.Z, mark stent digital".

### Spells panel

- Opens below the spell number, over the chord cells.
- Background `screen-panel`, border 1 px `screen-line`, corner radius 12 px, 8 px padding.
- Title "recent spells", then one row per spell, newest first: the seed, the four chord names, and "kept" if it is a favourite.
  The current spell's row has a `spell-current` background.
- Clicking a row restores that idea and closes the panel.
  Clicking the spell number again or pressing Escape closes it.

### Toast

- A `gold` pill with `on-colour` text, 6 px by 14 px padding, fully rounded ends.
- Appears for 1.7 seconds; a new toast replaces the current one.

## Motion

| Element | Trigger | Animation | Duration and easing |
| --- | --- | --- | --- |
| Notes | A lane is regenerated | Each note fades in while scaling 0.1, then 1.3, then 1. Each note's delay is its horizontal position in percent times 3.2 ms, capped at 320 ms, so the dust sweeps left to right. | 420 ms, cubic-bezier(0.2, 0.8, 0.2, 1) |
| Chord name | A chord changes | Fades in while scaling 0.5, then 1.12, then 1 | 380 ms, ease-out |
| Key | Pressed | Moves down 1 px; the base shadow shrinks | 80 ms, ease-out |
| LED | Turns on or off | Colour and glow fade | 120 ms |

Pinned parts never animate when other parts regenerate.
A "motion" setting turns every animation off, and the plugin also turns them off when macOS "Reduce motion" is on.

## Accessibility

- Every interactive element is keyboard-focusable in this order: the wordmark, the style and key windows, the chord cells, the lanes, the mood, energy, groove and flavour knobs, the three level knobs each followed by its mute key, play, stop, sprinkle, then the volume knob.
  The sender view's order is its three lane keys, then "full magic dust".
- Focus ring: 2 px `ink` on cream surfaces, 2 px `legend` on the metal faceplate.
- Every control has an accessible title through `juce::AccessibilityHandler`, for example "chord 1, Am9, home", "mood", "chords level", "play the preview" and "about magic dust".
  Knobs also report their value.
- Arrow keys step a knob; the wordmark, keys, cells and lanes answer Enter.
- Space plays and stops the preview whichever control has the focus, and a click anywhere in the editor gives it the keyboard focus so Space works straight away.
  The editor does not take the focus when it opens.
- Text contrast is at least 4.5:1 against its background.
- Colour never carries meaning alone: lanes, knobs and LEDs always sit next to a text label.

## JUCE component map

| Reference class | JUCE component | Notes |
| --- | --- | --- |
| `md-body` | `MagicDustEditor` (the `AudioProcessorEditor`) | Owns the scale transform; its main view paints the faceplate (`paintFaceplate`) |
| `md-screen` | `ScreenComponent` | The display: paints the bezel and cream window; owns the cells, lanes and bottom line |
| `md-topline` | `TopLine` | Four `ReadoutWindow`s: spell, style, key and tempo |
| `md-cell` | `ChordCell` (x4) | Custom `juce::Button`; starts an external drag after 4 px of movement |
| `md-lane` | `LaneView` (x3) | Custom component with a `PianoRoll` child; starts an external drag after 4 px of movement |
| `md-knob` | `Knob` (x8: mood, energy, groove, flavour, three level knobs and volume) | Subclass of `juce::Slider` in `RotaryVerticalDrag` mode with custom drawing |
| `md-readout` | `Readout` | The display's bottom line |
| `md-spells` | `SpellsPanel` | Child of the screen, not a separate window |
| `md-toast` | `Toast` | Child of the screen; hidden by a timer |
| `md-key` | `KeyButton` | Custom `juce::Button` with an optional LED; the sender view uses it for its lane keys and the full key |
| `md-wordmark` | `WordmarkButton` | A `juce::Button` over the wordmark; draws only its focus ring |
| `md-about` | `AboutPanel` | Child of the main view over the display; holds a `juce::HyperlinkButton` |
| `md-key--gold` | `SprinkleKey` | |
| `md-key--drag` | `BottleKey` | Starts an external drag of the multi-track MIDI file |

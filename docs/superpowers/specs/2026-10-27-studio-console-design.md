# Studio console look - design

Date: 2026-10-27.
Status: approved by the owner on 2026-10-27.

## Intent

The owner finds the current Magic Dust screen looks like "AI slop": soft, glowy and decorated, with too many colours.
They want it clean and streamlined, and above all it must look like hardware, like a solid piece of studio gear.
The reference they gave is Soundtoys EchoBoy: a black faceplate, wooden cheeks, white spaced capitals, red LED readouts and real knobs.
Shown three hardware directions, they picked **C, the studio console**, with no changes.
The approved mockup is `docs/spec/visual/studio-console-mockup.html`.

This replaces the look in `docs/spec/magic-dust-visual-spec.md`.
It does not change what any control does unless this document says so.

## Goals

- It reads as one piece of studio hardware: a desk-metal faceplate with wooden cheeks and screws.
- Calm and uncluttered: few colours, each with one job, and every control with one clear label.
- Every function of the current screen survives, laid out so a producer finds it without thinking.
- The plugin shows the company "Mark Stent Digital" and has an about panel with the version.

## Non-goals

- No change to the engine, the ideas it writes, the MIDI, sender mode or the saved state format.
- No new features beyond the about panel.
- No new flavours (the owner parked that).

## Anti-goals

- Neon glows, glassy gradients, sparkles and gloss: anything that reads as a generic app skin.
- A colour per element. Colour lives on the lane knob caps, the LEDs and the sprinkle key only.
- Fake detail that adds noise: no speaker grille, no decorative spectrum strips.

## The look (owner's choice, from the mockup)

- **Faceplate:** grey-blue desk metal, a vertical gradient from `#6F7A86` to `#5D6773`, with a screw in each corner.
- **Cheeks:** walnut wood on the left and right edges, 26 px wide.
- **Wordmark:** "MAGIC DUST" in a serif, bold, widely spaced capitals, in off-white `#F3F1EA`.
- **Tagline:** "HOUSE MUSIC IDEA ENGINE" beside the wordmark in small spaced capitals (the owner's exact words).
- **Legends:** small spaced capitals in off-white, printed above each knob and below each key, with range words at the knob's ends (for example "DARK" and "BRIGHT").
- **Display:** one warm, cream, backlit window like a VU meter (a radial gradient from `#FBF0CF` to `#E5CF98` inside a dark `#26292D` bezel).
  Everything on it is drawn in dark ink `#2A2418`.
- **Readouts:** small cream backlit windows in the top row for style, key and tempo, matching the display.
- **Knobs:** black knobs with an off-white pointer and an off-white tick scale over 270 degrees.
  The lane knobs have coloured centre caps: chords violet, bass rose and melody teal (the lane colours, muted to sit on metal).
  The other knobs have grey caps.
- **Keys:** square cream keys with a small amber LED above each.
- **Sprinkle:** the one red key, larger than the rest.

## Layout

Top to bottom, in three bands, as in the mockup:

1. **Header:** wordmark and tagline on the left. On the right are the readout windows: spell, style, key and tempo, each with a legend above it.
2. **Middle:** the display window on the left. On the right is a 2 x 2 group of knobs: mood, energy, groove and flavour.
3. **Bottom:** the lane group (a pattern knob and a level knob for each lane), then the keys, then volume at the far right.

## Where each current function goes

| Today | In the console |
| --- | --- |
| Wordmark | Wordmark. Clicking it opens the about panel. |
| Speaker grille | Removed. |
| Volume knob | Volume knob, bottom right, "MIN" to "MAX". Same behaviour. |
| Spell number and spells panel | "SPELL" readout window. Clicking it opens the spells list, drawn in the display's cream and ink. |
| Key and style menus | "KEY" and "STYLE" readout windows. Clicking one opens its menu, as today. |
| Tempo | "TEMPO" readout window. It shows the daw's tempo and can't be edited. |
| Chord bubbles | Four chord cells across the top of the display, each with the chord name and its feel word. |
| Lanes | Three rows in the display. Each label shows the lane name with the pattern name under it. |
| Lane level sliders | A small level knob beside each lane's pattern knob. Double-click resets it to full. |
| Aura pad and aura encoder | Two knobs, "MOOD" (dark to bright) and "ENERGY" (low to high), on the same parameters. |
| Groove and flavour fields | Stepped knobs with a printed legend at each position. |
| Readout (bar, selection, pins) | One line along the bottom of the display. |
| Toasts | Shown on that same line for 2 s, then the line returns. |
| Status line (sync and midi LEDs) | Two small LEDs, "SYNC" and "MIDI", beside the tempo window. |
| Play, stop, sprinkle, pin, reroll, mute, swap, keep and bottle it | Keys in the bottom band. |
| Sender screen | The same faceplate, cheeks and keys at 520 x 240 px. |

## Behaviour details

- **Chord cells:** chords pinned, playing, selected and focused all stay distinct.
  - Pinned is inverted: an ink fill with cream text.
  - Playing is a short ink bar under the cell.
  - Selected is an ink outline.
  - Focused is the standard focus ring.
  - Click to audition and drag to export, as today.
- **Lane rows:** a selected row gets a slightly darker cream band. A muted row is drawn at 35% ink. The playhead is a 1 px ink line.
  Notes are ink rectangles. Lanes are told apart by their row and label, and colour stays on the knob caps.
- **Lane pattern knobs:**
  - They are stepped, one step per pattern.
  - A push pins the lane, and the LED beside the knob lights in the lane colour.
  - Turning a pinned knob shows the toast, as today.
- **Groove knob:** 4 positions: "STRAIGHT", "54", "58" and "62".
- **Flavour knob:** 3 positions: "SIMPLE", "SOULFUL" and "JAZZY".
- **Mood and energy knobs:** continuous from 0 to 100.
  Releasing either one regenerates unpinned parts after 150 ms, as today.
- **About panel:** clicking the wordmark lays a cream panel over the display. It shows:
  - "MAGIC DUST"
  - "version X.Y.Z", from the single version source (CMake `project(VERSION)` through `JucePlugin_VersionString`)
  - "Mark Stent Digital"
  - "open source under AGPLv3"
  - the GitHub link, which opens in the browser

  A click anywhere on the panel or Escape closes it.
- **Company name:** the plugin is published as "Mark Stent Digital" (already done in `plugin/CMakeLists.txt`).

## Exact values (from the approved mockup)

| Thing | Value |
| --- | --- |
| Lane caps and pinned LEDs | chords `#7B6BD6`, bass `#D6527C`, melody `#3FB6C4` |
| Amber LED | on `#FFCF4A`, off `#4E4A3C` |
| Sprinkle key | gradient `#D84A3A` to `#A8301F` |
| Other keys | gradient `#F0ECE0` to `#C9C2AD`, with a 2 px `#2C2F33` shadow below |
| Walnut cheeks | stripes of `#3D2516`, `#4B2E1B` and `#35200F` |
| Legends and tick scales | `#F3F1EA` |
| Knob body | black, from `#4B4B4B` to `#141414` |

## Interaction (my calls, matching today's controls)

- **Knob drag:** knobs turn with a vertical drag.
  - Stepped knobs move one step per 16 px.
  - Continuous knobs cover their full range in 160 px, and holding shift makes them four times finer.
  - The scroll wheel moves one step, or 5 on a continuous knob.
  - Double-click resets a level or volume knob.
- **Knob keyboard:** arrows step the value, and Enter or Space pins a lane knob.
- **Regenerating:** mood and energy regenerate unpinned parts on mouse-up, or 150 ms after the last key or wheel step.
  Dropping the 2D aura pad for two knobs is intended, because it comes with the console the owner chose.
- **Lane pattern knobs:**
  - They have one tick per pattern rather than a continuous scale.
  - The display's lane label shows the current pattern name.
  - A pinned lane has two cues: its LED and an inverted lane label in the display.
- **Focus order:**
  1. wordmark (about)
  2. spell, style and key
  3. chord cells and lanes
  4. mood, energy, groove and flavour
  5. each lane's pattern knob, then its level knob
  6. keys, from left to right
  7. volume

  The focus ring is 2 px ink `#2A2418` on cream surfaces and `#F3F1EA` on metal.
  The spells list and the about panel take focus when they open. Escape closes them and puts focus back.
- **Menus:** the style and key menus stay JUCE popups, drawn in cream and ink through `MdLookAndFeel`.
- **Long text:** text that doesn't fit ends in an ellipsis. The about panel covers the bottom line while it is open.
- **Tempo window:** read-only. It shows the daw's tempo, or 124.0 when the daw has sent none, matching the preview.
- **SYNC and MIDI LEDs:** each LED has its legend printed beside it.

## Constraints

- These all stay as they are: JUCE vector drawing, the existing scaling and resizing, the keyboard focus order, accessible titles, and the rule that no colour carries meaning alone (see the current visual spec).
- Fonts must be OFL and bundled, like the current ones.
- No allocation, locks or blocking on the audio thread; this is a UI-only change.
- Each component keeps its current tests, updated for the new geometry, and the editor gets a snapshot that is compared by eye against the mockup.

## My calls (please check)

- **Text in capitals:** panel text is in capitals, as in the mockup, which reverses the "all interface text is lowercase" rule.
  Accessible titles stay lowercase.
- **Removed:** the speaker grille, the decorative spectrum strip and the "aura" quadrant word (midnight rave and the rest).
- **Window size:** 1040 x 600 px, in place of 1040 x 740, because the console is wider than it is tall.
  The builder may grow the height to 640 if the display's lane rows would drop below 52 px.
  It still scales like today.
- **Ink:** notes in the display are dark ink, not lane colours.
- **Messages:** toasts and the bar, selection and pin readout share the bottom line of the display.
- **Wordmark font:** an OFL serif, picked by the builder and shown to you in the first screenshot.

## Left to the builder

- Exact pixel positions inside each band, worked out from the mockup and recorded in the rewritten visual spec.
- The serif face for the wordmark, from OFL fonts.
- How the knob and key materials are drawn, as long as they match the mockup at 1x and 2x.

## Layout revision: groovebox (owner choice, 2026-10-27)

The owner found the bottom band sparse after the transport panel change and asked for new layouts.
Shown three (mixer strips, groovebox, compact rack), they picked **B, the groovebox**, as is.
The mockup is `docs/spec/visual/groovebox-layouts-mockup.html`, option B.
The console materials, colours and fonts above are unchanged; only the arrangement changes.

- The display runs the full content width under the header.
- One knob panel sits under the display: mood, energy, groove and flavour, a divider, the three lane level knobs, a divider, then volume.
- A row of big keys runs along the bottom: three mute keys (chords, bass, melody) under a vertical "MUTE" legend on the left, and PLAY, STOP and a wide SPRINKLE on the right.
- PLAY and STOP are dark keys with play and stop icons; SPRINKLE is the red key with "NEW IDEA" under it.
- The gap between the mute keys and the transport keys is where Phase 5's pin, reroll, swap, keep and bottle it will go.

# LED above MUTE and PIN keys never lights

## Root cause
`KeyButton` invalidated its LED, shadow and legend with `Component::repaint(rect)` on a rectangle expanded beyond its bounds.
JUCE's `Component::internalRepaint` intersects that rectangle with the component's local bounds, so the LED strip above the key was never marked dirty and a host window never redrew it.
Offscreen snapshots repaint everything, which is why the existing tests (which read `isLit()`) passed.

## Reproduction
- Hit test (`getComponentAt` on each key centre): passed, no overlay.
- Click path, `getToggleState`, parameter and `isLit()` with the animation on, host set, editor reopen: all passed. In JUCE 9.0.4 `ButtonParameterAttachment::setValue` uses sendNotificationSync, so `clicked()` does run.
- Painted LED pixel in a full snapshot after the animation: passed (full repaint hides the bug).
- Dirty-region test (editor with a recording CachedComponentImage, real click, animation pumped): FAILED, the LED rect was never invalidated.
- Standalone end to end: blocked, screencapture and cliclick lack Screen Recording and Accessibility permission.

## Fix
`KeyButton::repaintWithMargin` invalidates through the parent, so the margin around the key is repainted.

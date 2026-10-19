# Phase Distorter 0.5

## macOS

- Add a universal macOS application for Apple silicon and Intel, with SDL2
  included. Requires macOS 11 or later and uses the native OpenGL framework.
- The app is ad-hoc signed, without Apple Developer ID signing or notarization.
  macOS may require Privacy & Security > Open Anyway after the first attempt.
- Fix CRT phosphor stripe selection on macOS OpenGL while preserving the
  intended RGB mask pattern on other platforms.

## Display and frame pacing

- Add exact MacBook Air 13/15 and MacBook Pro 14/16 panel proportions to the
  shared Display menu and command-line options on Linux, Windows and macOS.
  Existing 16:10, 16:9, Window and Custom modes remain available. Window follows
  the actual drawable area; panel presets do not change display resolution.
- Merge [PR #9](https://github.com/TheRunaway5/Phase-Distorter/pull/9) by
  [s-ilent](https://github.com/s-ilent), with an optional adaptive deadline guard under **F1 > Debug >
  Experimental frame pacing**, default off and saved with display preferences.
  The guard accounts for the complete presentation cost, including swap.
- Keep interpolation on the native timeline to avoid alternating-cost holds
  and transition jumps. Toggling the guard preserves game and audio clocks.

## Compatibility

- Retain the Linux glibc 2.36 baseline and private static C++ support. SDL2 is
  included with each platform's application.
- Keep existing save, asset-pack, snapshot and display preference formats.
- No ROMs, imported asset packs, saves or personal preferences are included.

This remains a development release. Bounded tests do not establish a complete
playthrough, complete native-engine parity or physical display/VRR/audio behavior.
Higher-rate presentation remains sensitive to game update cost and host load.

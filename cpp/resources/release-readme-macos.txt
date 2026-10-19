Phase Distorter @VERSION@ for macOS

Requires macOS 11 or later. This universal application includes native Apple
silicon and Intel code, plus its private SDL2 runtime. It uses macOS OpenGL;
MoltenVK, Homebrew and a separate Vulkan SDK are not required.

Extract the entire ZIP, then drag Phase Distorter.app to Applications and open
it. The app is ad-hoc signed, not Apple Developer ID signed or notarized. macOS
may require Privacy & Security > Open Anyway after the first launch attempt.
Only approve a download you obtained from the project's official release page.

On first launch, select your own supported US EarthBound or Japanese Mother 2
ROM to import assets. No ROM, imported asset pack, save or preference is bundled.
Saves and imported assets remain outside the app in:
  ~/Library/Application Support/ebsrc/EarthBoundCpp/
Back up earthbound.srm and mother2.srm before updating. Quit normally to save.

F1 opens Settings. F11 toggles fullscreen. Display > Widescreen offers 16:10,
16:9 and exact MacBook Air 13/15 and MacBook Pro 14/16 panel proportions on all
platforms. Window follows the actual drawable area, including available space
on a notched display. Panel presets change proportions, not screen resolution.

Debug > Experimental frame pacing defaults off. It enables an adaptive deadline
guard for higher frame rates without changing game or audio speed. Results
depend on update cost, the display and the window system.

The default gameplay route and experimental native Continue route retain their
existing development limitations. This release does not establish whole-game
or cycle-accurate equivalence. See PATCH-NOTES.md for validation scope.

MANIFEST.json and SHA256SUMS record payload integrity. Dependency licenses and
notices are also inside the app under Contents/Resources so they follow a move
to Applications. Source and build instructions are available in the same GitHub
release's source snapshot.

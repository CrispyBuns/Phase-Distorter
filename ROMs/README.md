# User-supplied ROMs

Place your own EarthBound (US) or Mother 2 (Japan) ROM here as a `.smc` or
`.sfc` file. Phase Distorter checks this folder at startup and automatically
imports a supported ROM when the selected game's asset pack has not been
created yet. The application root and the current working directory are both
checked; `roms/`, `ROM/`, and `rom/` are also accepted.

ROMs are not included in this repository. Unsupported or modified files are
skipped. Select EarthBound by default, or launch with `--game mother2` to
import a Mother 2 ROM. Importing creates the normal local asset pack and does
not modify the ROM file.

Startup and import diagnostics are written to `log.txt` in the working
directory.

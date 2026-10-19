#!/usr/bin/env python3
"""Create and verify a self-contained, ad-hoc signed universal macOS app."""
import argparse
from pathlib import Path
import plistlib
import shutil
import subprocess
import tempfile

from package_release import COMMON_NOTICES, ROOT, template


def run(*args):
    return subprocess.check_output(args, text=True).strip()


def require_universal(path):
    if set(run("lipo", "-archs", str(path)).split()) != {"arm64", "x86_64"}:
        raise ValueError(f"Expected both Apple silicon and Intel slices: {path}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--executable", type=Path, required=True)
    parser.add_argument("--sdl-library", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    version = (ROOT / "VERSION").read_text().strip()
    for path in (args.executable, args.sdl_library):
        require_universal(path)
    if args.output.exists():
        raise ValueError(f"Output already exists; select a fresh app path: {args.output}")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=args.output.parent) as temporary:
        app = Path(temporary) / "Phase Distorter.app"
        contents = app / "Contents"
        binary = contents / "MacOS/Phase Distorter"
        library = contents / "Frameworks/libSDL2-2.0.0.dylib"
        for directory in (binary.parent, library.parent, contents / "Resources"):
            directory.mkdir(parents=True, exist_ok=True)
        shutil.copy2(args.executable, binary)
        shutil.copy2(args.sdl_library, library)
        binary.chmod(0o755)
        library.chmod(0o755)
        # Replace the exact linked SDL install name, whatever prefix the builder
        # used. System frameworks and /usr/lib are the only other dependencies.
        dependencies = run("otool", "-L", str(binary)).splitlines()[1:]
        linked_sdl = sorted({line.strip().split(" (", 1)[0] for line in dependencies if "libSDL2" in line})
        if len(linked_sdl) != 1:
            raise ValueError("Expected exactly one SDL2 dependency")
        run("install_name_tool", "-change", linked_sdl[0], "@executable_path/../Frameworks/" + library.name, str(binary))
        run("install_name_tool", "-id", "@rpath/" + library.name, str(library))
        for path in (binary, library):
            for line in run("otool", "-L", str(path)).splitlines()[1:]:
                dependency = line.strip().split(" (", 1)[0]
                if dependency.startswith(("/System/Library/", "/usr/lib/", "@executable_path/../Frameworks/", "@rpath/libSDL2")):
                    continue
                # otool prints architecture headings for universal files.
                if line.endswith(":"):
                    continue
                raise ValueError(f"Nonportable dependency in {path.name}: {dependency}")
        info = {
            "CFBundleIdentifier": "com.therunaway5.phase-distorter",
            "CFBundleName": "Phase Distorter", "CFBundleDisplayName": "Phase Distorter",
            "CFBundleExecutable": "Phase Distorter", "CFBundlePackageType": "APPL",
            "CFBundleShortVersionString": version, "CFBundleVersion": version,
            "CFBundleIconFile": "PhaseDistorter", "LSMinimumSystemVersion": "11.0",
            "LSApplicationCategoryType": "public.app-category.games",
            "NSHighResolutionCapable": True, "NSSupportsAutomaticGraphicsSwitching": True,
        }
        (contents / "Info.plist").write_bytes(plistlib.dumps(info))
        iconset = Path(temporary) / "PhaseDistorter.iconset"
        iconset.mkdir()
        for size in (16, 32, 128, 256, 512):
            for scale in (1, 2):
                suffix = "@2x" if scale == 2 else ""
                run("sips", "-z", str(size * scale), str(size * scale),
                    str(ROOT / "cpp/resources/phase-distorter.png"),
                    "--out", str(iconset / f"icon_{size}x{size}{suffix}.png"))
        run("iconutil", "-c", "icns", str(iconset), "-o", str(contents / "Resources/PhaseDistorter.icns"))
        for source, name in COMMON_NOTICES:
            destination = contents / "Resources" / name
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / source, destination)
        for source, name in (("release-notice.txt", "NOTICE.txt"), ("release-readme-macos.txt", "README.txt")):
            (contents / "Resources" / name).write_bytes(template(source, version, name).data)
        # Sign nested code first; the app seal covers its embedded notices too.
        run("codesign", "--force", "--sign", "-", str(library))
        run("codesign", "--force", "--sign", "-", str(app))
        run("codesign", "--verify", "--deep", "--strict", "--verbose=2", str(app))
        shutil.move(app, args.output)
    print(f"Verified universal macOS application: {args.output}")


if __name__ == "__main__":
    main()

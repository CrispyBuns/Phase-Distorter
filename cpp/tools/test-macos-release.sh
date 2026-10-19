#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/../.."
work="$PWD/build/macos-check"
mkdir -p "$work"
archive="$(find releases -maxdepth 1 -name 'Phase-Distorter-*-macos-universal.zip' -print)"
test -f "$archive"
unzip -q "$archive" -d "$work"
app="$work/$(basename "$archive" .zip)/Phase Distorter.app"
codesign --verify --deep --strict --verbose=2 "$app"
"$app/Contents/MacOS/Phase Distorter" --help
tar -xzf build/macos-release/macos-verification.tar.gz -C "$work"
export DYLD_LIBRARY_PATH="$app/Contents/Frameworks"
for target in desktop_support_tests frame_pacer_tests presentation_scheduler_tests \
    presentation_pipeline_tests frame_interpolator_tests video_tests debug_panel_tests direct_scene_video_tests; do
    echo "Testing $target on $(uname -m)"
    "$work/verification/$target"
done

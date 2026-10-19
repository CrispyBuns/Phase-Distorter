#!/bin/sh
# Resolve against this script, not the caller's working directory. This lets
# desktop shortcuts and paths containing spaces reach the same release files.
set -eu
release_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
if [ "$(uname -s)" = Darwin ]; then
    for program in "$release_dir/build/macos-release/Phase Distorter.app/Contents/MacOS/Phase Distorter" \
                   "$release_dir/Phase Distorter.app/Contents/MacOS/Phase Distorter"; do
        if [ -x "$program" ]; then
            exec "$program" "$@"
        fi
    done
    printf '%s\n' 'No macOS app found. Run bash cpp/tools/build-macos.sh or download the macOS release.' >&2
    exit 1
fi
# Prefer a developer's local build. The packaged binary remains a fallback for
# users who have only extracted the release and have no compiler installed.
for program in "$release_dir/build/cpp/eb_cpp" "$release_dir/launchers/linux/bin/eb_cpp"; do
    if [ -x "$program" ]; then
        exec "$program" "$@"
    fi
done
printf '%s\n' 'No Linux executable found. Run ./build-linux.sh first.' >&2
exit 1

#!/bin/bash
# Native universal build, including a private SDL2 runtime. No Homebrew runtime
# paths or donor game data enter the application bundle.
set -euo pipefail
cd "$(dirname "$0")/../.."
test "$(uname -s)" = Darwin
work="$PWD/build/macos-release"
mkdir -p "$work/dependencies"
curl --fail --location --retry 3 https://www.libsdl.org/release/SDL2-2.32.10.tar.gz -o "$work/dependencies/SDL2.tar.gz"
echo "5f5993c530f084535c65a6879e9b26ad441169b3e25d789d83287040a9ca5165  $work/dependencies/SDL2.tar.gz" | shasum -a 256 -c -
tar -xzf "$work/dependencies/SDL2.tar.gz" -C "$work/dependencies"
cmake -S "$work/dependencies/SDL2-2.32.10" -B "$work/sdl-build" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$work/sdl" \
    '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64' -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
    -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TEST=OFF -DSDL_TESTS=OFF
cmake --build "$work/sdl-build" --parallel 3
cmake --install "$work/sdl-build"
cmake -S . -B "$work/build" -DCMAKE_BUILD_TYPE=Release \
    '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64' -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
    -DCMAKE_PREFIX_PATH="$work/sdl" -DEB_BUILD_TESTS=ON
targets=(eb_cpp desktop_support_tests frame_pacer_tests presentation_scheduler_tests
    presentation_pipeline_tests frame_interpolator_tests video_tests debug_panel_tests direct_scene_video_tests)
cmake --build "$work/build" --target "${targets[@]}" --parallel 3
python3 cpp/tools/bundle_macos.py --executable "$work/build/cpp/eb_cpp" \
    --sdl-library "$work/sdl/lib/libSDL2-2.0.0.dylib" --output "$work/Phase Distorter.app"
python3 cpp/tools/package_release.py --platform macos --macos-app "$work/Phase Distorter.app" \
    --patch-notes "releases/Phase-Distorter-$(cat VERSION)-patch-notes.md"
mkdir -p "$work/verification"
for target in "${targets[@]:1}"; do cp "$work/build/cpp/$target" "$work/verification/"; done
tar -czf "$work/macos-verification.tar.gz" -C "$work" verification

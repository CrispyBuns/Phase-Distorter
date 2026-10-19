#!/bin/bash
# Fast renderer test loop against an already built app; no game rebuild needed.
set -euo pipefail
cd "$(dirname "$0")/../../.."
app="$1"
output="$2"
work="$PWD/build/macos-video-test"
mkdir -p "$work"
curl --fail --location --retry 3 https://www.libsdl.org/release/SDL2-2.32.10.tar.gz -o "$work/SDL2.tar.gz"
echo "5f5993c530f084535c65a6879e9b26ad441169b3e25d789d83287040a9ca5165  $work/SDL2.tar.gz" | shasum -a 256 -c -
tar -xzf "$work/SDL2.tar.gz" -C "$work"
python3 - "$work" <<'PY'
from pathlib import Path
import sys
root=Path('cpp/resources/shaders')
text=(root/'crt_shader_source.hpp.in').read_text().replace('@EB_CRT_SHADER@',(root/'crt_oled.frag').read_text())
(Path(sys.argv[1])/'crt_shader_source.hpp').write_text(text)
PY
clang++ -std=c++20 -O2 -DSDL_MAIN_HANDLED -Icpp/include -I"$work" -I"$work/SDL2-2.32.10/include" \
    cpp/tests/video_tests.cpp cpp/src/frame_presenter.cpp cpp/src/crt_filter.cpp cpp/src/scene_effects_renderer.cpp \
    "$app/Contents/Frameworks/libSDL2-2.0.0.dylib" -framework OpenGL -o "$output"

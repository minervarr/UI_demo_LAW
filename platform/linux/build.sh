#!/usr/bin/env bash
# Desktop (Linux/Wayland) build. The counterpart of what platform/windows held
# before the app_shell port.
#
# slangc is NOT from a Vulkan SDK on this machine — there isn't one installed.
# It comes from /opt/shader-slang, and vk_canvas's shader step needs to be told
# so explicitly or it silently skips the GUI or uses a stale cached path.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
MODE="${1:-debug}"
SLANGC="${VCE_SLANGC:-/opt/shader-slang/bin/slangc}"

case "$MODE" in
    debug)   BUILD_TYPE=Debug;   BUILD_DIR="$ROOT/build/linux_debug" ;;
    release) BUILD_TYPE=Release; BUILD_DIR="$ROOT/build/linux" ;;
    clean)   rm -rf "$ROOT/build"; echo "cleaned $ROOT/build"; exit 0 ;;
    *) echo "usage: $0 [debug|release|clean]" >&2; exit 2 ;;
esac

if [[ ! -x "$SLANGC" ]]; then
    echo "error: slangc not found at $SLANGC" >&2
    echo "       set VCE_SLANGC to its path" >&2
    exit 1
fi

git -C "$ROOT" submodule update --init --recursive

cmake -S "$ROOT" -B "$BUILD_DIR" -G Ninja \
      -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
      -DVCE_SLANGC="$SLANGC"
cmake --build "$BUILD_DIR" -j"$(nproc)"

echo
echo "built: $BUILD_DIR/ui_demo"
echo "  assets (shaders + baked atlas + CJK faces) sit beside it in $BUILD_DIR/assets"

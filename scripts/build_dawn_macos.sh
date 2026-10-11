#!/bin/zsh
# Builds the macOS (Metal) Dawn shared library that external/dawn/CMakeLists.txt
# imports on Apple. Run once from the repo root; the result is committed under
# external/dawn/macos/ like the vendored Linux .so and Windows .dll.
#
# The commit must match the headers in external/dawn/include (it is the one
# embedded in the vendored webgpu_dawn.dll). The script diffs the generated
# headers against them and fails if they differ.
set -e

DAWN_COMMIT="37c81933d340fe0091e0795d3a49b6acadf14c68"
REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK_DIR="${DAWN_WORK_DIR:-$REPO_ROOT/.dawn-build}"
SRC_DIR="$WORK_DIR/dawn"
BUILD_DIR="$WORK_DIR/build"
OUT_DIR="$REPO_ROOT/external/dawn/macos/lib"

if [ ! -d "$SRC_DIR/.git" ]; then
    mkdir -p "$SRC_DIR"
    git -C "$SRC_DIR" init -q
    git -C "$SRC_DIR" remote add origin https://dawn.googlesource.com/dawn
fi
if [ "$(git -C "$SRC_DIR" rev-parse HEAD 2>/dev/null)" != "$DAWN_COMMIT" ]; then
    echo "-- Fetching Dawn $DAWN_COMMIT..."
    git -C "$SRC_DIR" fetch --depth 1 origin "$DAWN_COMMIT"
    git -C "$SRC_DIR" checkout -q FETCH_HEAD
fi

cmake -S "$SRC_DIR" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=26.0 \
    -DDAWN_FETCH_DEPENDENCIES=ON \
    -DDAWN_BUILD_MONOLITHIC_LIBRARY=ON \
    -DDAWN_ENABLE_METAL=ON \
    -DDAWN_ENABLE_VULKAN=OFF \
    -DDAWN_ENABLE_DESKTOP_GL=OFF \
    -DDAWN_ENABLE_OPENGLES=OFF \
    -DDAWN_ENABLE_NULL=OFF \
    -DDAWN_USE_GLFW=OFF \
    -DDAWN_BUILD_SAMPLES=OFF \
    -DDAWN_BUILD_TESTS=OFF \
    -DTINT_BUILD_TESTS=OFF \
    -DTINT_BUILD_CMD_TOOLS=OFF \
    -DTINT_BUILD_SPV_READER=OFF

cmake --build "$BUILD_DIR" --target webgpu_dawn

# Generated headers must match the vendored ones the project compiles against.
for header in dawn/webgpu.h dawn/webgpu_cpp.h dawn/webgpu_cpp_print.h \
    webgpu/webgpu.h webgpu/webgpu_cpp.h; do
    generated="$BUILD_DIR/gen/include/$header"
    [ -f "$generated" ] || generated="$SRC_DIR/include/$header"
    if ! diff -q "$generated" "$REPO_ROOT/external/dawn/include/$header" >/dev/null; then
        echo "[-] $header from Dawn $DAWN_COMMIT differs from external/dawn/include."
        echo "    The vendored headers come from a different Dawn revision."
        exit 1
    fi
done

DYLIB="$(find "$BUILD_DIR" -name 'libwebgpu_dawn.dylib' -not -path '*/CMakeFiles/*' | head -1)"
if [ -z "$DYLIB" ]; then
    echo "[-] libwebgpu_dawn.dylib was not produced."
    exit 1
fi

mkdir -p "$OUT_DIR"
cp -f "$DYLIB" "$OUT_DIR/libwebgpu_dawn.dylib"
install_name_tool -id @rpath/libwebgpu_dawn.dylib "$OUT_DIR/libwebgpu_dawn.dylib"
strip -x "$OUT_DIR/libwebgpu_dawn.dylib"
codesign --force --sign - "$OUT_DIR/libwebgpu_dawn.dylib"

echo "-- Installed $OUT_DIR/libwebgpu_dawn.dylib"
otool -D "$OUT_DIR/libwebgpu_dawn.dylib"

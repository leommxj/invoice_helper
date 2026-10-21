#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CACHE_DIR="$ROOT_DIR/.cache"
CMAKE_BIN="$ROOT_DIR/tools/cmake/bin/cmake.exe"
NINJA_BIN="$ROOT_DIR/tools/ninja/ninja.exe"
WX_PREFIX="$ROOT_DIR/third_party/wxWidgets"
MUPDF_ROOT="$ROOT_DIR/third_party/mupdf"
BUILD_DIR="$ROOT_DIR/build"

mkdir -p "$CACHE_DIR" "$BUILD_DIR"
export TMPDIR="$CACHE_DIR"
export TMP="$CACHE_DIR"
export TEMP="$CACHE_DIR"

for required in "$CMAKE_BIN" "$NINJA_BIN" "$WX_PREFIX/lib/cmake/wxWidgets/wxWidgetsConfig.cmake" "$MUPDF_ROOT/include/mupdf/fitz.h" "$MUPDF_ROOT/lib/libmupdf.a"; do
    if [[ ! -e "$required" ]]; then
        echo "缺少本地依赖：$required" >&2
        exit 1
    fi
done

"$CMAKE_BIN" -S "$ROOT_DIR" -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_MAKE_PROGRAM="$NINJA_BIN" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$WX_PREFIX" \
    -DMUPDF_ROOT="$MUPDF_ROOT" \
    -DCMAKE_CXX_COMPILER=C:/Strawberry/c/bin/g++.exe \
    -DCMAKE_RC_COMPILER=C:/Strawberry/c/bin/windres.exe \
    -DCMAKE_AR=C:/Strawberry/c/bin/ar.exe \
    -DCMAKE_RANLIB=C:/Strawberry/c/bin/ranlib.exe

"$CMAKE_BIN" --build "$BUILD_DIR" --parallel 4

echo "构建完成：$BUILD_DIR/invoice_helper.exe"

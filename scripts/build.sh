#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CONFIG_INPUT="${1:-Release}"
CONFIG_LC="$(printf '%s' "$CONFIG_INPUT" | tr '[:upper:]' '[:lower:]')"

case "$CONFIG_LC" in
    debug) CONFIG_NAME="Debug" ;;
    release) CONFIG_NAME="Release" ;;
    *)
        echo "usage: $0 [Debug|Release]" >&2
        exit 1
        ;;
esac

if [[ ! -f "$ROOT_DIR/vcpkg.json" ]]; then
    echo "找不到 vcpkg.json，请在项目根目录运行" >&2
    exit 1
fi

if [[ ! -f "$ROOT_DIR/tools/vcpkg/scripts/buildsystems/vcpkg.cmake" ]]; then
    echo "vcpkg 尚未安装，请先运行：bash scripts/bootstrap-vcpkg.sh" >&2
    exit 1
fi

if [[ -x "$ROOT_DIR/tools/cmake/bin/cmake.exe" ]]; then
    CMAKE_BIN="$ROOT_DIR/tools/cmake/bin/cmake.exe"
elif command -v cmake >/dev/null 2>&1; then
    CMAKE_BIN="$(command -v cmake)"
else
    echo "找不到 CMake，请安装 CMake 3.24+ 或放到 tools/cmake/" >&2
    exit 1
fi

case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        PRESET="windows-$CONFIG_LC"
        OUTPUT="$ROOT_DIR/build/windows/bin/$CONFIG_NAME/invoice_helper.exe"
        ;;
    Darwin*)
        if [[ "$(uname -m)" == "arm64" ]]; then
            PRESET="macos-arm64-$CONFIG_LC"
            OUTPUT="$ROOT_DIR/build/macos-arm64"
        else
            PRESET="macos-x64-$CONFIG_LC"
            OUTPUT="$ROOT_DIR/build/macos-x64"
        fi
        if [[ "$CONFIG_NAME" == "Debug" ]]; then
            OUTPUT="${OUTPUT}-debug"
        fi
        OUTPUT="$OUTPUT/bin/invoice_helper"
        ;;
    *)
        PRESET="linux-$CONFIG_LC"
        OUTPUT="$ROOT_DIR/build/linux"
        if [[ "$CONFIG_NAME" == "Debug" ]]; then
            OUTPUT="${OUTPUT}-debug"
        fi
        OUTPUT="$OUTPUT/bin/invoice_helper"
        ;;
esac

echo "配置：$PRESET"
"$CMAKE_BIN" --preset "$PRESET"

echo "构建：$PRESET"
"$CMAKE_BIN" --build --preset "$PRESET" --parallel "${JOBS:-4}"

echo "构建完成：$OUTPUT"

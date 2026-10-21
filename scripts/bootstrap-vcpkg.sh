#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VCPKG_DIR="$ROOT_DIR/tools/vcpkg"

mkdir -p "$ROOT_DIR/tools"

if [[ ! -d "$VCPKG_DIR/.git" ]]; then
    echo "克隆 vcpkg 到 $VCPKG_DIR"
    git clone --depth 1 https://github.com/microsoft/vcpkg.git "$VCPKG_DIR"
fi

cd "$ROOT_DIR"
case "$(uname -s)" in
    MINGW*|MSYS*|CYGWIN*)
        cmd.exe /c "tools\\vcpkg\\bootstrap-vcpkg.bat -disableMetrics"
        ;;
    *)
        "$VCPKG_DIR/bootstrap-vcpkg.sh" -disableMetrics
        ;;
esac

echo "vcpkg 已就绪：$VCPKG_DIR"
echo "接下来运行：bash scripts/build.sh Release"

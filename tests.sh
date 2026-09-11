#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-"$ROOT_DIR/build"}"

if [[ "${DEBUG:-}" != "" ]]; then
    echo "[dbg][tests.sh] ROOT_DIR=${ROOT_DIR}" >&2
    echo "[dbg][tests.sh] BUILD_DIR=${BUILD_DIR}" >&2
fi

"$ROOT_DIR/build.sh"

meson test -C "${BUILD_DIR}"

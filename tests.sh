#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-"$ROOT_DIR/build"}"
BIN="${BIN:-"$BUILD_DIR/siml-dump"}"
TEST_DIR="$ROOT_DIR/tests"

if [[ "${DEBUG:-}" != "" ]]; then
    echo "[dbg][tests.sh] ROOT_DIR=${ROOT_DIR}" >&2
    echo "[dbg][tests.sh] BUILD_DIR=${BUILD_DIR}" >&2
    echo "[dbg][tests.sh] BIN=${BIN}" >&2
    set -x
fi

"$ROOT_DIR/build.sh"

rc=0

for siml in "$TEST_DIR"/*.siml; do
    [ -e "$siml" ] || continue
    echo "[test] $siml"
    out="${siml%.siml}.out"
    if ! "$BIN" "$siml" >"$out"; then
        echo "[test] FAILED (parse error): $siml" >&2
        rc=1
        continue
    fi
    gold="${siml%.siml}.gold"
    if [ -f "$gold" ]; then
        if ! diff -u "$gold" "$out"; then
            echo "[test] FAILED (output mismatch): $siml" >&2
            rc=1
        else
            rm -f "$out"
        fi
    fi
done

exit "$rc"

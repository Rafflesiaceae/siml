#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-"$ROOT_DIR/build"}"
BIN="${BIN:-"$BUILD_DIR/siml-dump"}"
BIN_ROUNDTRIP="${BIN_ROUNDTRIP:-"$BUILD_DIR/siml-roundtrip"}"
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
    err="${siml%.siml}.err"
    xfail="${siml%.siml}.xfail"

    if [ -f "$xfail" ]; then
        expected_err="$(cat "$xfail")"
        if [[ "$(basename "$siml")" == "xfail_io_error.siml" ]]; then
            if SIML_TEST_READ_ERROR_AFTER=1 "$BIN" "$siml" >"$out" 2>"$err"; then
                echo "[test] FAILED (expected error but succeeded): $siml" >&2
                rc=1
                continue
            fi
        else
            if "$BIN" "$siml" >"$out" 2>"$err"; then
                echo "[test] FAILED (expected error but succeeded): $siml" >&2
                rc=1
                continue
            fi
        fi
        if ! grep -F -q "$expected_err" "$err"; then
            echo "[test] FAILED (error mismatch): $siml" >&2
            echo "[test]   expected substring: $expected_err" >&2
            echo "[test]   got stderr:" >&2
            cat "$err" >&2
            rc=1
            continue
        fi
        rm -f "$out" "$err"
        continue
    fi

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

    if ! "$BIN_ROUNDTRIP" "$siml"; then
        echo "[test] FAILED (roundtrip mismatch): $siml" >&2
        rc=1
    fi
done

exit "$rc"

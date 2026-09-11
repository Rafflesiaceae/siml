#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FUZZER="${ROOT_DIR}/fuzzer"
CORPUS="${ROOT_DIR}/.cache/fuzzer"
TESTCASES="${ROOT_DIR}/testcases"
DEFAULT_TIME=60
FOREVER=0
MAX_LEN=""

usage() {
    cat <<EOF
Usage: $(basename "$0") [--forever] [--max-len=N] [-h|--help] [-- <libfuzzer-flags>]

Run the siml libFuzzer harness.

Options:
  --forever      Run until Ctrl-C (default: stop after ${DEFAULT_TIME} s)
  --max-len=N    Cap input size at N bytes (passed as -max_len=N to libFuzzer;
                 default: libFuzzer's own default of 4096)
  -h, --help     Show this help

Any arguments after -- are passed directly to the fuzzer binary, e.g.:
  $(basename "$0") -- -max_total_time=300 -jobs=4

The fuzzer binary is (re)compiled automatically when fuzzer.c or
siml.h is newer than the existing binary.  Requires clang.
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -h|--help)       usage; exit 0 ;;
        --forever)       FOREVER=1; shift ;;
        --max-len=*)     MAX_LEN="${1#--max-len=}"; shift ;;
        --)              shift; break ;;
        *)
            echo "$(basename "$0"): unknown option '$1'" >&2
            echo "Run '$(basename "$0") --help' for usage." >&2
            exit 1
            ;;
    esac
done

if ! command -v clang &>/dev/null; then
    echo "error: clang not found" >&2
    exit 1
fi

if [ ! -f "${FUZZER}" ] \
   || [ "${ROOT_DIR}/fuzzer.c"       -nt "${FUZZER}" ] \
   || [ "${ROOT_DIR}/siml.h"        -nt "${FUZZER}" ]; then
    echo "[fuzz] compiling fuzzer..."
    clang -fsanitize=fuzzer,address -O1 -o "${FUZZER}" "${ROOT_DIR}/fuzzer.c"
fi

mkdir -p "${CORPUS}"

if [ -z "$(ls -A "${CORPUS}" 2>/dev/null)" ]; then
    echo "[fuzz] seeding corpus from testcases/..."
    for f in "${TESTCASES}"/*.siml; do
        [ -f "$f" ] || continue
        cp "$f" "${CORPUS}/$(basename "$f")"
    done
fi

FUZZ_ARGS=("${CORPUS}")
if [[ "${FOREVER}" -eq 0 ]]; then
    FUZZ_ARGS+=("-max_total_time=${DEFAULT_TIME}")
fi
if [[ -n "${MAX_LEN}" ]]; then
    FUZZ_ARGS+=("-max_len=${MAX_LEN}")
fi
FUZZ_ARGS+=("$@")

echo "[fuzz] starting fuzzer ($([ "${FOREVER}" -eq 1 ] && echo 'running until Ctrl-C' || echo "stopping after ${DEFAULT_TIME} s"))..."
exec "${FUZZER}" "${FUZZ_ARGS[@]}"

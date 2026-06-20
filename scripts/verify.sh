#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."
mkdir -p build

echo "== Minimal Python bootloader syntax =="
python3 - <<'PY'
import py_compile

py_compile.compile("cdc_boot.py", cfile="build/cdc_boot.pyc", doraise=True)
PY

echo
echo "== Python host boundary =="
python3 - <<'PY'
from pathlib import Path

py = sorted(p.name for p in Path(".").glob("*.py"))
assert py == ["cdc_boot.py"], py
print("python host boundary: ok (cdc_boot.py only)")
PY

echo
echo "== Native .cdc contract and witness suite =="
python3 cdc_boot.py

echo
echo "== Operational bridge runtime =="
command -v cc >/dev/null 2>&1 || {
  echo "cc is required for operational bridge verification" >&2
  exit 1
}
cc -std=c99 -Wall -Wextra -pedantic -O2 \
  runtime/cdc_bridge_runtime.c \
  -o build/cdc_bridge_runtime
build/cdc_bridge_runtime verify bridge64.cdc

dyadic_lookup="$(build/cdc_bridge_runtime lookup-dyadic bridge64.cdc 101011)"
case "$dyadic_lookup" in
  *"index=43"*"triadic=223"*) echo "$dyadic_lookup" ;;
  *) echo "unexpected dyadic lookup: $dyadic_lookup" >&2; exit 1 ;;
esac

triadic_lookup="$(build/cdc_bridge_runtime lookup-triadic bridge64.cdc 223)"
case "$triadic_lookup" in
  *"index=43"*"dyadic=101011"*) echo "$triadic_lookup" ;;
  *) echo "unexpected triadic lookup: $triadic_lookup" >&2; exit 1 ;;
esac

trace_projection="$(build/cdc_bridge_runtime project-trits bridge64.cdc '+0-+0-' council)"
case "$trace_projection" in
  *"occupancy=101101"*"index=45"*"triadic=231"*) echo "$trace_projection" ;;
  *) echo "unexpected trace projection: $trace_projection" >&2; exit 1 ;;
esac

build/cdc_bridge_runtime codebook 9
build/cdc_bridge_runtime codebook 12
build/cdc_bridge_runtime grid bridge64.cdc > build/bridge64-grid.txt
build/cdc_bridge_runtime grid-svg bridge64.cdc > build/bridge64-grid.svg
test -s build/bridge64-grid.txt
test -s build/bridge64-grid.svg
cmp -s build/bridge64-grid.svg assets/bridge64-grid.svg || {
  echo "assets/bridge64-grid.svg is stale; regenerate with: build/cdc_bridge_runtime grid-svg bridge64.cdc > assets/bridge64-grid.svg" >&2
  exit 1
}

echo
echo "== Paper compile =="
if command -v tectonic >/dev/null 2>&1; then
  (cd paper/arxiv && tectonic main.tex)
else
  echo "tectonic not found; skipping PDF compile"
fi

echo
echo "All checks passed."

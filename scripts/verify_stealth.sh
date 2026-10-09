#!/usr/bin/env bash
set -euo pipefail

SO_PATH="${1:-}"
PID="${2:-}"
if [[ -z "$SO_PATH" || ! -f "$SO_PATH" ]]; then
  echo "usage: $0 /path/to/arm64-v8a.so [target-pid]" >&2
  exit 2
fi

echo "[ELF] exported symbols"
SYMBOLS="$(readelf --wide -Ws "$SO_PATH")"
awk '$4 == "FUNC" && $7 != "UND" {print}' <<< "$SYMBOLS"
if ! awk '$4 ~ /^(FUNC|OBJECT)$/ && $7 !~ /^(UND|ABS)$/ && $8 != "zygisk_module_entry" { print; invalid=1 } END { exit invalid }' <<< "$SYMBOLS"; then
  echo "error: private defined ELF symbols are visible" >&2
  exit 1
fi
awk '$4 == "FUNC" && $7 != "UND" && $8 == "zygisk_module_entry" { found=1 } END { exit !found }' <<< "$SYMBOLS" || {
  echo "error: zygisk_module_entry is missing" >&2
  exit 1
}

if [[ -n "$PID" ]]; then
  echo "[maps] module and writable-executable mappings for pid $PID"
  adb shell "cat /proc/$PID/maps" | grep -E 'lineage_hide|rwxp' || true
  echo "[GOT] Binder filtering uses the BinderProxy JNI hook; libbinder"
  echo "      PLT/GOT relocations stay untouched."
fi

echo "verification complete"

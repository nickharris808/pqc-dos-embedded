#!/usr/bin/env bash
# Cross-compile the freestanding Cortex-M3 PQC-reassembly memory-DoS demo.
#
# Exit codes:  0 = built (prints BINARY=...)   3 = SKIP (no toolchain)   1 = build error
#
# Optional -D overrides via EMBED_DEFS, e.g.:
#   EMBED_DEFS="-DFLOOD_N=64 -DARENA_BYTES=$((48*1024))" ./build.sh out.elf
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
SRC="$HERE/src/pqc_dos_embedded.c"
LD="$HERE/src/embedded.ld"
OUT="${1:-$HERE/build/pqc_dos_embedded.elf}"
mkdir -p "$(dirname "$OUT")"

GCC="$(command -v arm-none-eabi-gcc 2>/dev/null || true)"
[ -z "$GCC" ] && [ -x /opt/homebrew/bin/arm-none-eabi-gcc ] && GCC=/opt/homebrew/bin/arm-none-eabi-gcc
if [ -z "$GCC" ]; then
  echo "SKIP: arm-none-eabi-gcc not found." >&2
  echo "  macOS: brew install arm-none-eabi-gcc" >&2
  echo "  Debian/Ubuntu: sudo apt-get install gcc-arm-none-eabi" >&2
  exit 3
fi

# shellcheck disable=SC2086
"$GCC" -mcpu=cortex-m3 -mthumb -nostdlib -nostartfiles -ffreestanding -fno-builtin \
  -Os -Wall -Wextra ${EMBED_DEFS:-} -T "$LD" "$SRC" -lgcc -o "$OUT"
rc=$?
if [ $rc -ne 0 ]; then echo "ERROR: cross-compile failed (rc=$rc)" >&2; exit 1; fi
echo "BINARY=$OUT"
echo "GCC=$("$GCC" -dumpversion 2>/dev/null)"
exit 0

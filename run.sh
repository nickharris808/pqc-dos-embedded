#!/usr/bin/env bash
# Build + run both scenarios under QEMU and print the semihosting transcript.
#
# Usage:  ./run.sh              # both scenarios (benign, then attack)
#         ./run.sh benign       # 64 fragments  -> both designs survive
#         ./run.sh attack       # 200000 frags  -> naive OOMs, bounded survives
#
# Exit codes: 0 = ran   3 = SKIP (missing toolchain or qemu)   1 = error
#
# Note: the guest writes via ARM semihosting SYS_WRITE0, which QEMU emits on its
# *stderr*, and QEMU itself exits non-zero after the guest's SYS_EXIT. Both are
# normal; we merge stderr into stdout and do not treat QEMU's exit code as failure.
set -uo pipefail
HERE="$(cd "$(dirname "$0")" && pwd)"
WHICH="${1:-both}"

QEMU="$(command -v qemu-system-arm 2>/dev/null || true)"
if [ -z "$QEMU" ]; then
  echo "SKIP: qemu-system-arm not found." >&2
  echo "  macOS: brew install qemu   Debian/Ubuntu: sudo apt-get install qemu-system-arm" >&2
  exit 3
fi

run_one() {
  local name="$1" flood="$2"
  local elf="$HERE/build/pqc_dos_${name}.elf"
  EMBED_DEFS="-DFLOOD_N=${flood}UL" "$HERE/build.sh" "$elf" >/dev/null
  local rc=$?
  if [ $rc -eq 3 ]; then return 3; fi
  if [ $rc -ne 0 ]; then return 1; fi
  echo "===== scenario=${name} (FLOOD_N=${flood}) ====="
  # 2>&1: semihosting output arrives on QEMU's stderr.
  # || true: QEMU exits non-zero after the guest calls SYS_EXIT.
  "$QEMU" -M lm3s6965evb -nographic -semihosting -kernel "$elf" 2>&1 || true
  echo
  return 0
}

case "$WHICH" in
  benign) run_one benign 64 ;;
  attack) run_one attack 200000 ;;
  both)
    run_one benign 64; rc=$?
    if [ $rc -eq 3 ]; then exit 3; fi
    if [ $rc -ne 0 ]; then exit 1; fi
    run_one attack 200000
    ;;
  *) echo "usage: $0 [benign|attack|both]" >&2; exit 2 ;;
esac
exit $?

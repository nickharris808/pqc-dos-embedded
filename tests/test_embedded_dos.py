"""End-to-end tests for the constrained-device PQC reassembly DoS demo.

These tests cross-compile the freestanding Cortex-M3 image and run it under QEMU.
Where either tool is absent the tests SKIP rather than fail -- a missing toolchain
is not a defect in this package.

What is actually asserted:

  * the ungated ("naive") design SURVIVES a benign sequence and RUNS OUT OF MEMORY
    under the flood -- so the OOM is flood-induced, not a rigged harness;
  * the bounded design completes BOTH, processing every fragment; and
  * the bounded design's peak heap is BYTE-IDENTICAL across a 3125x change in
    offered input. That last one is the whole point of the demo.
"""
from __future__ import annotations

import re
import shutil
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
RUN = ROOT / "run.sh"

HAVE_GCC = shutil.which("arm-none-eabi-gcc") or Path("/opt/homebrew/bin/arm-none-eabi-gcc").exists()
HAVE_QEMU = shutil.which("qemu-system-arm") is not None

requires_toolchain = pytest.mark.skipif(
    not (HAVE_GCC and HAVE_QEMU),
    reason="needs arm-none-eabi-gcc and qemu-system-arm",
)


def _run(scenario: str) -> dict[str, str]:
    """Run one scenario and parse its key=value transcript."""
    proc = subprocess.run(
        [str(RUN), scenario], capture_output=True, text=True, timeout=600
    )
    if proc.returncode == 3:
        pytest.skip("toolchain unavailable")
    assert proc.returncode == 0, f"run.sh failed: {proc.stdout}\n{proc.stderr}"
    fields: dict[str, str] = {}
    for match in re.finditer(r"(\w+)=([^\s]+)", proc.stdout):
        fields[match.group(1)] = match.group(2)
    assert fields, f"no key=value output parsed from:\n{proc.stdout}"
    return fields


@pytest.fixture(scope="module")
def benign() -> dict[str, str]:
    return _run("benign")


@pytest.fixture(scope="module")
def attack() -> dict[str, str]:
    return _run("attack")


@requires_toolchain
def test_device_is_actually_constrained(benign):
    """The demo is worthless on a big device: assert the modeled SRAM is small."""
    assert int(benign["ram_bytes"]) == 65536
    assert int(benign["arena_bytes"]) == 49152
    assert int(benign["bounded_cap_bytes"]) == 16384


@requires_toolchain
def test_benign_positive_control_both_designs_survive(benign):
    """Positive control. If the ungated design failed here too, the attack
    result would prove nothing -- the OOM has to be caused by the flood."""
    assert benign["naive_status"] == "ok"
    assert benign["incremental_status"] == "ok"
    assert int(benign["incremental_processed"]) == 64


@requires_toolchain
def test_attack_ungated_design_runs_out_of_memory(attack):
    assert attack["naive_status"] == "oom"
    assert int(attack["naive_requested_bytes"]) == 102_400_000
    # It demands three orders of magnitude more than the device physically has.
    assert int(attack["naive_x_device_ram"]) >= 1000


@requires_toolchain
def test_attack_bounded_design_completes_every_fragment(attack):
    assert attack["incremental_status"] == "ok"
    assert int(attack["incremental_processed"]) == 200_000


@requires_toolchain
def test_peak_heap_is_identical_across_a_3125x_input_change(benign, attack):
    """The headline invariant.

    Offered input grows 32,768 -> 102,400,000 bytes (3125x). Peak heap must not
    move by a single byte. A design that merely 'used less memory' would still
    drift; a bounded one does not.
    """
    benign_peak = int(benign["incremental_peak_heap_bytes"])
    attack_peak = int(attack["incremental_peak_heap_bytes"])
    assert benign_peak == attack_peak == 16896

    offered_ratio = int(attack["flood_total_bytes"]) / int(benign["flood_total_bytes"])
    assert offered_ratio == pytest.approx(3125, rel=0.01)


@requires_toolchain
def test_bounded_peak_stays_under_device_ram(attack):
    """The ungated design exceeds device RAM; the bounded one must not."""
    assert int(attack["incremental_peak_heap_bytes"]) < int(attack["ram_bytes"])
    assert int(attack["naive_requested_bytes"]) > int(attack["ram_bytes"])


@requires_toolchain
def test_chain_value_differs_between_scenarios(benign, attack):
    """The chain folds every admitted fragment, so different inputs must give
    different chains. Equal chains would mean the bounded arm stopped early."""
    assert benign["incremental_chain"] != attack["incremental_chain"]
    assert re.fullmatch(r"[0-9a-f]{16}", benign["incremental_chain"])
    assert re.fullmatch(r"[0-9a-f]{16}", attack["incremental_chain"])

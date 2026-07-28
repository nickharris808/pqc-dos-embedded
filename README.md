# pqc-dos-embedded

[![license](https://img.shields.io/badge/license-Apache--2.0-blue.svg)](LICENSE)
[![arch](https://img.shields.io/badge/target-ARMv7--M%20(Cortex--M3)-green.svg)](#)
[![deps](https://img.shields.io/badge/libc-none%20(freestanding)-orange.svg)](#)
[![tests](https://img.shields.io/badge/tests-7%20passing-brightgreen.svg)](tests/)

**Post-quantum credentials don't fit in one frame. Watch what that does to a 64 KB device.**

A stock "reassemble, then verify" receiver asks for **1,562× the device's entire RAM** and
dies. A bounded receiver processes the same 200,000-fragment flood and finishes — with a peak
heap **identical to the byte** to what it used on a benign 64-fragment message.

Runs on a real ARMv7-M ISA under QEMU. **169 lines of freestanding C**, no OS, no libc, no
crypto library. Read the whole thing over a coffee, then run it yourself.

**📖 Full documentation, tutorial and conceptual guide: <https://nickharris808.github.io/pqc-toolkit/>**

---

## Why this exists

Classical key shares fit in one frame, so a receiver never had to hold attacker-controlled,
multi-frame data *before* it could check that data's integrity. Post-quantum credentials are
7,533 bytes and up — fragmentation is now mandatory on the ordinary path, and the receiver
holds partial state it cannot yet authenticate.

On a server that's a memory-growth problem you can paper over. On a microcontroller it is a
**hard stop**: the allocation fails and the device is out of the protocol. This repository is
the smallest honest demonstration of that, and of the fix.

## Install

Nothing to install — two tools and a shell:

```bash
# macOS
brew install arm-none-eabi-gcc qemu
# Debian / Ubuntu
sudo apt-get install gcc-arm-none-eabi qemu-system-arm
```

## 30-second quickstart

```bash
git clone <this-repo> && cd pqc-dos-embedded
./run.sh
```

## Worked example — actual output

```
===== scenario=benign (FLOOD_N=64) =====
device=cortex-m3(lm3s6965evb) isa=ARMv7-M ram_bytes=65536 arena_bytes=49152
flood_fragments=64 frag_bytes=512 flood_total_bytes=32768 bounded_cap_bytes=16384
naive_status=ok naive_requested_bytes=32768 (device was large enough)
incremental_status=ok incremental_processed=64 incremental_peak_heap_bytes=16896
incremental_chain=33e39cd3d22280b6
RESULT=done

===== scenario=attack (FLOOD_N=200000) =====
device=cortex-m3(lm3s6965evb) isa=ARMv7-M ram_bytes=65536 arena_bytes=49152
flood_fragments=200000 frag_bytes=512 flood_total_bytes=102400000 bounded_cap_bytes=16384
naive_status=oom naive_requested_bytes=102400000 naive_x_device_ram=1562
incremental_status=ok incremental_processed=200000 incremental_peak_heap_bytes=16896
incremental_chain=0854e8269b69cd7b
RESULT=done
```

Read the two `incremental_peak_heap_bytes` lines:

| | benign | attack | change |
|---|--:|--:|--:|
| Offered input | 32,768 B | 102,400,000 B | **3,125×** |
| Ungated design | ok | **out of memory** | — |
| Bounded design | ok | ok, all 200,000 processed | — |
| **Bounded peak heap** | **16,896 B** | **16,896 B** | **0 bytes** |

Input grows by a factor of 3,125. Peak retention does not move by one byte.

### The benign run is the control, and it matters

If the ungated design failed in *both* scenarios, the attack result would prove nothing — it
could just be a rigged harness. It succeeds on the benign message and fails only under the
flood, so the failure is caused by the flood.

### Why 16,896 and not 16,384

The 16,384-byte cap governs *resident reassembly state*. The extra 512 bytes are one in-flight
fragment being examined plus allocator overhead. We report the number the device actually
reached rather than the one that would look tidier.

## How the fix works

Five guards run per fragment, and the **order is the whole point**:

```
1. terminal?        2. well-formed?     3. expected index?
4. committed total?  5. CAPACITY: len(resident) + len(fragment) <= CAP
--- only past all five ---
6. advance the hash chain    7. append to resident state
```

The capacity test sits **before** the hash and **before** the append, so a refused fragment
costs neither a hash computation nor a byte of retention. A design that appends first and
checks second has already conceded the memory the check exists to deny.

The chain binds each fragment's index **and the total**, so truncation and mid-stream
substitution of the declared total are both detected.

## Configure it

```bash
EMBED_DEFS="-DFLOOD_N=1000UL -DMAX_BUFFER_BYTES=8192UL" ./build.sh out.elf
qemu-system-arm -M lm3s6965evb -nographic -semihosting -kernel out.elf 2>&1
```

`FLOOD_N` · `FRAG_BYTES` · `MAX_BUFFER_BYTES` · `DEVICE_RAM_BYTES` · `ARENA_BYTES`.

## Tests

```bash
pip install pytest && python3 -m pytest tests/ -q     # 7 passed
```

Tests skip cleanly when the cross-toolchain or QEMU is absent.

## Scope — what this does and does not show

- Modeled MCU SRAM under QEMU, not a measurement of any shipping product.
- Demonstrates a **memory bound**, not a complete secure handshake. There is no key exchange,
  no signature verification and no negotiation here.
- The SHA-256 is real and self-contained; the "fragments" are synthetic.
- **Not** a vulnerability disclosure against any vendor.

## Where this comes from

Extracted from a research lab on post-quantum authentication for constrained and wireless
devices. The bound demonstrated here is machine-checked in Lean (theorems `bounded`,
`naive_unbounded`, `gated_le_naive`, `separation`) and re-verifiable on-device without a
solver — see the companion [`farkas-check`](https://github.com/nickharris808/farkas-check) package.

This demo shows **one bound on one device**. Closing the full set of post-quantum migration
failure families — downgrade, key reinstallation, fragment splicing, roaming, multi-link key
separation and the rest — is what the closed core does.

Relevant subject matter is covered by a filed provisional patent application.
For commercial use, open a [GitHub Discussion](https://github.com/nickharris808) or an issue.

## Honest scope

**What this proves.** That on a real ARMv7-M target under QEMU, an unbounded
reassembly design requests memory proportional to attacker input while the
bounded design completes at a fixed peak heap. The numbers come from running it,
not from a model.

**What it does NOT prove.**

- **Not that your device behaves this way.** This is a 169-line freestanding
  demonstration, not your firmware.
- **Not a repair.** It shows the failure and one bound. Closing the family in a
  real stack is a different problem.
- **Not a vulnerability in any product.** No vendor's code is involved.
- **Not a timing or side-channel result.** Peak heap only.

---

## The PQC migration toolkit

Eleven free tools for teams moving authenticated key exchange to post-quantum. They **find and measure**; they do not repair.

| Tool | What it does | Where |
|---|---|---|
| [pqc-sizes](https://github.com/nickharris808/pqc-sizes) | Sizes, fragment counts, and the two-sided reassembly window | PyPI |
| [pqc-sizes-js](https://github.com/nickharris808/pqc-sizes-js) | The same arithmetic for Node and the browser | npm |
| [pqc-guard-action](https://github.com/nickharris808/pqc-guard-action) | Fail the build when the window is empty | GitHub Action |
| **pqc-dos-embedded** ← you are here | 169 lines of C: the failure on a real 64 KB device | source |
| [farkas-check](https://github.com/nickharris808/farkas-check) | Re-verify the bound on-device, no SMT solver | source |
| [pqc-migration-mcp](https://github.com/nickharris808/pqc-migration-mcp) | Six MCP tools for AI agents | PyPI |
| [pqc-mfb](https://github.com/nickharris808/pqc-mfb) | 322 cases · 39 failure families · scorer | PyPI |
| [pqc-mfb (data)](https://huggingface.co/datasets/nickh007/pqc-mfb) | The benchmark as a dataset | HF |
| [pqc-formal-corpus](https://huggingface.co/datasets/nickh007/pqc-formal-corpus) | 122 named formal results, 6 provers | HF |
| [pqc-bounds-lean](https://github.com/nickharris808/pqc-bounds-lean) | The same bound in Lean 4 — 0 `sorry`, 0 imports | source |
| [pqc-dos-gate-rtl](https://github.com/nickharris808/pqc-dos-gate-rtl) | The gate in synthesizable RTL, 5 Yosys proofs | source |
| [pqc-explorer](https://huggingface.co/spaces/nickh007/pqc-explorer) | Try it in your browser, no install | HF Space |

**New here?** The [end-to-end tutorial](https://github.com/nickharris808/pqc-sizes/blob/main/TUTORIAL.md) walks one realistic migration through all of them in about ten minutes: sizes -> window -> CI gate -> benchmark.

**In a hurry?** [`pqc-sizes`](https://github.com/nickharris808/pqc-sizes) tells you in five seconds whether your credential fragments and whether a safe cap exists. [`pqc-explorer`](https://huggingface.co/spaces/nickh007/pqc-explorer) does the same in a browser, with no install.

### The closed core

Closing the 39 failure families — downgrade binding, retransmission-safe installation, fragmentation transcripts, roaming forward secrecy, multi-link key separation, admission control, group-key binding — is a separate proprietary codebase. Relevant subject matter is covered by a filed provisional patent application.

That split is measured, not asserted: under a replicate noise control only **4 of 32** repair mechanisms are externally distinguishable, so publishing these detectors does not disclose the repairs.

For commercial licensing, open a [GitHub Discussion](https://github.com/nickharris808/pqc-sizes/discussions) or an issue on any of these repos.

## License

Apache-2.0. See [LICENSE](LICENSE) and [CONTRIBUTING.md](CONTRIBUTING.md).

# CMP 170HX / cmpunlocker — Investigation Documentation

Comprehensive record of the reverse-engineering session on the NVIDIA CMP 170HX (GA100),
the `cmpunlocker-vram` tool, and the PCIe-generation lock question.

## Documents
| File | Contents |
|---|---|
| [`01-cmpunlocker-and-unlock-mechanism.md`](01-cmpunlocker-and-unlock-mechanism.md) | How the tool works: SEC2 Booter/PLM exploit, compute + memory unlock |
| [`02-pcie-gen-investigation.md`](02-pcie-gen-investigation.md) | The full PCIe-gen investigation, the fuse question, my errors + corrections, final verdict, Gen2 reconciliation, bandwidth math |
| [`03-firmware-reverse-engineering.md`](03-firmware-reverse-engineering.md) | Detailed firmware RE: partition map, FwSec comparison, devinit trace, app `0x08` fuse processing, register maps |
| [`04-toolchain-and-reproducibility.md`](04-toolchain-and-reproducibility.md) | Everything built (fusedump, envytools, Ghidra + ghidra_falcon + GhidraMCP) and how to reproduce |
| [`05-open-questions-and-hardware-tests.md`](05-open-questions-and-hardware-tests.md) | Unresolved questions + on-card test protocols for when the 170HX arrives |
| [`06-pcie-gen-attack-avenues.md`](06-pcie-gen-attack-avenues.md) | First-pass experiment plan: ranked PCIe-gen attack avenues if Test 0 fails (software #1–3, hardware #4–6, dead ends) |
| [`07-fuse-override-and-static-recon.md`](07-fuse-override-and-static-recon.md) | Deep dive on avenue #1 (fuse-override enable) + static fuse-map recon from the VBIOS + the targeted on-card 4-register check |

## Executive summary

**The card.** CMP 170HX is a full **GA100 die** (same silicon as an A100) with compute, memory, PCIe,
and NVLink restricted in firmware/OTP. `cmpunlocker-vram` restores **compute** (full SM throughput)
and **memory** (8 GB→64 GB / 10 GB→40 GB geometry) on Linux by patching NVIDIA's open kernel modules.

**How the unlock works (mechanism confirmed).** The tool's core is a **SEC2 Booter signature-payload
exploit**: it feeds the genuine, NVIDIA-signed Booter a crafted payload that makes it perform an
**arbitrary MMIO register write**, which it loops to force four PLM (Protection Level Mask) registers
open, then writes the compute (`SS0`/`SS1`) and memory (`FBPA_CFG1`/`MMU_LMR`) config registers
directly. This is a *runtime signed-code-abuse* — not a signature forgery and not a firmware mod. It
re-applies at every GSP boot. Details: doc 01.

**Why compute/memory unlock but not PCIe gen — the central finding.** cmpunlocker's power is
"**write any protected register**." Compute and memory are gated by **writable config registers over
physically-present silicon**, so the exploit reaches them. **PCIe gen is gated by an OTP fuse feeding
a hardware-set `LnkCap2` advertised-capability that firmware never writes** — there is *no register to
override*. We proved this by disassembling the firmware: the PCIe PHY config (incl. `0x14118f78`) is
applied from **VBIOS devinit tables**, the fuses are read in partition **app `0x08`**, and the
`LnkCap2` supported-speeds register (`0x140880a4` PRIV) is **absent from all 561 PCIe registers the
firmware touches** — it's fused hardware. Details: doc 02, 03.

**Correction note (important for trust).** Mid-investigation I twice leaned toward "PCIe isn't
fuse-gated / Gen2 is plausible." That was an overreach from analyzing only the *downstream* devinit
partition (which reads no fuses) and mis-targeting `0x14118f78` (a PHY side-config register, not the
gen gate). Deeper tracing — prompted by justified user skepticism — **converged on the community's
empirical result: the PCIe gen ceiling is a genuine hardware fuse.** The community (170th-street) was
right; my intermediate inference was wrong. See doc 02 §"Corrections."

**Bandwidth reality.** Stock PCIe = Gen1 x4 ≈ 1 GB/s/dir. The two *physical* mods are independent and
multiplicative: the x16 caps mod (community-proven) + Gen2 (unresolved — depends on the fused `LnkCap2`
ceiling). Best case **Gen2 x16 = 8 GB/s/dir = 16 GB/s aggregate** (8× stock); demonstrated fallback
**Gen1 x16 = 4 GB/s/dir = 8 GB/s aggregate** (4×). Gen3/4 are fused out. Details: doc 02 §"Bandwidth."

## Status ledger (proven / inferred / open)
- ✅ **Proven:** compute+memory unlock mechanism; PCIe PHY config is table-driven; fuses read in app `0x08`; `LnkCap2` never firmware-written (⇒ fuse-set gen cap); FwSec code identical 170HX↔A100.
- 🔶 **Strongly supported:** PCIe gen is fuse-gated & software-unbeatable (community empirical + our static analysis converge).
- ❓ **Open (needs hardware):** exact fused ceiling (Gen1 vs Gen2 — read `LnkCap2`); whether a retimer can beat it; the compute tensor-core sub-throttle (~200 vs 312 TFLOPS).

## Artifacts referenced (in this repo)
- `roms/` — the two VBIOS ROMs (170HX `cmp170hx-bios-268495.rom`, A100 `a100-bios.rom`)
- `fwsec/` — extracted partitions, inner images, decompilations, disassembly windows
- `driver/.build/tools/` — built `nvbios`/`envydis`, analysis notes (gitignored)
- `driver/.build/fusedump/` — the BAR0 register-dump tool (gitignored)
- `driver/patches/` — the actual cmpunlocker unlock patches (0001–0006); `experimental/0007` = opt-in PCIe-gen probe
- `recon/` — read-only pre-hardware recon probe (`probe-170hx.sh`) + interpretation guide (`recon/README.md`)

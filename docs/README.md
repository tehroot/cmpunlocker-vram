# CMP 170HX / cmpunlocker — Investigation Documentation

Record of the reverse-engineering work on the NVIDIA CMP 170HX (GA100), the
`cmpunlocker-vram` tool, and the PCIe-generation lock question.

## Start here

**[`00-current-state.md`](00-current-state.md)** — what works, what the crippling
is as measured against a live A100, every closed route, what remains open, and
the method notes that each cost a wrong conclusion.

The numbered docs below are the record of *how* those conclusions were reached.
Several carry superseded claims and are banner-marked; where any of them
disagrees with doc 00, doc 00 wins.

## Documents
| File | Contents |
|---|---|
| [`00-current-state.md`](00-current-state.md) | **Single source of truth.** Current state, closed/open routes, capabilities, tooling, method notes |
| [`01-cmpunlocker-and-unlock-mechanism.md`](01-cmpunlocker-and-unlock-mechanism.md) | How the tool works: SEC2 Booter/PLM exploit, compute + memory unlock |
| [`02-pcie-gen-investigation.md`](02-pcie-gen-investigation.md) | The full PCIe-gen investigation, the fuse question, my errors + corrections, final verdict, Gen2 reconciliation, bandwidth math |
| [`03-firmware-reverse-engineering.md`](03-firmware-reverse-engineering.md) | Detailed firmware RE: partition map, FwSec comparison, devinit trace, app `0x08` fuse processing, register maps |
| [`04-toolchain-and-reproducibility.md`](04-toolchain-and-reproducibility.md) | Everything built (fusedump, envytools, Ghidra + ghidra_falcon + GhidraMCP) and how to reproduce |
| [`05-open-questions-and-hardware-tests.md`](05-open-questions-and-hardware-tests.md) | Unresolved questions + on-card test protocols for when the 170HX arrives |
| [`06-pcie-gen-attack-avenues.md`](06-pcie-gen-attack-avenues.md) | First-pass experiment plan: ranked PCIe-gen attack avenues if Test 0 fails (software #1–3, hardware #4–6, dead ends) |
| [`07-fuse-override-and-static-recon.md`](07-fuse-override-and-static-recon.md) | Deep dive on avenue #1 (fuse-override enable) + static fuse-map recon from the VBIOS + the targeted on-card 4-register check |
| [`08-vbios-mac-fuse-map-external.md`](08-vbios-mac-fuse-map-external.md) | Integrates external VBIOS RE (JRex286 gist): symmetric-MAC verification (not RSA), CFG1 tier nibbles + the 8 GB→64 GB aliasing flag, full fuse map, power/HULK/DFA avenues |
| [`09-onhw-pcie-gen-beta-result.md`](09-onhw-pcie-gen-beta-result.md) | First on-hardware PCIe-gen attempt (beta): advertises Gen2 / trains Gen1; pins the gen fuse `OPT_GEN23 @ 0x82057c`, but the write never enabled `EN_SW_OVERRIDE` first — override path **untested**, not dead |
| [`10-gen2-breakthrough-hypothesis.md`](10-gen2-breakthrough-hypothesis.md) | **CONFIRMED** Gen2 mechanism: minimal sequence (`0x8C2C0` DIS_G2 + `0x8C040` MAX_RATE + LTSSM + **upstream-driven** retrain), why each piece is load-bearing, and the reconstructed discovery methodology (redacted-header Rosetta Stone + RM disasm + open-source breadcrumbs + on-card bisection) |
| [`11-debian-build-notes.md`](11-debian-build-notes.md) | Building the patched nvidia-open modules on Debian trixie: the split-headers → conftest-blindness fix (merge `-common` into the build tree). Wrapped in the one-shot [`debian13-setup.sh`](../debian13-setup.sh) |
| [`12-gen3-attack-plan.md`](12-gen3-attack-plan.md) | **Gen3 theorycraft** (5-agent synthesis): register deltas are trivial + EQ is likely firmware-handled → it all gates on whether the fuse is terminal or enforcement-only. Ranked experiment ladder front-loaded by reading the uncaptured `OPT_GEN3 @ 0x820580` fuse; runnable [`recon/gen3-probe.sh`](../recon/gen3-probe.sh) |
| [`13-gen3-synthesis.md`](13-gen3-synthesis.md) | **Gen3 synthesis** (7-agent forensic, post-Gen2-confirmed). Corrects doc 12 (`OPT_GEN3=1` captured; `PRIV_MISC_1` Gen3=[14:13]; advertise test is a valid *safe* gate). Staged safe-first plan: Phase 0 read-only driver-context `0x85080` reachability dump → Phase 1 boot-time retrain-free advertise test → Phase 2 train on x16. Doc-13 §"The wedge" records why the 35–40% prior is unsupported in **both** directions (the wedge boot ran `pci=noaer`, so it measured nothing) — Phase 1 is what moves it |
| [`14-nvlink-and-p2p.md`](14-nvlink-and-p2p.md) | **Multi-GPU bandwidth path** (7-agent). **NVLink CLOSED** (traced, not assumed): RO-shadow `FUSE_NVLINK_DIS` + enforcement inside signed GSP (0 host reg access) + live-poll MINION training terminal + depopulated bridge PCB. **PCIe P2P = the path**: real gate is host-chipset + GSP caps (NOT the KBIF props or a devid gate); one CPU-side devid-gated patch (`0008-pcie-p2p.patch`, branch `p2p-prototype`); 64 MB BAR1 sufficient via the mailbox mechanism; validate with `ForceP2P=0x11` no-build test. TP needs NVLink-class BW → use pipeline-parallel + P2P |
| [`15-smbpbi-msgbox.md`](15-smbpbi-msgbox.md) | **The measurement instrument the Gen3 work lacks.** SMBPBI `GET_PCIE_LINK_INFO` (opcode `0x21`) exposes read-only `LTSSM_STATE` (incl. `RECOVERY_EQZN=0x4`) and **per-lane, per-direction Gen3 EQ coefficients** (`PRESET`/`FS`/`LF`/`PRE`/`MAIN`/`POSTCUR`). Doc 13 filed this under EQ dead-ends because it is read-only — backwards, since Phase 1 is a *measurement* problem. No in-band client exists in the open tree, but the mailbox `NV_THERM_MSGBOX_COMMAND` is a **BAR0 register at `0x660e0`** (lr10 cross-SKU inference), inside the 16 MB aperture. Gates on one read-only dword; ready-to-use CMD words + staged test inside |
| [`16-gen2-cap-reversion-fix.md`](16-gen2-cap-reversion-fix.md) | **Gen2 FIXED on AM5 (trains in 10 ms).** The doc-10 sequence failed on 9950X / Debian 13 because `LnkCap2` (`0x880A4`) reverts `0x6→0x2` between the `0007` boot block and `nv.c` device init, after which every restore write is rejected (`0x880A8` PLM-relocked, `0x8C1C0` clamped, cfg LnkCtl2 refused). **Key insight: the cap follows the *trained rate*, not the XP clamp** — `DIS_G2` clear + `MAX_RATE=2` alone provably do **not** move CAP2 (corrects doc 13). Fix = drive the upstream retrain from *inside* the window in `kernel_gsp_tu102.c`, via RM's `osPci*` abstraction (`g_os_nvoc.h:675`). Also: the `DLLLARC`-clear success-test bug in `0008`; `iomem=relaxed` required for userspace BAR0 mmap (falsifies `gen3-probe.sh`'s OcuLink attribution); `head -1` multi-GPU bugs in `retrain.sh` / `install.sh` |
| [`17-app08-phy-asymmetry.md`](17-app08-phy-asymmetry.md) | **`app08` PHY programming, 170HX vs A100** + a working Falcon CFG. 170HX `app08` references **76 PHY-space registers the A100 build never touches** (A100-only: 1), including a per-lane block at stride `0x40` — qualifying `FWSEC_COMPARISON.md`'s "functionally identical firmware", which only covered the fuse block. The 376-instr link/PHY routine at IMEM `0xcb00` **is live** (`lcall` from `0x15a4`). **IMEM addr = file offset − `0x30`** (the `*_imem.bin` files include the 48-byte descriptor). All of it is Falcon-only, far outside the 16 MB BAR0 aperture — the structural reason every host register sweep was inert. Records two wrong conclusions and why |
| [`18-pri-mapping-and-the-advertise-path.md`](18-pri-mapping-and-the-advertise-path.md) | **Falcon addresses are PRI addresses**: `falcon = 0x14000000 \| pri`, confirmed on silicon (8 register matches against this driver's own constants + `0x118xxx` is published `NV_PGC6_*`). So `0x14118f78` is PRI `0x118f78`, **inside** BAR0 and host R/W — overturning "out of reach" in docs 02/12/13/17. The app08 gate inputs read `[11:10]=2` ✓ and `bit30=0`; bit30 is PL0-writable and **persists across warm reboot** (AON island), yet with every precondition satisfied the routine still doesn't run — the open contradiction. `0x8872C` is a publish *trigger* (value ignored; publishes `0x6` even with `DIS_G2` set). Publish-path diff, the `0x118f78` field sweep and the fuse-block dumps, all closed. Explains doc 09's `VSEC_DEVICE` mystery |
| [`19-a100-reference-diff.md`](19-a100-reference-diff.md) | **The core result.** Live BAR0 diff against an A100-SXM4-80GB training Gen4. Confirms the gen fuses (`0x82057c`, `0x820580`) on silicon; closes `0xcb00`, the XP straps, the XVE window, XP3G and the publish path; maps the fuse array (256 rows, mirrored, no override bank). Records the volatility-mask and posted-write method fixes, and the contamination that made our own Gen2 writes look like crippling |
| [`20-hs-execution-surface.md`](20-hs-execution-surface.md) | The SEC2 payload is a **ROP chain**, not a hardware-limited single write: uniform fill `0x4a7` (a code address, so a sled) plus canary `0xc0deca7e` re-placed per frame. `dmem.bin` + `RAW_BOOTER` give file-driven chains, validated. Booter image is encrypted, so no static gadget discovery. Corrects two overclaims |
| [`21-app08-opt-magic.md`](21-app08-opt-magic.md) | `0x8872c` and `LnkCap2` appear in **no** ROM or ucode — firmware never publishes the advertise. `app08` writes `OPT_MAGIC` (`0x820520`) unconditionally, but our identical SEC2 write is refused: **the OPT bank is master-gated, not privilege-gated**, which is why `EN_SW_OVERRIDE`, the open PLM and `SENSE_CTRL` all failed |
| [`22-devinit-win-170hx.md`](22-devinit-win-170hx.md) | `devinit_win_170hx.bin` (0x500 bytes) verified as contiguous slice at offset `0x1eac` in `devinit_170hx_imem.bin`. `devinit_win2_170hx.bin` (0x700) = 0x200-byte preamble + `_win`. Functions span `FUN_imem_00001f77` to `FUN_imem_00004486` |
| [`23-gen3-proposal-and-red-team.md`](23-gen3-proposal-and-red-team.md) | **Gen3 proposal.** Three findings that close the register-write approach: LnkCap2 is hardware-composed (full app08 disassembly confirms), OPT bank is master-gated, every Gen3+ config table is host-RO. Four remaining vectors (SMBPBI, Booter→app08 redirect, VBIOS signing, feature-override shadow from Booter). Priority-ranked recommendation. Six assumptions listed for red-teaming |
| [`24-red-team-of-23.md`](24-red-team-of-23.md) | **Red team of doc 23.** Four claims disproved from artifacts already in the repo: the SMBPBI mailbox is `0x200e0` on a GPU, not NVSwitch's `0x660e0` (and the probe is ungated, so it has run every boot); `0x823824` is `0x00000001` on the A100 too; four of eleven `0x8238xx` deltas are our own writes; `0x823814` is a published `R--4R` readout. Master-gating contradicted by the PLM readback (`SOURCE_ENABLE` all-ones when the write was refused) and by the unverified premise that app08's store lands; competing sense-chain-re-drive model + the `OPT_WRSWEEP` experiment that decides between them. `0x8b000`–`0x8b7ff` (193 dwords) never analysed |
| [`25-counter-cases-to-23.md`](25-counter-cases-to-23.md) | **What replaces doc 23**, case by case. New result: app08's write and ours are separated by a *boot phase*, not necessarily a master — `GFW_BOOT` (`0x118234`) reads `COMPLETED` on both parts before either driver-side path runs, and the driver only ever loads FWSEC onto GSP and the Booter onto SEC2, while app08 is `0x01 DEVINIT` and runs before both. Vector B has no control-flow edge; vector C becomes coherent for a different reason; the offline app08 diff answers §1 and §3 for zero card time. Four probes with code (msgbox at `0x200e0`, `OPT_WRSWEEP`, phase witness, THERM dump region), counter-priority, and what would falsify it |

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

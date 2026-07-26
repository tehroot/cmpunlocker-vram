# 00 — Current state

Single source of truth. Where a numbered doc disagrees with this file, this file
wins — the older docs are kept as the record of how the conclusions were reached,
not as current claims.

---

## What works

**VRAM unlock + PCIe Gen2, reproducible.** Tag `gen2-am5-known-good` (`f27a663`),
patches `0001`–`0008`, verified on Debian 13 / AMD 9950X AM5 with the card on a
riser.

```
64 GB visible          (stock: 8 GB)
PCIe Gen2              (stock: Gen1)
```

Mechanism: the SEC2 Booter signature-payload exploit gives one arbitrary
HS/L3 MMIO write per Booter load
(`kgspSec2PostblTimingRefillPayload` + `kgspExecuteBooterLoad_HAL`). That opens
PLMs, extends the PMA region, and drives the Gen2 advertise + retrain.

## What does not work

**Gen3.** The reason is now mapped rather than suspected. See below.

---

## The crippling, as measured

Confirmed against a live `A100-SXM4-80GB` (`10de:20b2`, same `boot0=0x170000a1`)
training Gen4 x16.

### Fuses

```
0x82057c  OPT_GEN23   A100 = 0          170HX = 1
0x820580  OPT_GEN3    A100 = 0          170HX = 1
0x820520  OPT_MAGIC   A100 = 0x00200000 170HX = 0x16680000
                      difference        0x16480000  (bits 19, 22, 25, 26, 28)
```

`0x00200000` is the bit `app08` sets itself at boot, so the A100 carries only
that; the 170HX carries it plus five fuse-derived bits.

### What the fuse actually does

It holds an entire **class** of Gen3+ configuration state at zero, read-only
from the host. Four separate tables, all populated on the A100, all zero here,
all refusing writes (verified with a flushed re-read, so not the posted-write
artifact):

| table | region |
|---|---|
| `0x8c498` / `0x8c49c` | XP |
| `0x8890c`..`0x88928` | XVE per-lane |
| `0x88c3c`..`0x88c48` | XVE per-lane |
| `0x8e010`..`0x8e04c` | XP3G per-lane rate (A100 = `4`, i.e. Gen4) |
| `0x8e094`..`0x8e0a8`, `0x8e0c8`..`0x8e0d4` | XP3G PHY per-rate calibration |

The shared/base entries in the same blocks are **identical** on both parts
(`0x8e000`, `0x8e0ac`, `0x8e0b0`, `0x8e0b8`, `0x8e0bc`, `0x8e0c0`). Only the
higher-rate set is missing. This is the measured form of what
[doc 17](17-app08-phy-asymmetry.md) and Pry §6.4 inferred from a ROM footprint.

> **The XP3G rows are *never measured here*, not *held at zero by the fuse*.**
> `0x8e09c`–`0x8e0d4` is sixteen 16-bit values, one per lane, every one distinct
> and all inside `0x00b4`–`0x00cb`; `0x8e010`–`0x8e04c` is fifteen entries of `4`
> and one of `3` (`0x8e03c`, lane 11). A fuse mirror does not produce sixteen
> near-but-different values or one odd lane — a calibration pass does. So the
> target is not "write the table" (RO, `took=0/16`, and the values are
> board-derived anyway) but "find what runs the pass." Same reading doc 19
> already reached for `0x8c498`: *some other initialization populates them*.
> [doc 24](24-red-team-of-23.md) §W5.

It also holds down **individual bits, not whole registers**: `0x88c88` accepted
bits 17–18 while refusing bit 2 in the same write.

### The advertise layer

```
LnkCap2 = (0x2 | v) & permitted        v = 4-bit field at 0x8872c
```

`permitted` never includes bit 3. Swept all 11 plausible values of `v` in one
boot; `CAP2` tops out at `0x6`. The trigger is live and re-triggerable — `CAP2`
toggled `0x2 ↔ 0x6` eleven times — so this is a real ceiling, not a one-shot.

Clean 170HX baseline (cold boot, stock driver path): `CAP=0x00456101`,
`CAP2=0x00000002`.

Neither `0x8872c` nor `LnkCap2` is referenced in **any** ROM or ucode image. The
VBIOS never publishes the advertise; only our patch writes the trigger.

### Why every software write fails — two live models

`app08` writes `0x820520` unconditionally every boot (`st b32 D[$r15] $r9` at
`0xcdc9`). Our SEC2 payload writes the same address and is refused. What follows
from that is **not settled**; two models fit, and they imply opposite next moves.

**Model A — re-driven sense-chain output.** The `OPT_*` registers are not storage
at all; they are continuously re-driven outputs of the fuse sense chain, so a
write lands nowhere regardless of who issues it. Fits every observation: the
control flops in the *same* block (`EN_SW_OVERRIDE` `0x820040`, `FUSECTRL` /
`FUSEADDR` / `FUSEWDATA` `0x820000`–`0x820010`) are writable and persistent while
every readout refuses; `STATUS_OPT_*` is published `R-I4R`; `SENSE_CTRL` resolves
nothing because the array never changed; there is no `CTRL_OPT` bank for
`EN_SW_OVERRIDE` to override *from*. Note `NV_FUSE_OPT_NVDEC_DISABLE 0x820378` is
published `RW-4R` and still refuses, so "architecturally read-only" is *not* the
explanation — "written, then immediately re-driven" is, and it is
indistinguishable from a refusal under a flushed re-read.

**Model B — master- or window-gated.** Both writes are privileged falcon-context
writes and only one lands, so the accepted context differs.

The PLM evidence currently **argues against B**. Field layout is published
(`ls10/dev_falcon_v4.h`): `SOURCE_ENABLE` is bits 31:12, twenty PRI masters, and
that field *is* the master-identity check. At capture the 170HX read `0xffffffff`
across `0x8200d0`–`0x8200fc` — all four privilege levels for read and write, all
twenty sources enabled — and the OPT write was still refused. Decisive only if one
of those twelve governs `0x820520`, which nobody has established.

Model B also rests on an unverified premise: that `app08`'s store *lands*. The only
evidence is the A100 reading `OPT_MAGIC = 0x00200000`, but bit 21 is set on the
170HX too (`0x16680000` contains it), so the fuse carrying bit 21 on both parts and
`app08`'s store being as inert as ours explains the data equally well.

**Decider — `OPT_WRSWEEP`, one boot.** Walk `0x820100`–`0x8207fc` (448 dwords):
read, flip one bit, write, flush 32 reads through `PMC_BOOT_0`, re-read, count.
`0/448` ⇒ model A, and there is no master to find. Any nonzero ⇒ model B, and the
ones that stick are the map. Not yet written.

Under either model, `EN_SW_OVERRIDE=1`, `SENSE_CTRL` re-sensing and the fuse-block
PLM being open were addressing something that was never being checked — and the
PLM one was doubly moot: `0x8200fc` already reads `0xffffffff` on the **stock**
A100, so `0001`'s open of it has always been a no-op.

Full argument: [doc 24](24-red-team-of-23.md) §W1–W3.

---

## Closed routes

Each tested on-card and closed, not assumed.

| route | result |
|---|---|
| `0xcb00` / `0x118f78` gate | all four witnesses identical on a Gen4 part — never was the mechanism |
| `0x820584` | `1` on both parts |
| `EN_SW_OVERRIDE` `0x820040` | **writable and persistent** (doc 07's Q1, answered) — but `0` on both parts, so not a differentiator, and insufficient alone |
| `STATUS_OPT_*` `0x820Cxx` | RO by architecture (`t234/dev_fuse.h`: `R-I4R`) |
| `CTRL_OPT_*` bank | does not exist on this chip |
| legacy fuse alias `0x21000` | Turing-era base; writes do not reach the Ampere shadow |
| fuse array | 256 rows × 32 bits, perfect mirror, **no repair or override bank** |
| `FUSECTRL SENSE_CTRL` | executes; does not re-resolve `OPT_*` |
| XP straps `0x8c0xx`–`0x8c4xx` | host-RO; the one writable register moves nothing |
| XVE window `0x88000`–`0x88fff` | five registers took A100 values, `CAP2` unmoved |
| XP3G slots 0–3 | slot 0 was our own patch suppressing a bit; 1–2 match; 3 = `OPT_MAGIC`, overridable, not the gate |
| XP3G per-rate tables | read-only, `0/16` writes took |
| publish path `0x8872c` | bit 3 never granted, any input |

## Open routes

1. **VBIOS modification.** Make the write come from the context firmware writes
   in — i.e. change what `app08` does. Runs into image signing. Not investigated;
   a different class of problem and risk from everything above.
2. **HS code execution, redirected.** The harness works
   ([doc 20](20-hs-execution-surface.md)) but executes in the Booter's context —
   which is the context being refused. More capability there does not obviously
   help. It would matter only if a target is found that the Booter context *can*
   reach. **Blocked upstream:** under model A there is no accepted context to
   redirect *to*, and nobody has established which physical falcon runs
   `app08` — the VBIOS ucode table targets it at `0x01 DEVINIT`, enumerated
   separately from `0x05 PMU` and `0x07 GSP`, while our ROP runs on SEC2. If
   those are different engines there is no control-flow edge to find. Settle the
   engine offline before spending card time on gadget discovery
   ([doc 24](24-red-team-of-23.md) §W6).

3. **Measurement, then re-run `GEN3_TRY`.** `GEN3_TRY` already drives the MAC
   rate field to 3 (`0x8c040[19:18]`, `0x8c1c0 = 0x00340036`, `LnkCtl2`) —
   advertising Gen3 independently of `CAP2` is *tried*, not untried — but it ran
   blind. Without `LTSSM_STATE`, "the rate request is clamped" and "it entered
   `RECOVERY_EQZN` and fell back" are indistinguishable, and they lead to
   different next moves. The instrument is the SMBPBI mailbox, which is at
   **`0x200e0`** on a GPU, not the NVSwitch `0x660e0` the probe has been reading
   ([doc 24](24-red-team-of-23.md) §D1). The probe is ungated, so its `0x660e0`
   result is already in the `dmesg` history.

Not viable: burning fuses (OTP, irreversible, needs programming voltage).

---

## Capabilities gained

Independent of Gen3, these are real and reusable:

- **`EN_SW_OVERRIDE` is a writable, persistent register.** Open since doc 07.
- **The fuse macro is host-writable** and its READ path works. Array mapped:
  256 rows, `FUSEADDR` ignores bits 0 and 8, device-ID field at row 149 bit 17.
- **XP3G `OVR`/`VAL` defeat a fuse-derived value.** `STATUS3` forced from
  `0x16680000` to the A100's `0x00200000` and held. The only mechanism found
  that overrides a fuse-mirrored register rather than being refused by it.
- **File-driven HS ROP harness.** `recon/mkdmem.py` generates a payload,
  `RAW_BOOTER` executes it verbatim; validated by `EN_SW_OVERRIDE` going `0 → 1`
  from a cold start. Iteration is a file copy, not a rebuild.

---

## Method notes

Each of these cost a wrong conclusion before it was understood.

- **Volatility masks are mandatory.** Two back-to-back captures of the *same*
  card differ at ~39 offsets (PTIMER, counters, live PHY status). Without a mask
  the reference diff carries that many false positives, several inside the exact
  UPHY range under investigation.
- **Writes are posted.** An immediate readback returns the stale value.
  `0x88c28` read `REVERTED` on one run and read back as written at the start of
  the next. Verdicts require a flushed re-read (32 reads through `PMC_BOOT_0`).
  Any `REVERTED` verdict recorded before this fix is unreliable.
- **Disassemble falcon whole-file only.** Variable-length encoding means an
  arbitrary start offset produces plausible, wrong output. This produced a wrong
  gate address in doc 21 before the whole-file decode, and false gadget
  "confirmation" in doc 20.
- **The patch's own writes look like reference differences.** `0x8c040` bit 19
  was recorded as crippling; it was our Gen2 rate write. Any 170HX capture used
  as a baseline must come from a cold boot with probe keys unset. This has now
  happened three times; the known-contaminated offsets are:

  | offset | source | 170HX capture reads |
  |---|---|---|
  | `0x8c040` bit 19, `0x8c1c0`, `0x8c2c0` bit 2, `0x880a8`, `0x8872c` | `0007` Gen2 path | our values |
  | `0x82381c`, `0x823820` | `0001` SM-speed writes | `0x88888888`, `0x00000008` |
  | `0x823800`, `0x823804` | `0001`/`0007` PLM opens | `0xffffffff` (A100 stock: `0xffffff8f`) |
  | `0x8200d0`..`0x8200fc` | PLM opens, or a real part difference | `0xffffffff` — **unresolved** |

  The genuine `0x8238xx` deltas are `0x823808`, `0x82380c`, `0x823810`,
  `0x823814` (published `R--4R`, a readout), `0x823828` (A100 `7` / here `0`) and
  `0x82382c` (A100 `1` / here `0xa`). `0x823824` is `0x00000001` on **both**.
- **In-range is not verification.** "8 of 9 gadget addresses fall inside the
  image" was near-vacuous: any 16-bit value under `0xeb00` passes.
- **A single post-init snapshot cannot answer a question about a sequence.**
  Every capture taken before 2026-07 is state S1. `0x118f78` bit 30 reads `0` on
  both parts *after* init — and app08 contains two functions that clear that very
  bit, so "0 afterwards" and "never set" are different observations and only the
  first was ever made. Capture in states (`recon/capture-states.sh`); `rmmod` is
  not S0, because GSP has already run.

---

## Tooling

| path | purpose |
|---|---|
| `recon/ga100-bar0-dump.c` | read-only BAR0 capture; `--wide`, `--rom`, `--label`. Now covers `0x20000` (THERM / SMBPBI mailbox) and decodes the phase witnesses and the XP3G per-rate population count |
| `recon/capture-states.sh` | S0 / S1 / S2 boot-state ladder — captures BAR0 *in states*, which is what settles `0x118f78` bit 30 and what populates the per-rate set |
| `recon/kmod/` | kmod fallback where `iomem=relaxed` is unavailable |
| `recon/dump-diff.sh` | masked diff of two captures |
| `recon/volatile-offsets*.txt` | volatility masks |
| `recon/cmpx.sh` | one probe cycle without touching `modprobe.d` |
| `recon/mkdmem.py` | HS payload generator, `--verify` checks fidelity to `0001` |
| `recon/extract-booter.py` | pulls Booter ucode from the driver bindata |
| `recon/fuse-analyze.py` | fuse-array dump analysis |
| `tools/bin/envydis` | falcon disassembler (`-m falcon -V fuc5 -i`) |

Probe blocks in `0007`, all env-gated and inert by default: `GEN_EARLY`,
`GEN2_LATE`, `GEN3_TRY`, `FUSE_OVR`, `FEAT_WR`, `FEAT_DUMP`, `PLM_SWEEP`,
`GEN3_STRAP`, `XVE_PERMIT`, `XP3G_OVR`, `XP3G_LANE`, `FUSE_MACRO`, `FUSE_SENSE`,
`RAW_BOOTER`, `PUBSWEEP`.

> `FUSE_MACRO` drives a state machine and has wedged the card; recovery is a cold
> power cycle. Never run it, or any override probe, on rented hardware.

## Artifacts

```
a100-wide.txt, a100-wide-ver1.txt        first reference capture (9 windows)
a100-wide2.txt, a100-wide2-b.txt         second (18510 lines, adds 0x8e/0x8b/0x9a0/0x823/0x824)
170hx-wide.txt, 170hx-wide-2.txt         matching target captures
170hx-wide2.txt, 170hx-wide2-b.txt       matching wide2 captures
fuse-rows.txt                            fuse array dump
roms/a100-sxm4-80gb-G506.0212.00.01.rom  VBIOS via PROM aperture, matched to live registers
fwsec/booter_load_ga100_prod.bin         Booter ucode (encrypted)
```

## Doc map

- **Mechanism as built:** [01](01-cmpunlocker-and-unlock-mechanism.md),
  [10](10-gen2-breakthrough-hypothesis.md), [16](16-gen2-cap-reversion-fix.md)
- **Build / reproduce:** [04](04-toolchain-and-reproducibility.md),
  [11](11-debian-build-notes.md)
- **Reference diff, the core result:** [19](19-a100-reference-diff.md)
- **Proposal and its red team:** [23](23-gen3-proposal-and-red-team.md) →
  [24](24-red-team-of-23.md) — read 24 with 23; four of 23's claims are refuted
  from artifacts already in the repo
- **Firmware analysis:** [17](17-app08-phy-asymmetry.md),
  [18](18-pri-mapping-and-the-advertise-path.md), [21](21-app08-opt-magic.md)
- **HS execution:** [20](20-hs-execution-surface.md)
- **Superseded theory:** [02](02-pcie-gen-investigation.md),
  [05](05-open-questions-and-hardware-tests.md),
  [06](06-pcie-gen-attack-avenues.md),
  [07](07-fuse-override-and-static-recon.md),
  [08](08-vbios-mac-fuse-map-external.md),
  [09](09-onhw-pcie-gen-beta-result.md), [12](12-gen3-attack-plan.md),
  [13](13-gen3-synthesis.md) — read for history, not for current claims
- **Adjacent:** [03](03-firmware-reverse-engineering.md),
  [14](14-nvlink-and-p2p.md), [15](15-smbpbi-msgbox.md)

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

### Why every software write fails

`app08` writes `0x820520` unconditionally every boot (`st b32 D[$r15] $r9` at
`0xcdc9`). Our SEC2 payload writes the same address and is refused.

Both are privileged falcon-context writes. Only one lands. **The OPT bank is
gated by which master issues the write — or by a window that has closed — not by
privilege level.**

That retrospectively explains three failed approaches: `EN_SW_OVERRIDE=1`, the
fuse-block PLM being open (`0x8200fc = 0xffffffff`, set every boot by `0001`),
and `SENSE_CTRL` re-sensing. None addressed what was actually being checked.

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
   reach.

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
  as a baseline must come from a cold boot with probe keys unset.
- **In-range is not verification.** "8 of 9 gadget addresses fall inside the
  image" was near-vacuous: any 16-bit value under `0xeb00` passes.

---

## Tooling

| path | purpose |
|---|---|
| `recon/ga100-bar0-dump.c` | read-only BAR0 capture; `--wide` = 18510 lines; `--rom` reads the PROM aperture |
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

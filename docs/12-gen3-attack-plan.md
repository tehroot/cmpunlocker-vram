# 12 — Gen3 attack plan (synthesis of the 5-agent theorycraft)

> **[CORRECTED — see [doc 18](18-pri-mapping-and-the-advertise-path.md)]** Any claim below that
> `0x14118f78` (or the `0x14xxxxxx` range generally) is a reset-latched strap beyond the 16 MB BAR0
> aperture and out of software reach is **wrong**. Falcon addresses are PRI addresses:
> `falcon = 0x14000000 | pri`. `0x14118f78` is PRI `0x118f78`, inside the aperture, and is both
> readable and PL0-writable on-card.


> **SUPERSEDED by [doc 13](13-gen3-synthesis.md).** Kept for the lever map, the SI reasoning, the
> bandwidth table and the safety/recovery ladder — all still correct. **Stale:** the headline odds
> (55–60% / 25%), the ~70% "EQ auto-converges" number, Exp 0 (answered: `OPT_GEN3=1`), and Exp 1's
> oracle (was wrong; corrected below). Corrections are applied in place and marked **[CORRECTED]**.
>
> **Status: THEORYCRAFT, pre-hardware.** Synthesizes five parallel investigations (register/CYA, link
> equalization, A100-VBIOS diff, GSP-RM/fuse, PHY/SI + test harness) into one ranked plan for pushing the
> CMP 170HX (GA100) from the **confirmed Gen2** ([doc 10](10-gen2-breakthrough-hypothesis.md)) to **Gen3**.
> Nothing here is confirmed on hardware; every register lever is labelled by evidence strength. The runnable
> probe is [`recon/gen3-probe.sh`](../recon/gen3-probe.sh).

## Executive verdict
Three of the four things you'd expect to be hard are **not** the wall:

1. **The register deltas are trivial.** Gen3 is the Gen2 sequence with three literal edits: `MAX_RATE 0x8C040[19:18]` `2→3`, `TargetLinkSpeed` `2→3` on **both** ends, and (maybe) a `DIS_G3` chicken-bit clear. The rate encoding is header-solid (1-based: value N = Gen N; value 2 giving Gen2 confirms value 3 = Gen3).
2. **Equalization is probably firmware-handled, not a blocker (~70%).** On GA100 you don't hand-program Gen3 EQ — there is no host EQ-preset register and no EQ-bypass bit anywhere in the headers. EQ is run by the closed GSP-RM/LTSSM, and the *same firmware does Gen4 EQ on an A100* (byte-identical FwSec). You don't program EQ; you cause the firmware to run it.
3. **The A100 ROM holds nothing to steal.** GA100 devinit is Falcon handler *code*, not a data table, and it's byte-identical between the two SKUs. The entire difference is the fuse. No Gen3 EQ presets exist in either VBIOS to replay.

**The whole thing collapses to one binary question:** is the PCIe-gen fuse a **terminal SerDes gate** for Gen3, or just an **enforcement/cap input** the Gen2 CYA-bypass already defeated? Gen2 trained with `OPT_GEN23=1` untouched — proving the fuse is *not* terminal for Gen2. If Gen3 behaves the same, the identical trick should carry it. If Gen3 has a *second, independent* gate, it hits a wall Gen2 never saw.

- **If the fuse is enforcement-only for Gen3 (and EQ auto-converges): ~55–60%** Gen3 trains on a direct x16 slot.
- **If the fuse terminally clamps the Gen3 SerDes: ~25%** — you'd advertise Gen3 and train Gen2, mirroring the beta's advertise-Gen2/train-Gen1.

## The single most important unknown: `OPT_GEN3 @ 0x820580` — **[CORRECTED: answered]**
There is a **separate Gen3 fuse shadow** at `0x820580`, distinct from the `OPT_GEN23` fuse at `0x82057c`
(defined in `driver/patches/0007-pcie-gen2.patch:21,77`). NVIDIA can disable Gen3 *independently* of Gen2.

**[CORRECTED]** "Its value has never been captured" was wrong — **`OPT_GEN3 = 0x1`, blown**, captured
on-card at `message.txt:1757`. The print is `OPT=%08x/%08x/%08x` = `OPT_GEN23 / OPT_GEN3 / OPT_MAGIC`
(`0007-pcie-gen2.patch:43-45`); the on-card line reads `OPT=00000001/00000001/16680000`. So Gen3 *is*
independently fused off, and Exp 0 below is already done — unfavourable, but not disqualifying: `OPT_GEN23`
is equally blown and Gen2 trains anyway.

> **Reconciliation note.** The A100-diff agent (reading ROM *fuse-map data*) concluded "`OPT_GEN23` names
> Gen2+Gen3 as one fuse, no separate Gen3 gate." The RM/fuse agent (reading the *runtime patch*) found the
> distinct `0x820580` shadow. Not a contradiction: the ROM's fuse-map layout and the runtime fuse-shadow
> register are different views. Both agree **the gate is the fuse**; they differ only on whether Gen3 has its
> *own* fuse. That is exactly why reading `0x820580` is the gating experiment — it resolves the disagreement.

## The two readings, and what each predicts
| | **Enforcement-only** (doc 10 reading) | **Terminal SerDes gate** (doc 09 / field-manual reading) |
|---|---|---|
| What the fuse does for Gen3 | masks the *advertised/allowed* cap; LTSSM can still be driven past it via CYA + upstream retrain | physically disables the Gen3 SerDes/PLL; no register can wake it |
| `OPT_GEN3 @ 0x820580` | likely `0` (Gen3 just "never asked") | likely `1` (independently blown) — **[CORRECTED] observed `= 0x1`.** Points terminal, but is not decisive on its own: `OPT_GEN23` is also `0x1` and Gen2 trains |
| Clearing DIS_G3 + MAX_RATE=3 makes **LnkCap2 advertise Gen3?** | **yes** | **no** |
| Instrumented retrain outcome | LTSSM *attempts* 8 GT/s → EQ runs (converges → Gen3, or fails → falls back to Gen2 with an AER/EQ event) | LTSSM *never attempts* Gen3; stays Gen2, no EQ event |
| Next move if seen | solve EQ (route via GSP) → Gen3 | fuse-override (`EN_SW_OVERRIDE`) or accept Gen2 |

The experiments below are ordered to **distinguish these two readings as early and cheaply as possible.**

## The plan — ranked experiment ladder
Prefer a **direct x16 slot** — that is where the bandwidth prize is and where Gen3 SI is comfortable.
**[CORRECTED]** OcuLink is *not* disqualified as a register-method platform (see SI section); it is merely
x4-forever and SI-marginal at 8 GT/s. Confirm Gen2 via `retrain.sh` first as your known-good baseline.

### Exp 0 — Read the fuses — **[CORRECTED: DONE, `OPT_GEN3 = 0x1`]**
~~Dump `OPT_GEN23 (0x82057c)` and `OPT_GEN3 (0x820580)`.~~ Already captured on-card: **both blown**
(`message.txt:1757`, `OPT=00000001/00000001/…`). Per the decision rule below this says "Gen3 has its own
hard gate" — but note `OPT_GEN23` is *also* blown and Gen2 trains regardless, so a blown fuse has already
been shown non-terminal once. Skip to Exp 1. Tooling kept for reference: `driver/.build/fusedump/`, or the
pre-flight block of `gen3-probe.sh` (`recon/gen3-probe.sh:203` prints both).

### Exp 1 — The advertise test (near-read-only)
Apply only `clear DIS_G2` + `MAX_RATE=3` + candidate `DIS_G3` clears (bisect `0x8C2C0`, start bit 3), **no
retrain**, then read **LnkCap2** (`cap+0x2c`) supported-speed vector.

**[CORRECTED] The oracle is `0xE`, not `0x7`.** `LnkCap2[7:1]` is the Supported Link Speeds Vector —
bit1 = 2.5 GT/s, bit2 = 5.0 GT/s, bit3 = 8.0 GT/s (bit0 reserved). On-card the vector went `0x2` (Gen1) →
`0x6` (Gen1+Gen2) across the Gen2 crack (`message.txt:1845`). `0x7` sets the reserved bit and never sets
8 GT/s; the Gen3 value is **`0xE`**.
- `LnkCap2 = 0xE` → Gen3 advertised → the SerDes is willing → enforcement-only reading is likely right.
- `LnkCap2` stuck at `0x6` → the cap is clamped below Gen3 → terminal-gate reading gains ground.

### Exp 2 — Instrumented Gen3 retrain (the go/no-go)
Run `gen3-probe.sh` with **empty EQ placeholders** (autonomous-EQ baseline) and **`pci=noaer` removed** from
the kernel cmdline. Read the **hardware training signal**, not just the cap: `LnkSta2` (`cap+0x32`) EQ bits
(`EqualizationComplete`, `EqualizationPhase1/2/3`) + AER counters + `LnkSta` CurrentLinkSpeed.
- **Phase1 moves, Complete=0, fell back to Gen2** → the LTSSM *tried* 8 GT/s and EQ didn't converge → **EQ is
  the solvable target → Exp 3.**
- **No EQ bit ever moves, stayed Gen2** → LTSSM never attempted Gen3 → the block is the cap/fuse *upstream* of
  EQ → **terminal-gate territory → Exp 4.**
- **Trained Gen3** → do the **stability gate** before celebrating (a trained link can be high-BER): AER
  correctable-error counters + an `nvbandwidth`/CUDA stress run, then re-check errors.

### Exp 3 — Route the speed change through GSP (if EQ tried-and-failed)
Hand the speed change to *firmware* so it programs the EQ presets, instead of the raw-poke path entering
Recovery.Equalization with zero presets.

**[CORRECTED] `EnablePCIeGen3=1` writes `RMPcieLinkSpeed = 0x4`, not `0x5`.** `osinit.c:205` is a plain
assignment — `data = DRF_DEF(_REG_STR, _RM_PCIE, _LINK_SPEED_ALLOW_GEN3, _ENABLE)` — so `ALLOW_GEN3` `[3:2]`
= ENABLE and `ALLOW_GEN2` `[1:0]` is left at `_DEFAULT`(0), **not** `_ENABLE`(1). `0x5` is the value you
*want* (`GEN2_ENABLE | GEN3_ENABLE<<2`) and it is reachable only by setting `RMPcieLinkSpeed` **directly,
with `EnablePCIeGen3` unset** — `osinit.c:200-208` overwrites `RMPcieLinkSpeed` whenever `EnablePCIeGen3`
is nonzero, so setting both silently yields `0x4`. Field map at `nvrm_registry.h:1914-1937`.
*Caveat:* nothing in the open tree ever **reads** `RMPcieLinkSpeed` (only `osinit.c` writes it), so the
consumer is inside closed GSP-RM and the effect is not traceable from source — only from `LnkSta`/`LnkSta2`.

Then layer in the EQ agent's XP3G writes
(`0x8E110/120/11C/12C` OVR/VAL, PLM `0x8E1B0..BC`) into `gen3-probe.sh`'s `EQ_WRITES` block and retry. The
GSP path is `min()`-clamped by the supported cap, so this only helps *combined* with the CYA cap-bypass.

### Exp 4 — Fuse-override or retimer (if Gen3 never advertises / `OPT_GEN3=1`)
- **`EN_SW_OVERRIDE` @ `0x820040`** ([doc 07](07-fuse-override-and-static-recon.md)): the fuse-OPT override-enable that the beta never set
  before writing `OPT_GEN23`. If Gen3 is terminally fused, this is the only software lever left — and it's
  still untested. Enable it *first*, then attempt to override the gen fuse. High-risk, low-odds, but the last
  software card.
- **Retimer:** only ever needed for **signal integrity** on a long channel, and per the field manual a
  retimer/redriver **cannot force a higher rate** — it can't defeat a fuse. The old "retimer as lock-defeat
  interposer" idea ([doc 06](06-pcie-gen-attack-avenues.md) #4) is **mooted**; a retimer buys SI margin, not gen.

## Consolidated Gen3 lever map
| Lever | Addr / knob | Evidence | Role in the plan |
|---|---|---|---|
| MAX_RATE=3 | `0x8C040[19:18]` | field width EVIDENCE; 3=Gen3 strong inference | primary rate ceiling |
| TargetLinkSpeed=3, both ends | LnkCtl2 `cap+0x30[3:0]` | EVIDENCE (`ctrl2080bus.h`) | trains to min of both ends |
| Retrain from **upstream** | LnkCtl `cap+0x10` bit5 | EVIDENCE (the Gen2 crack) | the trigger |
| DIS_G3 chicken bit | `0x8C2C0` bit? (cand. 3) | **SPECULATION** — not in any header | bisect on-card |
| PRIV_MISC_1 Gen3 EN/VAL | `0x8841C` bits **[14:13]** **[CORRECTED]** | 2-bit/gen layout (doc 13): Gen2=[12:11], Gen3=[14:13], Gen4=[16:15]. "15/16" was Gen4's. Inert for Gen2 (XP route won) | belt-and-suspenders only — and see the **inverted-write** warning in [doc 13](13-gen3-synthesis.md) |
| RM policy allow-Gen3 | `RMPcieLinkSpeed ALLOW_GEN3 [3:2]` | EVIDENCE (`nvrm_registry.h:1914-1937`, `osinit.c:200-208`). **[CORRECTED]** `EnablePCIeGen3=1` ⇒ `0x4`, not `0x5` | necessary, not sufficient; use in Exp 3 |
| **`LOCK_AT_LOAD`** | `RMPcieLinkSpeed[31:31]` | **defined only** (`nvrm_registry.h:1935-1937`) — **no consumer anywhere in the open tree**, so GSP-side and semantics unverified | candidate answer to RM re-deriving the cap every link derivation; cheap regkey test |
| EQ presets / XP3G PHY | `0x8E110/120/11C/12C`, PLM `0x8E1B0..BC` | offsets EVIDENCE; EQ semantics unpublished | Exp 3 only; land before retrain |
| Supported-speed source | `0x85080[23:20]` → `0x85084[3:0]` | RM re-clamps each retrain; poison-walled | the wall the CYA path goes around |
| **`OPT_GEN3` fuse** | `0x820580` | **[CORRECTED] `= 0x1`, blown** — captured `message.txt:1757` | Exp 0 done; `OPT_GEN23` is equally blown and Gen2 still trains |
| `OPT_GEN23` fuse / strap | `0x82057c` / `0x14118f78` | RO / reset-latched, unreachable | never touched by the working path |

**Structural bound on this whole method:** `MAX_RATE` is `0x8C040[19:18]` — **2 bits**, values 0–3, 1-based
(value 2 gave Gen2 on-card). **Value 3 = Gen3 is the maximum the field can encode.** The XP-clamp approach
tops out at Gen3 no matter how the fuse question resolves; there is no Gen4 continuation of this path.

## What each agent contributed (confidence)
| Lens | Headline | Confidence |
|---|---|---|
| Register / CYA | 3 trivial deltas; DIS_G3 needs on-card bisection | register-path-alone ~20% |
| Equalization | EQ is firmware-run, not a host register; likely not the wall | EQ solvable/auto ~70% |
| A100-VBIOS diff | firmware byte-identical; difference is purely the fuse; nothing to replay | ROM yields a lever ~8% |
| GSP-RM / fuse | `ALLOW_GEN3` knob exists (98%); **separate `OPT_GEN3` fuse — [CORRECTED] now read: `0x1`** | CYA bypass extends ~35% |
| PHY / SI + harness | direct slot is Gen3-trivial SI; runnable recoverable probe | Gen3 trains if register+EQ solved ~55–60% |

## Signal-integrity + bandwidth reality
- **Direct x16 slot (main rig): SI is not the limiter.** Same Gen4-capable silicon an A100 runs Gen4 on; a
  few inches of PCB + one CEM connector is well inside the Gen3 channel budget. **No retimer/redriver needed.**
  If Gen3 fails here, it's the register/EQ/fuse layer, not the wire.
- **OcuLink x4 (R530): a poor Gen3 *train* target, but a valid register platform. [CORRECTED]** The
  original second ground — "`resource0` mmap returns EINVAL there, the register method can't even be
  applied" — is **falsified**: `tools/retrain.sh:88-90` mmaps `/sys/bus/pci/devices/<bdf>/resource0` and
  lands all three MMIO writes (`0x8C2C0`, `0x8C040`, `0x8872C`) on this exact rig, which is where **Gen2 was
  confirmed** (`LnkSta 5GT/s`). What remains against it: marginal 4 GHz SI across the eGPU connector chain,
  and physically x4 forever — so the *train* belongs on a direct slot, but Phase 0/1 register work does not.
- **Bandwidth payoff** (per-dir, per-lane ≈ G1 0.25 / G2 0.50 / G3 0.985 GB/s):

  | Config | per-dir | vs stock | where |
  |---|---|---|---|
  | Stock Gen1 x4 | 1.0 GB/s | 1× | anywhere |
  | Gen2 x4 (OcuLink now) | 2.0 GB/s | 2× | current |
  | **Gen2 x16** (direct + caps mod) | 8.0 GB/s | 8× | direct slot + solder |
  | **Gen3 x16** (direct + caps + Gen3) | ~15.75 GB/s | **~16×** | direct slot + solder + Gen3 |

  The real prize (Gen3 x16, ~16×) needs **all three**: direct slot + the 24× 0402 caps width mod + Gen3.
  OcuLink can never reach it. This is the decisive reason the card belongs in the main rig.

## Safety — the probe cannot cost you Gen2 / 64 GB
`gen3-probe.sh` touches **only** PCIe link registers (BAR0 `0x8Cxxx/0x88xxx/0x8Exxx` + config-space cap) —
never FBPA/memory geometry, never fuses/VBIOS/NVRAM. Every write is **volatile**. The memory unlock is
re-applied by the patched driver at every boot and Gen2 by the systemd oneshot, so any failure is at most
"reboot → stock Gen1 → auto-Gen2." Two hard rules the script enforces: **never install it as a service**
(non-persistent), and **never touch BAR0 after the retrain** (config-space-only recovery avoids SIGBUS on a
downed device). Recovery ladder: R1 upstream re-target+retrain (no GPU reset — preserves the live unlock) →
R2 Secondary Bus Reset → R3 remove/rescan → R4 cold cycle (always restores).

## Open unknowns, ranked
1. ~~**`OPT_GEN3 @ 0x820580` value**~~ — **[CORRECTED] ANSWERED: `0x1`, blown** (`message.txt:1757`).
   It does *not* settle terminal-vs-enforcement on its own: `OPT_GEN23` is blown too and Gen2 trains.
2. **Does clearing the Gen3-enable make LnkCap2 advertise Gen3 (`0xE`)?** — the advertise test (Exp 1) that splits the two readings. **Now unknown #1.**
3. **Does Gen3 EQ converge with autonomous presets on a clean slot?** — Exp 2's `LnkSta2` bits answer it.
4. **Where is DIS_G3** (if it exists at all) — on-card bisection of `0x8C2C0`/adjacent CYA. Stock value still uncaptured.
5. **Does the raw-poke path enter Recovery.Equalization with no presets** (→ needs the GSP route, Exp 3)?
   **[CORRECTED]** Still open — and the one wedge that occurred cannot answer it, because that boot carried
   `pci=noaer` (`message.txt:2`). See [doc 13](13-gen3-synthesis.md) §"The wedge".
6. **Does `RMPcieLinkSpeed[31] LOCK_AT_LOAD` stop RM re-deriving the cap each link derivation?** — no open-tree
   consumer, so GSP-side; testable with a regkey alone.

## Cross-references
- Confirmed Gen2 mechanism + discovery methodology: [doc 10](10-gen2-breakthrough-hypothesis.md)
- On-hardware beta result (advertise-Gen2/train-Gen1): [doc 09](09-onhw-pcie-gen-beta-result.md)
- Fuse-override architecture / `EN_SW_OVERRIDE`: [doc 07](07-fuse-override-and-static-recon.md)
- Field manual (the "terminal wall" reading, partially overturned for Gen2): [`recon/PCIE_GEN1_LOCK.md`](../recon/PCIE_GEN1_LOCK.md)
- The runnable probe: [`recon/gen3-probe.sh`](../recon/gen3-probe.sh)

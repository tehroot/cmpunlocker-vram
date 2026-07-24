# 12 — Gen3 attack plan (synthesis of the 5-agent theorycraft)

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

## The single most important unknown: `OPT_GEN3 @ 0x820580`
There is a **separate Gen3 fuse shadow** at `0x820580`, distinct from the `OPT_GEN23` fuse at `0x82057c`
(defined in `driver/patches/0007-pcie-gen2.patch:21,77`). NVIDIA can disable Gen3 *independently* of Gen2.
**Its value on the 170HX has never been captured** — only `OPT_GEN23=0x1` was ever read (doc 09).

> **Reconciliation note.** The A100-diff agent (reading ROM *fuse-map data*) concluded "`OPT_GEN23` names
> Gen2+Gen3 as one fuse, no separate Gen3 gate." The RM/fuse agent (reading the *runtime patch*) found the
> distinct `0x820580` shadow. Not a contradiction: the ROM's fuse-map layout and the runtime fuse-shadow
> register are different views. Both agree **the gate is the fuse**; they differ only on whether Gen3 has its
> *own* fuse. That is exactly why reading `0x820580` is the gating experiment — it resolves the disagreement.

## The two readings, and what each predicts
| | **Enforcement-only** (doc 10 reading) | **Terminal SerDes gate** (doc 09 / field-manual reading) |
|---|---|---|
| What the fuse does for Gen3 | masks the *advertised/allowed* cap; LTSSM can still be driven past it via CYA + upstream retrain | physically disables the Gen3 SerDes/PLL; no register can wake it |
| `OPT_GEN3 @ 0x820580` | likely `0` (Gen3 just "never asked") | likely `1` (independently blown) |
| Clearing DIS_G3 + MAX_RATE=3 makes **LnkCap2 advertise Gen3?** | **yes** | **no** |
| Instrumented retrain outcome | LTSSM *attempts* 8 GT/s → EQ runs (converges → Gen3, or fails → falls back to Gen2 with an AER/EQ event) | LTSSM *never attempts* Gen3; stays Gen2, no EQ event |
| Next move if seen | solve EQ (route via GSP) → Gen3 | fuse-override (`EN_SW_OVERRIDE`) or accept Gen2 |

The experiments below are ordered to **distinguish these two readings as early and cheaply as possible.**

## The plan — ranked experiment ladder
Run on the **main rig, direct x16 slot** (OcuLink is out — see SI section). Confirm Gen2 via `retrain.sh`
first as your known-good baseline.

### Exp 0 — Read the fuses (read-only, 30 seconds, most decisive per dollar)
Dump `OPT_GEN23 (0x82057c)` **and** `OPT_GEN3 (0x820580)` on the 170HX, and diff against an A100 if you can
get one. Tools: `driver/.build/fusedump/`, or just the pre-flight block of `gen3-probe.sh` (it prints both).
- `OPT_GEN3 = 0` → Gen3 not independently fused off → **enforcement-only reading gains ground; proceed.**
- `OPT_GEN3 = 1` (blown) → Gen3 has its own hard gate → jump to Exp 4 (fuse-override), lower your odds.
- *Caveat:* a `0` here isn't a guarantee (the terminal clamp could live in `OPT_GEN23`'s Gen3 portion), but a `1` is a strong stop sign.

### Exp 1 — The advertise test (near-read-only)
Apply only `clear DIS_G2` + `MAX_RATE=3` + candidate `DIS_G3` clears (bisect `0x8C2C0`, start bit 3), **no
retrain**, then read **LnkCap2** (`cap+0x2c`) supported-speed vector.
- LnkCap2 now advertises Gen3 (`0x7`+) → the SerDes is willing → the enforcement-only reading is likely right.
- LnkCap2 still Gen1/Gen2 only → the cap is fixed below Gen3 → terminal-gate reading gains ground.

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
Set `EnablePCIeGen3=1` (module regkey → the driver writes `RMPcieLinkSpeed = ALLOW_GEN3_ENABLE`, i.e. `0x5`
for Gen2+3) so *firmware* owns the speed change and programs the EQ presets, instead of the raw-poke path
entering Recovery.Equalization with zero presets. Then layer in the EQ agent's XP3G writes
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
| PRIV_MISC_1 Gen3 EN/VAL | `0x8841C` bits 15/16 | SPECULATION; likely inert (XP route won for Gen2) | belt-and-suspenders only |
| RM policy allow-Gen3 | `EnablePCIeGen3=1` → `RMPcieLinkSpeed ALLOW_GEN3 [3:2]` | EVIDENCE (`nvrm_registry.h:1915-1939`, `osinit.c:200-208`) | necessary, not sufficient; use in Exp 3 |
| EQ presets / XP3G PHY | `0x8E110/120/11C/12C`, PLM `0x8E1B0..BC` | offsets EVIDENCE; EQ semantics unpublished | Exp 3 only; land before retrain |
| Supported-speed source | `0x85080[23:20]` → `0x85084[3:0]` | RM re-clamps each retrain; poison-walled | the wall the CYA path goes around |
| **`OPT_GEN3` fuse** | `0x820580` | **value on 170HX UNKNOWN** | **read it first (Exp 0)** |
| `OPT_GEN23` fuse / strap | `0x82057c` / `0x14118f78` | RO / reset-latched, unreachable | never touched by the working path |

## What each agent contributed (confidence)
| Lens | Headline | Confidence |
|---|---|---|
| Register / CYA | 3 trivial deltas; DIS_G3 needs on-card bisection | register-path-alone ~20% |
| Equalization | EQ is firmware-run, not a host register; likely not the wall | EQ solvable/auto ~70% |
| A100-VBIOS diff | firmware byte-identical; difference is purely the fuse; nothing to replay | ROM yields a lever ~8% |
| GSP-RM / fuse | `ALLOW_GEN3` knob exists (98%); **separate `OPT_GEN3` fuse, value unknown** | CYA bypass extends ~35% |
| PHY / SI + harness | direct slot is Gen3-trivial SI; runnable recoverable probe | Gen3 trains if register+EQ solved ~55–60% |

## Signal-integrity + bandwidth reality
- **Direct x16 slot (main rig): SI is not the limiter.** Same Gen4-capable silicon an A100 runs Gen4 on; a
  few inches of PCB + one CEM connector is well inside the Gen3 channel budget. **No retimer/redriver needed.**
  If Gen3 fails here, it's the register/EQ/fuse layer, not the wire.
- **OcuLink x4 (R530): out on two grounds** — marginal 4 GHz SI across the eGPU connector chain *and* the
  `resource0` mmap returns EINVAL there (the register method can't even be applied). Also physically x4 forever.
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
1. **`OPT_GEN3 @ 0x820580` value on the 170HX** — terminal-vs-enforcement hinges on it. *Read it (Exp 0).*
2. **Does clearing the Gen3-enable make LnkCap2 advertise Gen3?** — the advertise test (Exp 1) that splits the two readings.
3. **Does Gen3 EQ converge with autonomous presets on a clean slot?** — Exp 2's `LnkSta2` bits answer it.
4. **Where is DIS_G3** (if it exists at all) — on-card bisection of `0x8C2C0`/adjacent CYA.
5. **Does the raw-poke path enter Recovery.Equalization with no presets** (→ needs the GSP route, Exp 3)?

## Cross-references
- Confirmed Gen2 mechanism + discovery methodology: [doc 10](10-gen2-breakthrough-hypothesis.md)
- On-hardware beta result (advertise-Gen2/train-Gen1): [doc 09](09-onhw-pcie-gen-beta-result.md)
- Fuse-override architecture / `EN_SW_OVERRIDE`: [doc 07](07-fuse-override-and-static-recon.md)
- Field manual (the "terminal wall" reading, partially overturned for Gen2): [`recon/PCIE_GEN1_LOCK.md`](../recon/PCIE_GEN1_LOCK.md)
- The runnable probe: [`recon/gen3-probe.sh`](../recon/gen3-probe.sh)

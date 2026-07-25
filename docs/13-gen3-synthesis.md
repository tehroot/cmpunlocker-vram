# 13 — Gen3 attack synthesis (7-agent forensic + theorycraft)

> **Superseded — see [doc 00](00-current-state.md) for current state.**
> Synthesis superseded by the A100 reference diff ([doc 19](19-a100-reference-diff.md)) and doc 00.


> **[CORRECTED — see [doc 18](18-pri-mapping-and-the-advertise-path.md)]** Any claim below that
> `0x14118f78` (or the `0x14xxxxxx` range generally) is a reset-latched strap beyond the 16 MB BAR0
> aperture and out of software reach is **wrong**. Falcon addresses are PRI addresses:
> `falcon = 0x14000000 | pri`. `0x14118f78` is PRI `0x118f78`, inside the aperture, and is both
> readable and PL0-writable on-card.


> Supersedes/corrects [doc 12](12-gen3-attack-plan.md). Built from the confirmed Gen2 mechanism on the
> user's own hardware (R530 + OcuLink, `LnkSta 5GT/s`), both fuses read blown, and the live Gen3 wedge.
> Stance: wall = not-yet-bypassed. `OPT_GEN23=1` was already bypassed for Gen2.

## Verdict (two numbers)
- **P(Gen3 gate is enforcement-only, i.e. reachable on a good channel) ≈ 35–40%, and this number is
  currently unsupported in both directions — see §"The wedge" below.** Up from the field manual's implied
  ~5%. What it legitimately rests on: **Gen2 trained with `OPT_GEN23=1` blown** — a blown gen fuse has been
  shown non-terminal once already. Held <50% by a *second dedicated blown fuse* (`OPT_GEN3=1`, confirmed)
  + mandatory Gen3 EQ.
  **[CORRECTED]** The original second pillar — "the Gen3 SerDes visibly engages 8 GT/s (the wedge)" —
  is **withdrawn**: the wedge was never instrumented (§"The wedge"). A later review moved the prior the
  other way, toward terminal-gate, on the grounds that this pillar was refuted — but that refutation's
  reasoning was never written down either. **Both the pro- and anti-evidence are undocumented; Phase 1 is
  what actually moves this number.**
- **P(Gen3 trains stably on the current R530 + OcuLink x4) ≈ 12%.** OcuLink Gen3 SI is marginal and already
  wedged once. **The train belongs on a direct x16 slot** (also where the bandwidth prize is: Gen3 x16 ≈
  15.75 GB/s/dir vs Gen3 x4 ≈ 3.9).
- Play: extract the terminal-vs-enforcement bit **cheaply and boot-safe**, then relocate the train to x16.

## Confirmed Gen2 model (the falsifier)
Minimal set = **6 writes** (`retrain.sh` does only these; the rest of `0007` is vestigial):
| Write | Reg | Value | Layer |
|---|---|---|---|
| clear `DIS_G2` | `0x8C2C0[2]` | 0 | XP CYA — LTSSM gate/advertise |
| `MAX_RATE` | `0x8C040[19:18]` | 2 | XP rate ceiling |
| `LTSSM` nudge | `0x8872C` | 6 | LTSSM |
| `TargetLinkSpeed` (both ends) | LnkCtl2 `cap+0x30[3:0]` | 2 | config |
| `Retrain-Link` (**upstream**) | LnkCtl `cap+0x10[5]` | 1 | the trigger |

- **Enforcement-layer model:** `OPT_GEN23=1` is sensed **once at reset** → seeds a *runtime-writable* XP clamp
  (`DIS_G2`+`MAX_RATE`). It is **not** a continuous SerDes gate. Proof: the clears stick, the fuse never
  re-drives them, and Gen2 trains with the fuse blown.
- ~~**`LnkCap`/`LnkCap2` = live reflection of the XP clamp**, not a frozen fuse latch (on-card `CAP2 0x2→0x6`
  when `DIS_G2`/`MAX_RATE` written).~~ **[CORRECTED — see [doc 16](16-gen2-cap-reversion-fix.md).]**
  `DIS_G2` clear + `MAX_RATE=2` **do not** move `CAP2` — measured twice (post-boot via
  `recon/retrain-diag.sh`, and in-driver: `CAP2=0x2` both before and after the XP writes). The cap follows
  the **trained rate**: it reaches `0x6` only once the link actually trains Gen2, after which the
  previously-rejected writes (`0x880A8`, `0x8C1C0`, GPU `LnkCtl2`) start being accepted.
  Not a frozen fuse latch either — so docs 02/03 stay overturned, but for the right reason.
- **Timing:** GSP-RM re-derives `0x85084`←`0x85080` every link derivation.
  **[CORRECTED]** `retrain.sh`'s "write after RM's derivation" framing is wrong for the reversion: `CAP2`
  is not re-clamped on a timer. It reverts because the link never trained Gen2 inside the Booter window,
  and by `nv.c` device-init time `0x88xxx` is PLM-re-locked. Fix is to retrain *inside* the window
  ([doc 16](16-gen2-cap-reversion-fix.md)), not to win a race.

## SEC2 Booter primitive — reach (decides what a Gen3 patch can touch)
- 1 HS/L3 arbitrary MMIO write per Booter load (~65 ms), gated on **readback** (status `0xffff` = normal).
- Reach = **16 MB BAR0 aperture**. Writes land in `0x88xxx/0x8Cxxx/0x8Exxx/0x820xxx`.
- **Hard-RO even at HS:** `OPT_GEN23 0x82057c` (write failed ×2 on-card). `OPT_GEN3 0x820580` presumed same class.
- **Poison-walled to GSP (not reachable from the SEC2-postbl ROP):** `0x85080/0x85084` read `0xBADF1100`,
  writes dropped. ← the RM source is one priv-domain above this primitive.
- **Out of reach:** strap `0x14118f78` (>16 MB, reset-latched).
- **Untested & reachable:** `EN_SW_OVERRIDE 0x820040` (PLM openable, never written).

## Corrected facts (vs doc 12)
- **`OPT_GEN3 @0x820580 = 0x1`** — CONFIRMED on-card (`message.txt:1757,1845`; `OPT=…/00000001/…`). doc 12's
  "never captured" is wrong; Exp 0 is already answered (unfavorable, not disqualifying).
- **`PRIV_MISC_1` Gen3 EN/VAL = bits [14:13], not 15/16.** Under RM's 2-bit/gen layout:
  Gen2=[12:11], Gen3=[14:13], Gen4=[16:15] (so doc 12's "15/16" was Gen4's pair).
  The Gen2 patch already wrote it (`0x20340500→0x20342d00` = sets bits 11,13) and it was **inert** —
  the XP `DIS_G2` route won.
  **⚠ [CORRECTED] That write is *inverted*, not merely inert.** `0007-pcie-gen2.patch:166-168` (and again
  "late" at `:278-281`) computes
  `misc1Want = (misc1 | (bit11|bit13)) & ~(bit12|bit14)` — i.e. it sets **override-ENABLE with VAL = 0 for
  both Gen2 *and* Gen3**. If `PRIV_MISC_1` is ever live, that forces both generations **off**, the exact
  opposite of intent. Harmless today only because the register is inert on the confirmed path.
  **Before Phase 2 opens any PLM that could make it live, fix or drop this write.** The macro names hide it:
  `PCIE_GEN2_PRIV_MISC_1_GEN2_EN` = `bit11|bit13` lumps *Gen3's* enable under a "GEN2" name, and
  `..._GEN2_VAL` = `bit12|bit14` likewise. Left in the patch deliberately — changing it would invalidate the
  confirmed-Gen2 baseline.
- **`0x8C2C0` ≈ `NV_XP_PL_CYA_0(0)`** (`0x40` below `CYA_1(0)=0x8C300`). `DIS_G3` candidates: **bit3 (~45%)**,
  none/MAX_RATE-only (~20%), bit4 (~15%). Stock `0x8C2C0` value never captured — capturing it (any bit >2 set
  = an enforcing `DIS_G3` exists) is a top read.

## Conflicts resolved
1. **Advertise test — valid SAFE gate, contra #7's dismissal.** On the *confirmed* rig, Gen2 has `LnkCap2=0x6`
   AND trained (`LnkSta 5GT/s`). Advertise (`CAP2→0xE`) is a **necessary precondition** and is boot-time /
   retrain-free / cannot wedge. It is *not sufficient* — sufficiency = `LnkSta2` EQ-phase bits after a retrain
   (riskier). So: advertise test = safe gate; EQ-bit test = train confirmation. Both used, in order.
2. **#5 (route EQ via GSP) ↔ #6 (GSP-EQ is walled).** #6 is right that the RM's EQ programmer (`0x4DFC854`
   Gen3 branch) is gated behind fuse-capped `0x85084`, and `0x85080` is poison-walled **from the SEC2 ROP**.
   BUT #7's H3: the poison is a property of *that one context*; `0x85080/0x85084` may be writable from the
   **driver's own `GPU_REG_WR32`** after opening the BIF/XVE PLM cmpunlocker never opened. → **Untested. The
   single pivotal unknown.** If driver-context reaches `0x85080`, the GSP-EQ route (and the wedge cure) opens;
   if poison there too, we're limited to autonomous EQ + host-side presets.
3. ~~**The wedge = EQ entered `RECOVERY_EQZN` (0x4) with no presets + no fallback owner.**~~
   **[CORRECTED — moved to OPEN, see §"The wedge".]** This was filed as resolved but was never measured.
   The *hypothesis* is unchanged and still the best one — cure: route the speed change through GSP so its
   vendor EQ+downgrade state machine runs (the raw poke bypasses it), plus driver self-heal (re-program the
   Gen2 snapshot on failure before returning control) — but it is a hypothesis, not a finding.

## The wedge — OPEN, not resolved **[CORRECTED]**
The claim "EQ entered `RECOVERY_EQZN` (0x4)" has **no supporting measurement anywhere in the artifacts.**
- `RECOVERY_EQZN = 0x4` is the **enum definition** from `smbpbi.h:1328-1399`, not an observation.
- No captured LTSSM state exists: grep for wedge/RECOVERY/EQZN/LTSSM-state across `message.txt` and
  `recon/*.txt` returns nothing. `0x8C2C0` stock was never captured either (0 hits in `message.txt`).
- **The decisive detail:** that boot carried **`pci=noaer`** on the kernel cmdline (`message.txt:2`).
  AER is the one diagnostic that separates *"LTSSM attempted 8 GT/s and EQ failed"* from *"LTSSM never
  attempted Gen3"* — and it was switched off during the only event we have.

**The wedge is therefore uninterpretable in either direction.** It cannot be cited as evidence that the
Gen3 SerDes engages (the original 35–40% pillar), nor as evidence that it does not (the later
terminal-gate reset). Phase 0 fixes this for free: **boot without `pci=noaer`.**

**A direct LTSSM oracle does exist, out-of-band.** SMBPBI command
`NV_MSGBOX_CMD_ARG1_GET_PCIE_LINK_INFO_PAGE_6` returns `LTSSM_STATE` in bits `[4:0]`, fully enumerated at
`smbpbi.h:1329-1345` (`DETECT=0x0`, `POLLING=0x1`, `CONFIGURATION=0x2`, `RECOVERY=0x3`,
**`RECOVERY_EQZN=0x4`**, `L0=0x5`, …). That is exactly the reading needed to settle "did the LTSSM ever
attempt 8 GT/s". Constraint: **no in-band client exists** — `grep -rn "NV_MSGBOX_CMD" --include=*.c src/`
over the open tree returns nothing, so the msgbox is serviced GSP-side / by the OOB microcontroller and is
**not** reachable via `GPU_REG_RD32`. Getting it means SMBus access to the card (slot pins B5/B6, or the
board's SMBus header — plausible on a riser/dock). Worth the wiring before another Gen3 train attempt:
combined with AER it turns the next wedge from an anecdote into a measurement.

## EQ reality
- Every beta/patch reg (`0x8E1xx` XP3G, `0x8C1C0` PL_LINK_RATE, `MAX_RATE`, `DIS_G2`) = rate-force/access-mask.
  **Zero are EQ presets.** Real presets: UPHY `0x14118xxx` (GSP-owned, no host port) + config Secondary-cap
  Lane-EQ (RO on GPU endpoint, **writable on the root port `00:03.0`**). Coeff/FS/LF/`USE_PRESET` exist only as
  RO SMBPBI telemetry (`smbpbi.h:1328-1399`; LTSSM state `RECOVERY_EQZN=0x4`).
- Program-presets-ourselves (XP3G) = dead (wrong regs). A100-ROM harvest = void (firmware byte-identical).
- Host-side lever that needs no RM: seed **root-port Lane-EQ = P7** + "Perform Equalization" (Link Control 3),
  then autonomous HW EQ. Open question: does GA100 UPHY autonomously EQ Gen3 without the RM programming it?

## Ranked avenues (EV = odds × payoff × safety × cheapness)
| # | Avenue | Odds | Boot-safe | Note |
|---|---|---|---|---|
| 1 | **Read-only driver-context dump** (`0x85080` reachability, stock `0x8C2C0`, `LnkSta2` EQ bits) | info=1.0 | ✅ read-only | gates everything; zero risk |
| 2 | **Advertise test** (`DIS_G3`+`MAX_RATE=3` at boot, no retrain → read `LnkCap2`) | necessary gate | ✅ | `CAP2=0xE` = enforcement-only |
| 3 | **Coherent-state Gen3 via GSP route + self-heal, single retrain** (H1) | 0.20 | ✅* | needs #1 to show `0x85080` reachable; do on x16 |
| 4 | Feed `0x85080/85084` from driver post-PLM (H3) | 0.15 | ✅ | the pivotal untried injection point |
| 5 | Root-port Lane-EQ P7 preseed + autonomous EQ (H4) | multiplier | ✅ | convergence aid, not an enable |
| 5b | **`RMPcieLinkSpeed[31] LOCK_AT_LOAD=1`** — stop RM re-deriving the cap every link derivation | untested | ✅ regkey-only | **[NEW]** `nvrm_registry.h:1935-1937`; **no open-tree consumer** ⇒ GSP-side, semantics unverified. Directly targets the timing race `retrain.sh` currently wins by hand. Cheapest untried lever on the RM-policy layer |
| 6 | `EN_SW_OVERRIDE=1`→`OPT_GEN3=0` readback-gated (Q1) | 0.05–0.15 | ✅ | settles doc-07 Q1 either way; likely cosmetic (shadow not re-read; strap-latched) |
| 7 | Raw-poke coherent state + 1 upstream retrain (`gen3-probe.sh`) | 0.12 | ⚠ wedged once | only on x16, watchdog'd |
| 8 | Retimer/redriver — SI margin (iff enforcement-only) | conditional | ✗ HW | reframed: buys EQ margin, not rate |
| — | doc-12 Exp 0 "read `OPT_GEN3`" | — | done → **=1** | drop |

## Plan (staged, safe-first)
**Phase 0 — read-only baseline dump (boot-time, zero link risk).** Add `NV_PRINTF` to the `SEC2_DEBUG` block,
on the known-good Gen2 link, no Gen3 attempt.
**[CORRECTED] Boot this phase *without* `pci=noaer`** — the rig currently boots with it (`message.txt:2`).
Free at Phase 0, costs nothing on a link that isn't being retrained, and it is exactly what makes the next
wedge readable instead of uninterpretable (§"The wedge"). Do not wait until Phase 2 for this.
- `GPU_REG_RD32(0x85080)`, `(0x85084)` from **driver context** → real value or `0xBADF` poison? **Decides
  whether the GSP-EQ route is reachable** (resolves the #5↔#6 pivot).
- `0x8C2C0` + `0x8C2C4` + `0x8C300` array → stock; any bit >2 set = `DIS_G3` enforcing exists.
- `LnkSta2 (cap+0x32)` EQ bits baseline; reconfirm `OPT_GEN3=0x820580` on this card.

**Phase 1 — advertise test (boot-time, retrain-free, cannot wedge).** Add `DIS_G3` candidate clear +
`MAX_RATE=3` to the boot CYA block; **no `TargetLinkSpeed=3`, no upstream Retrain**. Read `LnkCap2` from dmesg.
Bisect `DIS_G3`: boot A = MAX_RATE=3 only; boot B = +`0x8C2C0[3]`; boot C = +bit4. Fold the `EN_SW_OVERRIDE`
Q1 rows into the same boots (free). Outcome:
- `CAP2=0xE` → **Gen3 advertised → `OPT_GEN3` is enforcement-only → green light to attempt the train.**
- `CAP2=0x6` (stuck G1+2) → advertisement clamped → fuse-override (avenue 6) or wall.

**Phase 2 — the train (only if Phase 1 passes; on a direct x16 slot).** Coherent state, one retrain, self-heal.

> **[CORRECTED] regkeys — pick exactly one, do not set both.** The old `EnablePCIeGen3=1`/`RMPcieLinkSpeed=0x14`
> is wrong twice over. (a) `0x14` = `ALLOW_GEN3_ENABLE`(0x4) | `ALLOW_GEN4_ENABLE`(0x10) — it silently
> requests **Gen4**, which is unmotivated and outside what `MAX_RATE`'s 2-bit field can even express.
> (b) Setting both is self-defeating: `osinit.c:200-208` **overwrites** `RMPcieLinkSpeed` whenever
> `EnablePCIeGen3` is nonzero, so you end up with `0x4` regardless. Field map `nvrm_registry.h:1914-1937`.
> - **`EnablePCIeGen3=1` alone** → `RMPcieLinkSpeed = 0x4` (Gen3 ENABLE, Gen2 left DEFAULT), or
> - **`RMPcieLinkSpeed=0x5` alone, `EnablePCIeGen3` unset** → Gen2 ENABLE | Gen3 ENABLE, both explicit. Preferred.
>
> Caveat on both: nothing in the open tree *reads* `RMPcieLinkSpeed` (only `osinit.c` writes it) — the
> consumer is closed GSP-RM, so the effect is observable only via `LnkSta`/`LnkSta2`, never from source.

Then: the chosen regkey + CYA-bypass + (if Phase 0 showed `0x85080` reachable) feed
`0x85080/0x85084` Gen3 + root-port P7 Lane-EQ preseed + `HW Autonomous Speed Disable=0` + **one** upstream
retrain, bounded (~100 ms → auto-recover to Gen2). Oracle = `LnkSta2` `EqualizationPhase1/2/3`/`Complete` +
`LnkSta` speed + AER (boot **without** `pci=noaer`).
- speed=3 + Complete=1 + AER-clean-under-stress → **Gen3 trained.**
- EQ phases latch but fell back to Gen2 → enforcement-only, now an EQ/SI problem → x16 ± retimer.
- No EQ bit moves → clamp upstream of EQ → fuse-override or accept Gen2.

## Cross-refs
- Confirmed Gen2: [doc 10](10-gen2-breakthrough-hypothesis.md) · prior plan (corrected here): [doc 12](12-gen3-attack-plan.md)
- Field manual (terminal-wall reading, falsified for Gen2): [`recon/PCIE_GEN1_LOCK.md`](../recon/PCIE_GEN1_LOCK.md)
- Fuse-override Q1: [doc 07](07-fuse-override-and-static-recon.md) · probe: [`recon/gen3-probe.sh`](../recon/gen3-probe.sh)
- On-card: `message.txt:1757,1810,1845` (OPT_GEN3=1, OPT_GEN23 hard-RO, LnkCap2 reflection)

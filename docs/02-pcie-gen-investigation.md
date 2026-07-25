# 02 — PCIe generation lock: the investigation

> **Superseded — see [doc 00](00-current-state.md) for current state.**
> Gen fuses now confirmed on live silicon (`0x82057c`, `0x820580`); the Falcon-only conclusion here was superseded by [doc 18](18-pri-mapping-and-the-advertise-path.md).


> **[CORRECTED — see [doc 18](18-pri-mapping-and-the-advertise-path.md)]** Any claim below that
> `0x14118f78` (or the `0x14xxxxxx` range generally) is a reset-latched strap beyond the 16 MB BAR0
> aperture and out of software reach is **wrong**. Falcon addresses are PRI addresses:
> `falcon = 0x14000000 | pri`. `0x14118f78` is PRI `0x118f78`, inside the aperture, and is both
> readable and PL0-writable on-card.


The central question of the session: **is the CMP 170HX's PCIe Gen1 limit a hard OTP fuse
(unbeatable in software) or a re-writable register the cmpunlocker Booter primitive could target?**
This document records the full arc, including two intermediate errors and how they were corrected.

## Verdict (final)
**The PCIe gen ceiling is a genuine hardware fuse — unbeatable by any software/register method.**
The fuse feeds a hardware-set `LnkCap2` (advertised supported-speeds) that firmware **never writes**;
there is therefore no register for cmpunlocker's "write any register" primitive to override. This
matches the community's empirical measurement. The only avenues are hardware: the **x16 caps mod**
(width) and a speculative **PCIe retimer interposer** (speed).

## Evidence chain (how we got there)

### A. Open-source driver analysis (610.43.03) — inconclusive but suggestive
- The PCIe link registers in the open modules are **read-only**: `NV_XVE_LINK_CAPABILITIES` (cfg
  `0x84`, `R--4R`), `PCIE_LINK_CAPABILITIES_2` (cfg `0xA4`, `SUPPORTED_LINK_SPEED 7:1 R-EVF`),
  `NV_XVE_LINK_CONTROL_STATUS.LINK_SPEED` (cfg `0x88`, bits 19:16 `R--VF`). The driver only **reads**
  them and forwards to GSP (`kbifGetGpuLinkCapabilities_IMPL`, `kernel_bif.c:1253`).
- The speed-**set** control `subdeviceCtrlCmdBusSetPcieSpeed_IMPL` is **declared but has no body** in
  open source → runs in **closed GSP firmware**.
- **No PCIe/gen/lane fuse** in any published `dev_fuse.h` (GA100 header has 109 defines, none PCIe).
  But the published header is a *curated subset* — can't rule out undocumented fuses.
- The **XP PHY register map** (`dev_xp.h` / `NV_XP_PL_LINK_CONFIG`) exists only for Maxwell/Pascal,
  **absent for Ampere** — no host-visible PHY link-config surface in open source.

### B. Community research (170th-street) — empirical, hardware-based
- **`fuse-register-reference`**: `FUSE_PCIE_GEN23_DIS` = `0x1` on 170HX vs `0x0` on A100;
  `FUSE_EN_SW_OVERRIDE = 0x0` (SW fuse override disabled); fuse bank read-only at runtime.
- **`runtime-pcie-unlock-attempt`**: five software paths tried on hardware, **all failed** →
  (1) `setpci` to `LnkCap2` (read-only, rejected); (2) root-port retrain (stays Gen1 — endpoint
  advertises Gen1 in TS1/TS2); (3) MMIO `nvapeek/nvapoke` (link regs PROT-protected, read `0xbadf`);
  (4) Falcon PRIV `0x14118f78` ("not in host BAR0; needs signed HS ucode"); (5) RAM-patch VBIOS
  (no TOCTOU — DMA-time verification). Conclusion: *"every software-only avenue eliminated."*
- **`the-falcon-security-architecture`**: Ampere HS signing (SHA-256 + RSA-3072) unbroken; GA100 uses
  **FalconUCodeDescV2** (Turing lineage, the generation OMGVflash exploited).
  **[Correction — see [doc 08](08-vbios-mac-fuse-map-external.md)]:** external Booter disassembly shows the
  VBIOS *content* integrity check is a **symmetric MAC** (Davies-Meyer + AES-KDF), not RSA — forgeable by
  extracting `csecret(2)` via DFA glitching. RSA-3072 is the *ucode-descriptor* layer; the content check
  that gates a modified VBIOS is the MAC, which reframes the firmware-mod frontier as key extraction.
- **`what-has-been-bypassed`**: PCIe Gen1 = BLOCKED; NVLink = BLOCKED (fuse + missing PCB); memory =
  "UNPROVEN"; compute FMA = app-layer workaround only.
- **`open-research-problems`**: rates GA100-V2 / FwSec analysis "most actionable"; suggests comparing
  the Type 0xE0 FwSec partition 170HX vs A100 PCIe ROM.

### C. Firmware reverse-engineering (this session) — see doc 03 for detail
Working from both VBIOS ROMs and a from-source Ghidra + `ghidra_falcon` toolchain:
1. The PCIe PHY config, including `0x14118f78`, is applied by a **devinit register-init table
   interpreter** (app `0x01`). The `0x14118f78` selector is `table[r11].value` from a **DMEM table**
   at a fixed address (`0x3e8`). The devinit reads **zero fuse registers**.
2. The devinit `0x14118f78` RMW code is **byte-identical** between 170HX and A100 (masks
   `0x3000/0x2000/0xc000/0x8000`); the difference is the **table data**, not the code.
3. **Fuses are read in partition `app 0x08`** (the largest) — it references the fuse block via
   **235 fuse-address table entries**; every other partition references zero fuses. app `0x08` also
   touches `0x14118f78` (5×) with hardcoded PHY/PLL config (bit-clears, a 500 MHz value).
4. **Decisive check:** the firmware references `LnkCap` (`0x14088084`) and `LnkCtl/Sta`
   (`0x14088088`) in the XVE config space, but **`LnkCap2` (`0x140880a4`, advertised supported-speeds)
   is absent from all 561 PCIe PRIV registers the firmware touches.** ⇒ `LnkCap2` is **hardware/fuse-
   set at reset, never firmware-written**. That is the gen gate, and there is no register to override.

## Corrections (where I was wrong, and why the final answer is trustworthy)
This is recorded deliberately, because the intermediate conclusions contradicted the community.

1. **Overreach #1 — "0x14118f78 has no register target / harder than compute-memory."** Early on I
   concluded the gen cap had "no identified register target," leaning dead-end but for the wrong
   reason (missing XP map), then later drifted toward "register-overridable."
2. **Overreach #2 — "the devinit is table-driven, not fuse-gated ⇒ community may be wrong / Gen2
   plausible."** This came from scanning only app `0x01`/`0x45` for fuse *immediates in code*. But a
   **table-driven engine reads fuse addresses from the table data, not code** — so a code-immediate
   scan is blind to it by construction. I mistook "no fuse in the code I read" for "no fuse anywhere."
3. **The correction (user skepticism → deeper tracing).** Pushed to reconcile with the community's
   empirical result, we (a) localized the fuse reads to app `0x08` (235 table entries), and (b) found
   `LnkCap2` is never firmware-written. Both **confirm** the fuse-gated picture. **The community's
   hardware measurement was right; my static inference was wrong.** Epistemic lesson: direct hardware
   measurement outranks incomplete static RE, and "not found in the partition I read" ≠ "not present."

Also corrected: **`0x14118f78` is a PHY *side-config* register** (one of ~30 in the `0x14118xxx` PHY
block), **not** the gen gate. Deeply tracing it was informative but off-target; the gate is `LnkCap2`.

## Reconciling the "Gen2 capability" reports
Conflicting data points: cmpunlocker's binary README claims **"Gen2 x4"**; `nvidia-smi` on the card
reportedly shows **"Max Generation: 2"**; yet the community read **`LnkCap2` = Gen1-only** and every
runtime attack failed.

**Key reconciliation:** *reaching Gen2 ≠ beating the fuse.* Our finding (`LnkCap2` is fuse-set and
firmware-untouchable) means software can't **raise** the ceiling — it says nothing about **whether the
fused ceiling is Gen1 or Gen2.** If the fuse leaves Gen2 in the supported set, reaching Gen2 is
*negotiating up to the fused max via a directed retrain*, fully consistent with the cap being fused.
If `LnkCap2` is truly Gen1-only, then per PCIe spec Gen2 isn't negotiable at all and the "Gen2" claim
is overstated (the better-supported reading, given the community's careful evidence).

**Resolved on hardware ([doc 09](09-onhw-pcie-gen-beta-result.md)):** a beta branch made the card
*advertise* Gen2 at every layer (`CAP`, `CAP2`, `LC2`) and it still **trained Gen1 x4** (`speed=1`). It
also found the fuse — `OPT_GEN23` @ `0x82057c` = `0x1` — and its write to `0x0` **failed**. So both
readings were right: the card can be made to *advertise* Gen2, but the fuse clamps the *trained* rate.
The "Max Generation: 2" reports are the advertised cap; the real link is Gen1.

- **Certain:** Gen3/4 are fused out. Software cannot exceed the fused ceiling.
- **Unresolved:** Gen1 vs Gen2 fused ceiling — resolvable only by reading `LnkCap2` on the card
  (`setpci -s <bdf> CAP_EXP+2c.l`). See doc 05.

## Bandwidth math (both physical mods are independent & multiplicative)
Per-lane usable (8b/10b): **Gen1 = 0.25 GB/s/lane/dir; Gen2 = 0.5 GB/s/lane/dir.** PCIe is full-duplex.

| Config | per direction | bidirectional aggregate | vs stock |
|---|---|---|---|
| **Stock** — Gen1 x4 | ~1 GB/s | ~2 GB/s | 1× |
| Gen1 x16 (caps mod only, *demonstrated*) | **4 GB/s** | **8 GB/s** | 4× |
| **Gen2 x16** (caps + Gen2, *best case*) | **8 GB/s** | **16 GB/s** | 8× |

- The **x16 caps mod** (solder ~24 missing 0402 0.22 µF AC-coupling caps on lanes 4–15) is
  community-proven — but yields x16 at **Gen1** by itself.
- The extra 2× (Gen2) is the unresolved fused-ceiling question.
- **Gen3/4 (up to ~32 GB/s Gen4 x16) are fused out** and unreachable.
- 8 GB/s/dir ≈ Gen3 x8 — genuinely useful (e.g. makes two-card LLM tensor/pipeline parallelism viable,
  vs. unusable at stock ~1 GB/s).

## What remains (see doc 05 for test protocols)
- Read `LnkCap2` on the card → settles Gen1-vs-Gen2 ceiling and the cmpunlocker-Gen2 claim.
- The retimer interposer (attack the physical negotiation on the wire) is the only remaining *speed*
  avenue, and only viable if the PHY is physically Gen-capable (likely, being A100 silicon) — unproven.

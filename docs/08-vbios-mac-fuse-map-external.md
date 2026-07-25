# 08 — VBIOS MAC, CFG1 strap table & full fuse map (external analysis)

> **Superseded — see [doc 00](00-current-state.md) for current state.**
> The external fuse map's `OPT_GEN23`/`OPT_GEN3` naming is confirmed on live silicon. Its claim that `EN_SW_OVERRIDE` is inert is **wrong** -- it is writable and persistent.


> **Integrates an external GA100 VBIOS reverse-engineering analysis**
> ([JRex286 gist](https://gist.github.com/JRex286/84cd3921788d2ffbc1e9bf8b6f2c9396); contributors Petri
> Krohn — ECB cryptanalysis, Cab — license-region/padding). It is far deeper on the *VBIOS/firmware*
> side than our own RE (docs 01–07): it **corroborates** several of our conclusions, **corrects one key
> assumption**, and **opens new avenues**. Claims here are that source's unless marked ours. The
> highest-impact ones (symmetric-MAC verification, DFA key extraction, 32 GB physical) are credible and
> internally self-consistent, but we have **not independently verified them** — each is flagged where it
> bears on cmpunlocker.

## TL;DR — what changes for us
1. **Verification is a symmetric MAC, not RSA-3072** (corrects docs 02 / 05 / 06). Booter disasm:
   `_acrVerifySignature_TU10X` = Davies-Meyer hash + AES-KDF + memcmp. A symmetric MAC is *forgeable via
   key extraction* — so the firmware-mod frontier is **DFA glitching to recover `csecret(2)`**, not
   breaking RSA. Much less "impossible" than doc 05 assumed.
2. **The memory limit is a VBIOS CFG1 strap-table tier nibble** (`0x44`/`0x66`/`0x77` = 12/14/15 row
   bits = 2/8/16 GB per die). Our *runtime* CFG1 values encode the same tiers: `0x0277_9000` → tier **77**,
   `0x0266_9000` → tier **66**.
3. **⚠ Physical HBM is 32 GB (8 GB SKU) / 40 GB (10 GB SKU).** So our unlocks split:
   - **10 GB → 40 GB uses tier 66 (8 GB/die × 5 stacks = 40 GB) — matches physical. ✓**
   - **8 GB → 64 GB uses tier 77 (16 GB/die × 4 stacks = 64 GB) — but physical is 32 GB (8 GB/die × 4).**
     That **over-addresses 2× ⇒ the upper 32 GB aliases the lower 32 GB.** The physically-correct 8 GB
     unlock would be **tier 66 → 32 GB** (`CFG1=0x02669000`, same value the 10 GB card uses). See below.
4. **`EN_SW_OVERRIDE = 0` is reported "inert / cannot change"** and the CTRL_OPT fuse-override table is
   all-zeros → external evidence that **approach #1 (doc 07) is fuse-side / dead**.
5. **New avenues:** unsigned-tail 250 W→300 W power unlock (CH341A), HULK-cert injection into the 170HX
   license region (targets `FUSE_FEATURE_OVERRIDE 0x823800`), and DFA→`csecret` key extraction as the
   master key that unlocks memory/PCIe (MAC forgery) or everything (debug HULK).

---

## 1. ROM structure & the content MAC (corrects "RSA-3072")
ROM = 1,044,480 bytes. Verification of the signed body is a **symmetric MAC** (Davies-Meyer hash +
AES-KDF, `_acrCalculateDmhash_TU10X` / `_acrDeriveLsVerifKeyAndEncryptDmHash_TU10X`), **not RSA**.

MAC-protected ranges (from the RFRD manifest `field_0C` @ `0x2000`, whose `pci_option_rom_size` high byte
`0x200D` defines the signed size):
| Variant | MAC range |
|---|---|
| 170HX 250 W | `0x2200–0x43A00` |
| 170HX 300 W | `0x2200–0x43C00` |
| A100 PCIe | `0x2200–0x44400` |

Empirical (CH341A, 4 flash cycles): FF-padding **outside** the MAC range still boots; any byte change
**inside** it stalls the Booter (`GFW_BOOT=0x401`/`0x001`). Firmware body (`0x14A00–0x33800`) is
**AES-128-ECB**, **same key across all GA100 variants** (first 4 blocks `b2a93eaa0300209b…`; known
plaintext `AES_ECB(key,0xFF×16)=717d1494 eaca317f f1061952 58b38377`).

## 2. Memory: VBIOS CFG1 strap table vs cmpunlocker's runtime write
The memory nerf is a **CFG1 / RAMCFG strap table** (16 entries × 4 bytes per strap position). The **tier
nibble at entry+2** sets HBM row-address bits:

| Tier byte | Row bits | Per-die capacity |
|---|---|---|
| `0x44` | 12 | 2 GB (NERFED) |
| `0x66` | 14 | 8 GB (FULL) |
| `0x77` | 15 | 16 GB |

Strap-4 tier is the SKU differentiator: A100 = `0x66` (full), all 170HX = `0x44` (nerfed). The single
byte-flip that lifts it (170HX 8 GB) is `0x41D53: 0x44→0x66` (300 W: `0x41F53`) — but that byte is
**inside the MAC range, so a VBIOS flash needs MAC forgery** (`csecret(2)`, §4). Strap table @ `0x41D41`
(170HX 8/16 GB & 10 GB), `0x41F41` (300 W).

**cmpunlocker sidesteps the MAC entirely** by writing the *runtime* FBPA_CFG1 register (`0x9a0204`) after
opening PLMs — no VBIOS change, no MAC. Our CFG1 values carry the same tier nibble: `0x02779000` = tier
77, `0x02669000` = tier 66. This is the cross-link between the two analyses.

**⚠ The aliasing flag (our most actionable takeaway).** External physical capacity: **8 GB SKU = 32 GB**
(4 stacks × 8 GB, 8-Hi), **10 GB SKU = 40 GB** (5 stacks × 8 GB). Mapping our runtime unlocks onto that:

| SKU | cmpunlocker tier | Addressed | Physical | Verdict |
|---|---|---|---|---|
| 10 GB | 66 (8 GB/die) | 40 GB | 40 GB | **matches — correct** ✓ |
| 8 GB | **77 (16 GB/die)** | **64 GB** | **32 GB** | **2× over-address ⇒ upper 32 GB aliases** ⚠ |

If the external 32 GB figure is right, the physically-correct 8 GB unlock is **tier 66 → 32 GB**
(`CFG1=0x02669000` — the value the 10 GB card already uses), not tier 77 → 64 GB. This is directly
testable on-card (write-verify a unique pattern across the full 64 GB; if reads above 32 GB return the
low-32 GB data, it aliases) and is a prime suspect for any memory instability — and plausibly related to
the **ReBAR issue** (a BAR1 sized for 64 GB over 32 GB backing). **Do not change the shipped default on a
gist alone**; verify physical capacity first.

## 3. Full fuse map (corroborates + extends doc 07)
| Fuse | 170HX value | Effect | Bypass (per gist) |
|---|---|---|---|
| `SM_SPEED_SELECT_FFMA` (+8 more) | `0x5` (max throttle) | FP32-FMA 16:1, DP4A 16–20×, Tensor gating | **software only** (`-fmad=false`, cmppatcher) — this is what cmpunlocker's SS0/SS1 do |
| `FUSE_NVLINK_DIS` | `0x7` (all groups) | NVLink off | CTRL_OPT override (under investigation) |
| `FUSE_PCIE_GEN23_DIS` = **`OPT_GEN23` @ `0x82057c`** | `0x1` | Gen2/3 fused off | on-hardware write `0x1→0x0` failed ([doc 09](09-onhw-pcie-gen-beta-result.md)) — **but the override-enable (`EN_SW_OVERRIDE`) was never set first, so "immutable" is untested** |
| `FUSE_EN_SW_OVERRIDE` | `0x0` | CTRL_OPT fuse override disabled | **"cannot change — inert on 170HX"** |

Reconciliation with **approach #1** (doc 07): the CTRL_OPT override mechanism the gist names is exactly
the OPT-override path #1 targets. The gist reports it **gated off by `EN_SW_OVERRIDE=0` and "inert"**, and
the CTRL_OPT table (`0x47341`, 25 entries) reads **all-zeros on 13 probed GA100 cards**. That is external
evidence that approach #1's master unknown (Q1: is `EN_SW_OVERRIDE` flippable?) resolves **fuse-side →
#1 is a dead end**. Our own live-hardware Q1 write-test (doc 07 §recon step 4) is still worth running to
confirm, but expectations should drop accordingly.

## 4. New unlock avenues
- **Unsigned-tail power unlock (250 W→300 W).** The tail `0x43A00–0x47700` (15,616 B) is **outside the MAC
  range**. Power-limit reg `0x45E45`: `90 D0 03` (250 W) → `E0 93 04` (300 W), a 3-byte edit. Flashable
  via **CH341A** (no MAC break); **nvflash refuses** it (FwSecLic write-time check). The factory "170HX
  300 W" ROM is this exact change. *Cleanest real hardware win on this list.*
- **HULK certificate injection.** 170HX license region `0xFE000–0xFEFFF` (A100's is at `0xFF000`, beyond
  the nvflash dump window — 170HX's is **inside** it, so injectable *without* CH341A). HLK slot `0xFE504`
  (1113 B), zero-filled on stock but FWSECLIC scans it every boot. Lapsus-leaked production certs
  (`HULK_9970/12231/12549`) are signed with `csecret(40)`, `STRICT_ID_MATCH=NO`, and target
  **`FUSE_FEATURE_OVERRIDE (0x823800)`** — the same feature-override block cmpunlocker already writes at
  runtime. A valid HULK cert would make the override *persistent and firmware-blessed* instead of a
  per-boot register poke.
- **DFA → `csecret` key extraction (the master framework).** Because verification is symmetric, voltage
  glitching to recover keys is the real firmware-mod path: `csecret(6)` → ECB firmware decryption;
  **`csecret(2)` → MAC forgery → VBIOS memory (CFG1) + PCIe-speed unlock**; `csecret(0)` → debug HULK /
  `SKIP_VBIOS_SIG` (all verification off). Hard (glitch rig, precise timing) but a **known attack class**,
  not the RSA wall doc 05 assumed.

## 5. Reference data (corroborates ours)
- **Device/subsystem IDs:** 170HX 8/16 GB = `0x20C2` (subsys `0x1585`), 170HX 10 GB = `0x2082` (subsys
  `0x1557`); all FwSec NPDS = `0x2080`. Matches our `0x20c2`/`0x2082`.
- **PCIe x4 = 24 missing AC-coupling caps** → x16 restore. Matches doc 05.
- **Tools in the gist:** `z1_dump_and_parse_vbios.sh` (sysfs VBIOS dump + parse) and
  `z2_parse_vbios_table.py` (CFG1 strap table / tier bytes / training table / RFRD / PCIe-speed / FwSec
  device-ID parser). Complementary to our read-only `recon/probe-170hx.sh` — worth folding their parser
  logic into the recon flow.

## What this changes for the plan
1. **Add a memory-aliasing test** to the first-pass plan: verify true physical capacity (write-verify
   across 64 GB) before trusting the 8 GB→64 GB default; if it aliases, the correct 8 GB unlock is
   tier 66 → 32 GB. Likely relevant to the ReBAR/BAR1 mismatch.
2. **Down-weight approach #1** (doc 07): external evidence says `EN_SW_OVERRIDE` is inert. Keep the live
   Q1 write-test as confirmation only.
3. **The firmware frontier is DFA→`csecret(2)`, not RSA** — reframes doc 05/06 and makes VBIOS memory/PCIe
   forgery a (hard but bounded) key-extraction problem.
4. **Two genuinely new wins** worth adding to doc 06: the CH341A power unlock (250→300 W) and HULK-cert
   injection targeting `0x823800` (a persistent version of the compute unlock).

# 03 — Firmware reverse engineering (detailed)

All offsets are within the **NVGI-stripped inner VBIOS image** unless noted. The raw ROMs
(`roms/cmp170hx-bios-268495.rom`, `roms/a100-bios.rom`) are each 1,044,480 bytes and start with an
`NVGI` container; the classic `0x55AA`+PCIR VBIOS image begins at full-ROM **`0x5E00`** (170HX) /
**`0x5000`** (A100). Inner images saved as `fwsec/inner_170hx.rom` / `fwsec/inner_a100.rom`.

## ROM identities
- **170HX** — board `900-11001-0108-000` / `B05P699-11001`, PCIR devid **`0x20C2`**.
- **A100** — board `699-2G509-0200-ES1` (**PG509 = A100 SXM4** engineering sample), PCIR devid
  **`0x20B0`**; multi-SKU VBIOS also referencing PCIe ids (`0x20F1` ×12). This is the only A100 40 GB
  ROM on TechPowerUp (vgabios/277449). For die-level signing/FwSec analysis the variant is irrelevant
  (GA100-level); it only matters for a fuse-config byte-diff (no identical-PCB A100 exists for the
  custom 170HX board anyway).

## Falcon partition map (`FALCON UCODE TABLE`)
Table @ inner **`0x6354`** (170HX) / `0x5F44` (A100). Both expose the same six apps
(`{app_id, target, desc_ptr}`, target: 0x01=?, 0x05=PMU, 0x07=GSP):

| app_id | target | 170HX desc_ptr | role |
|---|---|---|---|
| `0x01` | 0x01 | `0x6954` | **DEVINIT** — register-init table interpreter; holds `0x14118f78` code |
| `0x08` | 0x01 | `0x2e21c` | **largest; the fuse processor** (235 fuse-table refs); also touches `0x14118f78` |
| `0x45` | 0x07 (GSP) | `0xe70c` | **FwSec** (HS security / WPR setup) — the "Type 0xE0" partition |
| `0x85` | 0x07 (GSP) | `0x1a4e8` | GSP (variant of FwSec) |
| `0x49` | 0x05 (PMU) | `0x262c4` | PMU |
| `0x89` | 0x05 (PMU) | `0x2a270` | PMU |

Descriptors: apps `0x45/0x85/0x49/0x89` use **FalconUCodeDescV2** (magic `0x003c0201`; pkc/sig fields
zero, split `imem_sec_base`/`size`); apps `0x01/0x08` use a V1-style descriptor (starts with imem
size). FwSec (`0x45`): imem `0xbda0`, sec-tail `0x9a0`; A100 FwSec imem `0xad60`, sec-tail `0x960`.

## FwSec comparison (170HX vs A100) — the "most actionable" community lead
Whole-partition byte-diff is **noise** (different VBIOS builds → ~100% diff by layout). Version-robust
comparisons instead:
- **Register footprint identical.** Distinct fuse-block regs referenced: 170HX **61**, A100 **62** —
  the only difference is the A100 reads one *extra* fuse (`0x820070`); **170HX-only = none**.
- **FwSec touches zero enforcement registers** (`0x14118f78`, fuses = 0 on both). It is purely
  security/WPR setup; enforcement is not in FwSec.
- **Decompilation diff** (Ghidra, `ghidra_falcon`): 170HX 35 funcs / A100 28 funcs; **23 exact-
  identical logic**, 1 near-identical; every "unique" function is an **artifact** (either a
  `halt_baddata()` truncation at a `ghidra_falcon` opcode gap at build-specific offsets, or the two
  builds factoring identical logic into different function boundaries). I/O footprint identical (both
  touch only `0x1c000`, a Falcon watchdog).

**Conclusion:** FwSec is *functionally identical* 170HX↔A100 → enforcement is in **fuse data**, not
FwSec code. Firmware modification is pointless (identical to A100's, and signed).

## The `0x14118f78` devinit code (trace)
Disassembled with envydis (`fuc5`). The RMW (inner `0x8c43`, Falcon `0x22ef`):
```
mov $r9 0x14118f78 ; ld b32 $r14 D[$r9]     ; read current value
and $r9 $r10 0x1                            ; test selector bit0
  or $r15 $r14 0x3000  /  clear+or 0x2000   ; conditional
  or $r15 $r14 0xc000  /  clear+or 0x8000   ; conditional
mov $r9 0x14118f78 ; st b32 D[$r9] $r15      ; write back
```
This RMW is **byte-identical** on both cards. The selector `r10` traces to:
```
ld b16 $r9  D[$r11]      ; entry.type
ld b16 $r10 D[$r11+0x2]  ; entry.value  ← the selector
bra $r9==0 -> 0x14118f78 handler
```
`r11` is a **devinit table pointer** set to fixed DMEM addresses (`mov $r11 0x3e8` / `0xba0` / `0xc30`)
and walked (`add $r11 $r11 0x4`). It's a classic NVIDIA **devinit register-init table interpreter**,
configuring ~dozens of `0x14xxxxxx` PCIe/XP PHY registers (`0x14001490`, `0x14118f78`, `0x141c5068` …)
from table data. **The devinit reads zero fuses** (its fuse addresses, if any, would be in table data,
not code immediates).

## Fuse reads localized → app `0x08`
Per-partition fuse-block reference count: app `0x01`=0, `0x45`=0, `0x85`=0, `0x49`=0, `0x89`=0,
**`0x08` = all of them**. The fuse addresses appear as **table data**: e.g. a fuse-mapping table @
inner `0x862`:
```
0x000862: 00820c14 00000001 00000000   ; {fuse reg 0x820c14, bit 0x01, ...}
0x00086e: 00820c14 00000002 00000000
   ...   (12-byte {register, mask, value} entries, reading the fuse bit-by-bit)
```
Similar tables at `0xedc`, `0x41741`, `0xc7928` (+`0x60000` copies). **235 total fuse-address
occurrences.** app `0x08`'s `0x14118f78` handlers do hardcoded PHY/PLL config (e.g.
`FUN_0000cf7c`: `_DAT_141c5068 = 500000000; _DAT_14118f78 &= 0xbfffffff;`) — bit-fields and a 500 MHz
value, **not** a fuse-gated gen selection.

**Architecture (reconciled):** VBIOS fuse-config tables (fuse addrs as data) → **app `0x08` reads
fuses & computes config** → propagates via DMEM → **devinit (app `0x01`) applies it** to the PHY
registers. Fuse-driven, exactly as the community measured; the "no fuse in devinit" observation is true
but downstream/irrelevant.

## The decisive check — `LnkCap2` is hardware/fuse-set
Single-pass scan of the inner image for the XVE config space in PRIV view (`0x14088000` = config base):
- **`0x14088084` (LnkCap, cfg 0x84)** — referenced ✓
- **`0x14088088` (LnkControl/Status, cfg 0x88)** — referenced ✓
- **`0x140880a4` (LnkCap2 advertised supported-speeds vector, cfg 0xa4)** — **absent** from all **561**
  distinct `0x14xxxxxx` PCIe/XP PRIV registers the firmware references.

`0x14118f78` sits inside a dense `0x14118xxx` PHY block (`…f10, f14, f18, f30, f34, f78, fb0, fb8…`) —
confirming it is PHY side-config. **`LnkCap2` — the advertised gen cap that governs link negotiation —
is never touched by firmware ⇒ it is set by hardware from the fuse at reset.** That is the gen gate,
and there is no register for any software primitive to override.

## Register reference (as understood)
| Register | Meaning | Notes |
|---|---|---|
| `0x0082381c` / `0x00823820` | SS0 / SS1 SM-speed feature override (compute) | writable → cmpunlocker writes them |
| `0x00823804` | FEAT PLM (QUADRO_WR_SEC gate) | opened by Booter exploit |
| `0x009a0204` | FBPA_CFG1 (memory geometry) | writable → cmpunlocker writes it |
| `0x009a0148` | FBPA PLM | opened by Booter exploit |
| `0x00100ce0` | MMU LOCAL_MEMORY_RANGE (memory) | writable → cmpunlocker writes it |
| `0x001fa7c4/cc`, `0x001fa824/828` | WPR / WPR2 | PLM + save-restore in exploit |
| `0x00820000`/`0004` | Fuse controller CMD / STATE | fuse read interface |
| `0x00820c14`, `0x82081c`, `0x820838…` | fuse OPT/status registers | read via app `0x08` tables |
| `0x14118f78` | PCIe **PHY** config (bit-fields/PLL) | table-driven; **not** the gen gate |
| `0x14001490`, `0x141c5068` | PCIe PHY config / 500 MHz PLL | table-driven |
| `0x14088084` / `0x88` | XVE LnkCap / LnkCtl-Sta (PRIV view) | firmware reads/writes |
| `0x140880a4` | XVE **LnkCap2** advertised speeds | **never firmware-written = fused gen gate** |

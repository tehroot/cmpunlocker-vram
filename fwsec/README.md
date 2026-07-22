# FwSec / VBIOS analysis artifacts

Binaries extracted from the two VBIOS ROMs in `../roms/` during the FwSec comparison.
See `../driver/.build/tools/FWSEC_COMPARISON.md` for the analysis and conclusions.

## Ghidra import
Import the `*_imem.bin` (and `*_sec.bin`) files with language **`falcon:LE:32:v5`**
(File → Import File → set Language → search "falcon"). Base address 0, no offset.
Provided by the `ghidra_falcon` extension (installed for Ghidra 12.0.4).

## Files

| File | Size | What it is |
|---|---|---|
| `inner_170hx.rom` | 0xF9200 | NVGI-stripped 170HX VBIOS (from `cmp170hx-bios-268495.rom`, image @ full-ROM 0x5E00) |
| `inner_a100.rom` | 0xFA000 | NVGI-stripped A100 VBIOS (from `a100-bios.rom`, image @ full-ROM 0x5000) |
| `fwsec_170hx_imem.bin` | 0xBDA0 | **170HX FwSec code** — Falcon app 0x45 (GSP HS security/WPR). Import as falcon fuc5. |
| `fwsec_a100_imem.bin` | 0xAD60 | **A100 FwSec code** — app 0x45. Import as falcon fuc5. |
| `fwsec_170hx_sec.bin` | 0x9A0 | 170HX FwSec **HS secure tail** (sig-validation region — the signing-analysis target) |
| `fwsec_a100_sec.bin` | 0x960 | A100 FwSec HS secure tail |
| `fwsec_170hx_full.bin` | 0xBDDC | 170HX app 0x45 full partition (descriptor + code + dmem) |
| `fwsec_a100_full.bin` | 0xAD9C | A100 app 0x45 full partition |
| `devinit_win_170hx.bin` | 1280 B | 170HX devinit slice @ inner 0x8800 — the `0x14118f78` PCIe RMW cluster |
| `devinit_win_a100.bin` | 2272 B | A100 devinit slice @ inner 0xBBC0 — same RMW for comparison |
| `devinit_win2_170hx.bin` | 1792 B | 170HX devinit slice @ inner 0x8600 |
| `fwsec_170hx.decomp.c` | 35 funcs | **Ghidra decompilation** of 170HX FwSec (falcon fuc5, headless) |
| `fwsec_a100.decomp.c` | 28 funcs | **Ghidra decompilation** of A100 FwSec (falcon fuc5, headless) |

## Decompilation notes
Produced headless: import as falcon:LE:32:v5 → linear-sweep disassembly (flow-following alone stalls
at opcodes ghidra_falcon can't decode) → auto-analyze → export C. Much of it is readable pseudocode
(I/O poll loops, mem copies, register RMW). Some functions truncate at `halt_baddata()` where the
ghidra_falcon fuc5 Sleigh lacks an opcode — for those spots the envydis listing (mature Falcon
disassembler) decodes more; cross-reference the two.

## Key findings recap
- FwSec code is **functionally identical** 170HX vs A100 (same register footprint, same fuse reads);
  enforcement is in **fuse data**, not patchable firmware. See FWSEC_COMPARISON.md.
- The `devinit_win_*` slices contain the byte-identical `0x14118f78` read-modify-write
  (masks 0x3000/0x2000/0xc000/0x8000) that programs the PCIe link config per the fuse selector.
- Signing frontier: GA100 uses FalconUCodeDescV2 (Turing lineage); the `*_sec.bin` HS tails are
  the target for any OMGVflash-class signature analysis.

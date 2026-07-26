# 22 — devinit_win_170hx.bin: extracted window into devinit IMEM

Extracted from `devinit_170hx_imem.bin`, not a standalone firmware.

## What it is

| Property | Value |
|---|---|
| **File** | `fwsec/devinit_win_170hx.bin` |
| **Size** | 1280 bytes (0x500) |
| **Architecture** | NVIDIA Falcon, LE, 32-bit |
| **Source image** | `devinit_170hx_imem.bin` (32184 bytes / 0x7db8) |
| **Offset in source** | 0x1eac |
| **End offset in source** | 0x23ab |

## Relationship to sibling files

```
devinit_170hx_imem.bin  (0x7db8 bytes, full image)
├─ [0x0000 – 0x1cac]    preamble / vector table / early init
├─ [0x1cac – 0x1eab]    win2-only preamble (0x200 bytes)
│   └─ devinit_win2_170hx.bin  (0x700 bytes, starts at 0x1cac)
│       ├─ preamble  (0x200)
│       └─ [0x1eac – 0x23ab]  ← devinit_win_170hx.bin  (0x500 bytes)
```

`devinit_win2_170hx.bin` = 512-byte preamble + `devinit_win_170hx.bin`. The two
are exact substrings of the IMEM image.

## Covered functions (decomp cross-reference)

The window spans **0x1eac–0x23ab**, covering the tail of `FUN_imem_00001f77`
through all of `FUN_imem_00004486`:

| Address | Function | Notes |
|---|---|---|
| 0x1f77 | `FUN_imem_00001f77` | Bad instruction data (partial, entry is mid-function) |
| 0x20a1 | `FUN_imem_000020a1` | Bounds-checked handler; returns 4 or 0x1f; calls `FUN_imem_00007573` |
| 0x23b4 | `FUN_imem_000023b4` | Bad instruction data |
| 0x23bb | `FUN_imem_000023bb` | Loop over entries, calls `FUN_imem_000020a1` 3×, processes 16-bit pairs from 0xc38/0xc3c/0xba0 |
| 0x274f | `FUN_imem_0000274f` | Conditionally sets param_5=9, ORs 0xf into param_2 |
| 0x2814 | `FUN_imem_00002814` | Calls `todo(..., 0xf8)`, bad instruction |
| 0x29c0 | `FUN_imem_000029c0` | Swaps two undefined4 values when param_1 == 0x1f |
| 0x29f0 | `FUN_imem_000029f0` | Reads `_DAT_dmem_14001478` bit 0 into output |
| 0x2a7e | `FUN_imem_00002a7e` | ORs 0x20 if `(in_r9 + 9)` is non-zero |
| 0x2bb5 | `FUN_imem_00002bb5` | Copies two undefined4 from r9 to param_3 |
| 0x2c19 | `FUN_imem_00002c19` | Returns 6 if param_1 != 5, else sets *param_2 = 2 |
| 0x2dd4 | `FUN_imem_00002dd4` | Sums 4 byte extracts into *unaff_r5 |
| 0x302f | `FUN_imem_0000302f` | Shifts/stores register values |
| 0x3293 | `FUN_imem_00003293` | Calls 0x302f, equality check against 2 |
| 0x34b6 | `FUN_imem_000034b6` | Calls `todo(..., 0x32)`, bad instruction |
| 0x39cf | `FUN_imem_000039cf` | Tight loop calling `todo` + `FUN_imem_000034b6` |
| 0x39e9 | `FUN_imem_000039e9` | Reads `_DAT_dmem_000051c8 + 0x80`, loops |
| 0x3a41 | `FUN_imem_00003a41` | Stores unaff_r8, bad instruction |
| 0x3d1e | `FUN_imem_00003d1e` | `io_read(unaff_r5 + 0x264)`, bad instruction |
| 0x3d20 | `FUN_imem_00003d20` | Same pattern as 0x3d1e |
| 0x42e1 | `FUN_imem_000042e1` | Returns 4 or 8 depending on r9 == 0xa5 |
| 0x4441 | `FUN_imem_00004441` | Calls `func_0x00004372`, swaps 16-bit halves on success |
| 0x4486 | `FUN_imem_00004486` | Bad instruction data |

## Notable patterns

- **PCI config / MMIO reads**: repeated `89cc 5100` / `89d8 5100` / `8ed8 5100` byte sequences
- **Return-value immediates**: `3e XX 1f 00` / `3e XX 20 00` patterns load 0x1f or 0x20 — standard return codes
- **Error handler**: `FUN_imem_000072af` called when `_DAT_dmem_00000714` doesn't match expected value (status/checksum gate)
- **Embedded data**: multiple `halt_baddata()` regions indicate data tables intermixed with Falcon instructions — variable-length encoding makes mid-table disassembly unreliable
- **`todo` calls**: unresolved indirect calls scattered throughout; likely jump table targets the decompiler couldn't resolve

## Verified

Substring offsets checked against the binaries:

```
devinit_win_170hx.bin    1280 B (0x500)   IMEM 0x1eac - 0x23ac
devinit_win2_170hx.bin   1792 B (0x700)   IMEM 0x1cac - 0x23ac
win2 ends with win: True     preamble 512 B (0x200)
```

Both are exact substrings of `devinit_170hx_imem.bin`, as stated.

**`devinit_win_a100.bin` (2272 B) is not a substring of the 170HX IMEM and is
not aligned to these windows.** The size difference between the 170HX and A100
"win" files is an artifact of different extraction bounds and carries no
information — it is not a content asymmetry, and was briefly mistaken for one.

## Relation to the advertise question

The window covers `0x1eac-0x23ac`. Both advertise-register sites in devinit lie
**outside** it: the `LnkCap` getter at `0x153f` and the `LnkCtl2` target-speed
switch at `0x3bac` ([doc 21](21-app08-opt-magic.md)). Both are reads; devinit
never writes the advertise.

The `0x23bb` loop's 16-bit pairs from `0xba0` are table data — `0xba0` is one of
the three devinit table pointers identified in
[doc 03](03-firmware-reverse-engineering.md).

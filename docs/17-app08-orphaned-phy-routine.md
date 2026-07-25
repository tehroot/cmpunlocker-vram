# 17 — An unreferenced link/PHY bring-up routine in the signed 170HX firmware

> Static result, no hardware. Derived from `fwsec/app08_170hx_imem.bin` and the two ROMs already in
> the repo. Prompted by [`docs/pry_pdf.pdf`](pry_pdf.pdf) §6.4: *"the higher-generation PHY per-rate
> calibration appears to be fuse-gated and is never run in the sequences we observed."*
> This doc identifies **why** it is never run, on this image.

## Result

`app08` — the fuse-processing partition ([doc 03](03-firmware-reverse-engineering.md)) — contains a
**376-instruction link/PHY bring-up routine at imem `0xcb30`–`0xcfaa`**, and the 170HX build of that
partition references **76 PHY-space registers the A100 build never touches** (A100-only: 1).

> **[CORRECTED]** The first version of this doc claimed the routine is *unreachable dead code*
> because neither it nor its predicate is a direct branch/call target. **That claim is withdrawn —
> the test was invalid.** See §"Reachability — unresolved". The register-footprint asymmetry and the
> disassembly below stand; the reachability conclusion does not.

## How it was found

The 170HX ROM references **more** UPHY registers than the A100 ROM, not fewer:

| ROM | distinct `0x14118xxx` | refs |
|---|---|---|
| `roms/a100-bios.rom` | 24 | 88 |
| `roms/cmp170hx-bios-268495.rom` | **30** | **124** |

Six appear **only** in the 170HX ROM, and all six live in `app08_170hx_imem.bin`:

```
1411823c  1411899c  14118b8c  14118bb4  14118e80  14118e90
```

(Each occurs twice in the ROM, 0x60000 apart — the primary and redundant image copies.)

This corrects [doc 12](12-gen3-attack-plan.md)'s "GA100 devinit is byte-identical between the two
SKUs, nothing to replay". That holds for **FwSec** ([`fwsec/`](../fwsec/) `FWSEC_COMPARISON.md`); it
does not hold for `app08`.

## Disassembly

`ghidra_falcon`'s decompilation (`fwsec/app08_170hx.decomp.c`) fails in this region — "Control flow
encountered bad instruction data". envydis handles it:

```bash
driver/.build/tools/envydis -m falcon -V fuc5 -i < fwsec/app08_170hx_imem.bin
```

`-V fuc5` is required; `fuc4`/`fuc3`/`fuc0` mis-decode. Output: 23155 lines, 249 unknown
instructions, 651 distinct branch targets, **zero indirect branches or calls**.

### The predicate at `0xcb12`

```
0000cb12: d9 3c 82 11 14   mov  $r9 0x1411823c
0000cb17: bf 99            ld   b32 $r9 D[$r9]
0000cb19: c7 99 2a         extr $r9 $r9 0xa:0xb      ; bits [11:10]
0000cb1c: b3 90 02 08      bra  b32 $r9 0x2 e 0xcb24 ; if == 2
0000cb20: 3d a4            clear b8 $r10             ; else return 0
0000cb22: f8 00            ret
0000cb24: d9 78 8f 11 14   mov  $r9 0x14118f78
0000cb29: bf 99            ld   b32 $r9 D[$r9]
0000cb2b: c7 9a 1e         extr $r10 $r9 0x1e:0x1e   ; return bit 30
0000cb2e: f8 00            ret
```

`0x1411823c[11:10] == 2` gates whether `0x14118f78[30]` is consulted at all. `0x14118f78` is the
register [doc 02](02-pcie-gen-investigation.md) identified as the PHY strap; the decompiled
`FUN_imem_0000cf7a` / `FUN_imem_0000cf7c` both do `0x14118f78 &= ~(1<<30)`.

### The routine at `0xcb30`

376 instructions, terminates `0000cfaa: f8 00 ret`, five internal calls (`0xc35`, `0xbfa`,
`0x8df2` ×3). Opens with a read-modify-write of a UPHY register, then a long programming sequence:

```
0000cb30: f9 52            mpush $r5
0000cb32: dd 90 8e 11 14   mov  $r13 0x14118e90
0000cb37: bf d9            ld   b32 $r9 D[$r13]
0000cb39: 4f e0 fc         mov  $r15 -0x320          ; clear bits 5,8,9
0000cb3e: dc 94 00 9a 14   mov  $r12 0x149a0094
0000cb43: fd 9f 04         and  $r9 $r15
0000cb46: 8f 00 08 04      mov  $r15 0x40800         ; set bits 11,18
0000cb4a: fd 9f 05         or   $r9 $r15
0000cb4d: a0 d9            st   b32 D[$r13] $r9
...      stores of 0x200000, 0x20004800, 0xe3c, 0x10000, 0x20000000,
         0x80000000, 0xc0000000, 0xe39fffff into 0x149a0090/0094/0154/0210/103c
```

Blocks touched:

| Block | Registers |
|---|---|
| UPHY | `14118e80 14118e90 14118b90 14118bb4 14118f78` |
| UPHY | `149a0090 149a0094 149a0154 149a0210 149a103c` |
| UPHY | `1413218c 14132518 14132844 1413744c 14137450 14137c00 14137f10` |
| BIF | `14088088 1408814c 14088150 14088488 1408b980 1408d110 1408e000` |
| misc | `141c5008 141c5068 141c509c 14820520 14820684` |

### Reachability — unresolved

Observations that are solid:

| Address | Direct branch/call target? | Data reference (16/32-bit)? |
|---|---|---|
| `0xcb12` predicate | no | no |
| `0xcb30` routine | no | no |
| `0xcb24` | yes — only from `0xcb1c`, the predicate's own branch | no |
| `0xcb00` (neighbour) | yes — from `0x15d4`, `0xc756` | — |

**Why this does not prove unreachability.** Taking "instruction following a `ret`/`mpopaddret`" as a
proxy for function starts and testing which are branch targets gives an orphan rate of **129/138
(93.5%)** on the 170HX image and **116/123 (94.3%)** on the A100 image. At that base rate the test
carries no signal. It also produces false negatives: `0xcf7a` has **26 confirmed `lcall` sites** yet
is not preceded by a `ret`, so the proxy misses it entirely.

A control-flow walk from the entry at `0x30` was attempted and is **broken** — it reports 4 of 23116
instructions reachable and marks `0xcf7a` unreachable. Not usable.

So: whether the routine is invoked on this part is **open**. Resolving it needs a real Falcon CFG
(Ghidra with base 0 and the `fuc5` Sleigh, or a purpose-built walker), not the scripting used here.
Note the image contains **zero indirect branches or calls**, which does constrain how it could be
reached — but external dispatch into a fixed imem address remains possible.

## Why no host-side register work could ever have reached this

These are `ld`/`st b32 D[…]` at `0x14118xxx` / `0x149axxxx` / `0x1408xxxx`. BAR0 is 16 MB
(`0xdd000000–0xddffffff` on the AM5 rig); `0x14118f78` is at ~336 MB. **Outside the host aperture
entirely.** Reachable only from Falcon. That is the structural reason every sweep in
[doc 16](16-gen2-cap-reversion-fix.md) and the `0007` regkey work — XVE `0x88xxx`, XP `0x8Cxxx`,
fuse-OPT `0x820xxx`, feature-override `0x8238xx` — was in the wrong address space for this problem.

## What it implies

*(Conditional on the reachability question above, which is unresolved.)*

Pry §5 obtains **arbitrary HS program-counter control**: the LS signature-verification routine
issues an unbounded DMA whose length is an adversary-controlled WPR-metadata field, and a uniform
fill with value `V` sets `__stack_chk_guard`, the saved on-stack canary and the saved return address
all to `V`, so the epilogue check `V == V` passes and the return loads `V` into the PC.

The routine at `0xcb30` is already resident and already signed. **`V` = its entry address invokes
NVIDIA's own calibration code** — no firmware modification, no re-signing, no reconstruction of the
sequence from scratch. cmpunlocker's existing payload path
(`kgspSec2PostblTimingRefillPayload` + `kgspExecuteBooterLoad_HAL`, `0001`) is the same bug shaped
to yield one arbitrary PRI write per Booter load; the difference is the overrun length and the fill
value.

## Open / unproven

1. **Reachability.** Unresolved — see above. If the routine *is* invoked normally, the premise of
   this doc collapses and the register asymmetry needs a different explanation.
2. **Identity.** That this routine is *the Gen3* per-rate calibration is inferred from the UPHY
   register set plus Pry §6.4, not proven. It may be bring-up for something else.
3. **Runtime IMEM base.** Addresses here are offsets into the extracted partition. The base at which
   `app08` is loaded must be confirmed before `0xcb30` is usable as a PC value.
4. **External dispatch.** Static analysis cannot exclude a caller outside this image entering imem at
   a fixed address. No address-taken reference exists in the image, which argues against it.
5. **Preconditions.** The routine sets up its own pointers in `$r12`/`$r13`/`$r14` and starts with
   `mpush $r5`; whether it needs inputs, and what state it assumes, is unread.
6. **Predicate inputs.** `0x1411823c[11:10]` and `0x14118f78[30]` are unreadable from the host, so
   their values on this card are unknown.

## Reproduce

```bash
driver/.build/tools/envydis -m falcon -V fuc5 -i < fwsec/app08_170hx_imem.bin > app08.dis
grep -n '^0000cb12:' -A 20 app08.dis          # the predicate
grep -c '0xcb30' app08.dis                    # 0 -> no callers
```

## Cross-refs
- Source paper: [`pry_pdf.pdf`](pry_pdf.pdf) §2.1, §5, §6.4, Table 1
- Register-space exhaustion on the host side: [doc 16](16-gen2-cap-reversion-fix.md)
- `app08` as the fuse-processing partition: [doc 03](03-firmware-reverse-engineering.md)
- `0x14118f78` as the PHY strap: [doc 02](02-pcie-gen-investigation.md)

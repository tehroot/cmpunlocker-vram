# 17 — `app08` PHY programming: 170HX vs A100, and a working Falcon CFG

> Static result, no hardware. Derived from `fwsec/app08_170hx_imem.bin`, an A100 `app08` extracted
> here for the first time, and the two ROMs in `roms/`.
>
> **This doc went through two wrong conclusions before the tooling worked. Both are recorded in
> §"What was wrong" so the mistakes aren't repeated.** The surviving results are below.

## Results

1. **The 170HX `app08` does substantially more PHY programming than the A100's.** In the
   `0x14xxxxxx` PHY/BIF space: **76 registers referenced only by the 170HX build, 1 only by the
   A100 build.**
2. **That code runs, behind a two-bit gate.** The 376-instruction link/PHY routine at IMEM `0xcb00`
   is called from IMEM `0x15a4`, guarded by the predicate at IMEM `0xcae2`:
   **`0x1411823c[11:10] == 2` AND `0x14118f78[30] == 1`**. If either fails, control goes to a
   reduced alternative at IMEM `0x7eea` instead. See §"The gate".
3. **IMEM address = file offset − 0x30.** The extracted `*_imem.bin` files include the 48-byte
   descriptor; the loader strips it.
4. **All of it is Falcon-only.** These addresses are far outside the 16 MB BAR0 aperture, so no
   host register access can reach them.

## Partition extraction

FALCON UCODE TABLE at inner `0x6354` (170HX) / `0x5f44` (A100), same layout in both:

| app | target | 170HX desc | A100 desc |
|---|---|---|---|
| `0x01` | `0x01` DEVINIT | `0x6954` | `0x9bec` |
| **`0x08`** | `0x01` DEVINIT | **`0x2e21c`** | **`0x2e7f4`** |
| `0x45` | `0x07` GSP | `0xe70c` | `0x10f64` |
| `0x85` | `0x07` GSP | `0x1a4e8` | `0x1bd00` |
| `0x49` | `0x05` PMU | `0x262c4` | `0x26a9c` |
| `0x89` | `0x05` PMU | `0x2a270` | `0x2a948` |

`FalconUCodeDescV2`, 48 bytes:

| field | 170HX | A100 | meaning |
|---|---|---|---|
| `[0]`/`[1]` | `0xfe64` | `0xe554` | stored / uncompressed size |
| `[3]` | `0x38` | `0x38` | interface offset |
| `[5]` | `0xd384` | `0xbc34` | imem load size |
| `[9]` | `0xd384` | `0xbc34` | dmem offset |
| `[11]` | `0x2ae0` | `0x2920` | dmem load size |

Layout: descriptor `0x00–0x30`, IMEM `0x30–[9]`, DMEM `[9]–[0]`. Check: `0xfe64 − 0xd384 = 0x2ae0`.

## Register-footprint asymmetry

Measured on both extracted partitions, `0x14xxxxxx` immediates in the linear disassembly:

```
170HX distinct: 498      A100 distinct: 423
170HX-only: 76           A100-only: 1  (1410a3c4)
```

The 170HX-only set is structured — a per-lane block, stride `0x40`, four registers per group:

```
14137678 1413767c 14137684 14137688
141376f8 141376fc 14137704 14137708
14137738 1413773c 14137744 14137748
14137778 1413777c 14137784 14137788
141377b8 141377bc 141377c4 141377c8
14137904 14137908 14137910 14137914
14137944 14137948 14137950 14137954
14137984 14137988 14137990 14137994
141379c4 141379c8 141379d0 141379d4
```

plus `1411823c`, the whole `149a00xx` group, `14132af4..14132b04`, `141321a4..141321b4`, and a
`14020xxx` group.

**This qualifies `FWSEC_COMPARISON.md`'s "functionally identical firmware".** That comparison covered
the fuse block `0x820000–0x824fff`, where the 170HX genuinely has no extra registers (170HX-only:
none; A100-only: one). In PHY space the asymmetry is large and runs the other way.

Note the direction: the *crippled* part carries more PHY code, and it executes. A plausible reading
is that the extra code is the limiting configuration — untested.

## The routine at IMEM `0xcb00` (file `0xcb30`)

376 instructions, ends `ret`, five internal calls. Called from IMEM `0x15a4`.

```
mpush $r5
mov   $r13 0x14118e90
ld    b32 $r9 D[$r13]
mov   $r15 -0x320          ; clear bits 5,8,9
and   $r9 $r15
mov   $r15 0x40800         ; set bits 11,18
or    $r9 $r15
st    b32 D[$r13] $r9
... stores of 0x200000, 0x20004800, 0xe3c, 0x10000, 0x20000000,
    0x80000000, 0xc0000000, 0xe39fffff into 0x149a0090/0094/0154/0210/103c
```

| Block | Registers |
|---|---|
| UPHY | `14118e80 14118e90 14118b90 14118bb4 14118f78` |
| UPHY | `149a0090 149a0094 149a0154 149a0210 149a103c` |
| UPHY | `1413218c 14132518 14132844 1413744c 14137450 14137c00 14137f10` |
| BIF | `14088088 1408814c 14088150 14088488 1408b980 1408d110 1408e000` |
| misc | `141c5008 141c5068 141c509c 14820520 14820684` |

## The gate

Caller, IMEM `0x1591`–`0x15b0`:

```
0158e: and   $r9 0x1
01591: bra e 0x159c              ; if (0x140012e0 & 1) == 0 -> gate path
01594: lcall 0x8034              ; else other path
01598: lbra  0x15b0
0159c: lcall 0xcae2              ; the predicate
015a0: bra b8 $r10 0x0 e 0x15ac  ; result == 0 -> skip the routine
015a4: lcall 0xcb00              ; the 376-instruction PHY routine
015a8: lbra  0x15b0
015ac: lcall 0x7eea              ; reduced alternative
015b0: (join)
```

So:

```
run 0xcb00  <=>  0x1411823c[11:10] == 2  AND  0x14118f78[30] == 1
else        ->   0x7eea
```

This is the mechanism behind Pry §6.4's "the higher-generation PHY per-rate calibration appears to be
fuse-gated and is never run in the sequences we observed" — two strap/fuse bits, with a fallback path
when either fails. Note `FUN_imem_0000cf7a` / `FUN_imem_0000cf7c` both perform
`0x14118f78 &= ~(1<<30)`, clearing the very bit the gate tests.

`0x14118f78` is in Falcon space, far outside the 16 MB BAR0 aperture, so the gate input cannot be set
from the host at any privilege level. With HS program-counter control it does not need to be: `V` =
IMEM `0xcb00` invokes the routine directly, past the gate.

## The predicate at IMEM `0xcae2` (file `0xcb12`)

```
mov  $r9 0x1411823c
ld   b32 $r9 D[$r9]
extr $r9 $r9 0xa:0xb       ; bits [11:10]
bra  b32 $r9 0x2 e +0x12   ; if == 2
clear b8 $r10              ; else return 0
ret
mov  $r9 0x14118f78
ld   b32 $r9 D[$r9]
extr $r10 $r9 0x1e:0x1e    ; return bit 30
ret
```

`0x1411823c[11:10] == 2` gates whether `0x14118f78[30]` is read. `0x14118f78` is the register
[doc 02](02-pcie-gen-investigation.md) identified as the PHY strap.

**[CORRECTED]** An earlier pass reported this predicate as unreached with no call edges. Wrong — it
is called from IMEM `0x159c`, immediately before the guarded call to `0xcb00`. The walk had decoded
that region under a bad alignment; a clean decode anchored at file `0x30` shows the `lcall` plainly.
The walk needs its anchor pinned rather than chosen per-target.

## The CFG walk

`ghidra_falcon` (targets Ghidra 11.1) loads into 12.0.4 and imports, but flow-following produces
**19 instructions** from the entry — the fuc5 Sleigh has decode gaps, as `fwsec/README.md` warns.
Ghidra is not usable for reachability here.

Working approach — recursive descent with envydis as the decode oracle, re-anchoring a fresh linear
decode whenever a target lands mid-instruction under the current alignment:

```
BASE = 0x30                      # file offset of IMEM 0
target_file = target_imem + BASE
envydis -m falcon -V fuc5 -i < image[anchor:0xd384]
```

Result: 2346 instructions, 71 call/jump edges. `-V fuc5` is required; `fuc4`/`fuc3`/`fuc0`
mis-decode.

## What was wrong

**First conclusion — "the routine is unreachable dead code".** Based on it not being a direct
branch/call target. Invalid: taking "instruction after a `ret`" as a proxy for function starts gives
an orphan rate of 129/138 (93.5%) on the 170HX image and 116/123 (94.3%) on the A100 image. No signal
at that base rate.

**Second conclusion — "IMEM address == file offset".** Based on a byte-pattern search finding 26
occurrences of `7e 7a cf 00` (`lcall 0xcf7a`) that appeared to resolve coherently. Those were
**byte-pattern false positives, not decoded instructions.** The real entry does `lcall 0xcf7c`; under
IMEM == file that lands mid-instruction, under IMEM = file − 0x30 it lands on `mpush $r0` at file
`0xcfac`, a proper prologue. The walk only works under the latter — 19 instructions vs 2346.

The lesson for anything downstream: byte-pattern searches for call encodings in Falcon images are
unreliable, and any address claim should be validated by whether a CFG walk closes.

## Open

1. **Identity.** That the `0xcb00` routine is per-rate PHY calibration is inferred from its register
   footprint plus Pry §6.4, not proven.
2. **What it does on this part.** It runs; whether it configures the link *up* or *limits* it is
   unread. Its internal conditionals have not been traced.
3. **Gate inputs.** `0x1411823c[11:10]` and `0x14118f78[30]` are unreadable from the host, so their
   values on this card are unknown. The gate structure is read from code, not observed.
4. **Internal branches.** At least one further conditional inside the routine, IMEM `0xcce2`, tests
   `0x14820684 & 7`. Untraced.
5. **Coverage.** 2346/23k instructions reachable from the entry. The remainder is either genuinely
   unreachable, reached via paths the walk misses, or data.

## Reproduce

```bash
driver/.build/tools/envydis -m falcon -V fuc5 -i < fwsec/app08_170hx_imem.bin > app08.dis
# IMEM addr = file offset - 0x30
```

## Cross-refs
- [`pry_pdf.pdf`](pry_pdf.pdf) §2.1, §5, §6.4, Table 1
- Host-side register exhaustion: [doc 16](16-gen2-cap-reversion-fix.md)
- `app08` as the fuse-processing partition: [doc 03](03-firmware-reverse-engineering.md)
- `0x14118f78` as the PHY strap: [doc 02](02-pcie-gen-investigation.md)
- Fuse-block comparison this qualifies: `driver/.build/tools/FWSEC_COMPARISON.md`

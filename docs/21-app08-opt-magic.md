# 21 — app08 writes OPT_MAGIC: the advertise path in firmware

Offline result, from ROM/ucode analysis only. No card time.

## What referenced what

Searching all four ROMs and the extracted ucode for the advertise-path registers
(both PRI and falcon `0x14000000 | pri` encodings):

- **`0x8872c` — the publish trigger — appears in no ROM and no ucode.** Neither
  does `LnkCap2` (`0x880a4`). The VBIOS never writes the publish trigger; only
  our patch does. The `CAP2 = 0x2` a clean card boots with is not published by
  firmware.
- `OPT_MAGIC` (`0x820520`) is referenced by every ROM **and by `app08`**, twice.

`app08` is plaintext and disassembles cleanly from offset 0 — 254 unknown
instructions in 23155 lines (~1%), versus the Booter image which is encrypted and
decodes to near-total garbage. Instruction boundaries here are trustworthy.

> Method note: an earlier pass at this disassembled from arbitrary offsets and
> produced a plausible but wrong gate address. Falcon is variable-length; only
> whole-file decodes are safe. Same failure mode as the Booter gadget addresses
> in [doc 20](20-hs-execution-surface.md).

## Site 1 — conditional write, `0x3b0b`

```
mov $r9 0x14820148      ; PRI 0x820148
ld  b32 $r9 D[$r9]
and $r9 0x1             ; bit 0
bra e 0x3b23            ; skip if clear
mov $r15 0x200000
mov $r9 0x14820520      ; OPT_MAGIC
st  b32 D[$r9] $r15
```

`0x820148` reads `0x00000000` on **both** parts post-boot, so this branch is not
the differentiator in the state we can observe. It may be transient at devinit
time.

## Site 2 — unconditional write, `0xcdaf`

The PCIe link-setup routine:

```
ld  b32 $r9 D[$r15]     ; r9 = OPT_MAGIC
mov $r5 0x200000
or  $r9 $r5             ; OPT_MAGIC | 0x200000
st  b32 D[$r15] $r9     ; write back
ld  b32 $r15 D[$r14]    ; 0x88488
mov $r9 0xfdffffff      ; clear bit 25
st  b32 D[$r14] $r15
mov $r10 0x14088088     ; LnkCtlStatus
mov $r11 0x1408e000     ; XP3G base
and $r9 $r4             ; clear bit 31 of 0x8814c
mov $r1  0x14137c00     ; per-lane region
```

It touches `0x88488`, `0x8814c`, `0x88088`, the XP3G base `0x8e000`, `0x88150`,
`0x8b980`, `0x137c00` — the whole advertise/PHY cluster this investigation has
been circling.

## The reading

`0x00200000` is the bit **app08 sets itself**. So:

```
A100  OPT_MAGIC = 0x00200000              just app08's own bit
170HX OPT_MAGIC = 0x16680000 = 0x00200000 | 0x16480000
```

The extra `0x16480000` (bits 19, 22, 25, 26, 28) is present only on the crippled
part and does not come from app08. That is the fuse signature.

## The untested experiment

**`app08` writes `0x820520` directly**, so it is a writable register in falcon
context. This project has never tried writing `OPT_MAGIC` itself — only
`0x82057c`, `0x820378`, and `0x820c24`, all of which refused.

Overriding `XP3G_STATUS3` (which mirrors `OPT_MAGIC`) worked but moved nothing
([doc 19](19-a100-reference-diff.md)). The consumer may read `0x820520` itself
rather than the mirror.

```
CmpFeatAddr=0x820520  CmpFeatVal=0x00200000
```

One Booter load. If `OPT_MAGIC` takes the A100 value and `CAP2` follows, that is
the answer. If it takes and `CAP2` does not move, `OPT_MAGIC` is not the gate
either and the fuse signature lives somewhere it has not been found.

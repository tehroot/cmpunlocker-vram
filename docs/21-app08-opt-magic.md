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

## Result — refused, and the asymmetry is the finding

```
FEAT_WR begin addr=0x820520 val=0x00200000 cur=0x16680000
FEAT_WR 0x820520=0x00200000 attempt=0 status=0xffff rd=0x16680000
FEAT_WR 0x820520=0x00200000 attempt=1 status=0xffff rd=0x16680000
FEAT_WR FAILED
```

`OPT_MAGIC` refuses our write and `CAP2` stays `0x6`.

**But `app08` writes this exact register** — `st b32 D[$r15] $r9` at `0xcdc9`,
unconditionally, every boot. Our SEC2 Booter payload writes the same address and
is refused.

Both are privileged falcon-context writes. Only one lands. So **the OPT bank is
not gated purely by privilege level** — it is gated by which master issues the
write, or by a window that has closed by the time the Booter payload runs.

That reframes every OPT refusal recorded in [doc 19](19-a100-reference-diff.md):

- `EN_SW_OVERRIDE = 1` did not help because the override enable was never the
  variable.
- The fuse-block PLM being open (`0x8200fc = 0xffffffff`, set by `0001` on every
  boot) did not help for the same reason.
- `SENSE_CTRL` re-sensing did not help because the shadow was never refusing on
  privilege grounds.

The question was always "do we have enough privilege." The evidence now says
privilege was never what was being checked.

## What that implies

Writing the OPT bank appears to require being the master that firmware uses at
the time firmware uses it — i.e. running as devinit/`app08` during early init,
not as a driver-triggered Booter payload afterwards.

Two consequences:

1. The HS ROP harness ([doc 20](20-hs-execution-surface.md)) does not obviously
   help. It executes in the Booter's context, which is the context already being
   refused. More capability in the wrong context is still the wrong context.
2. The remaining route is modifying what `app08` itself does — a VBIOS change,
   which runs into image signing. Not investigated, and a different class of
   problem from everything attempted so far.

Stated plainly: this closes the register-write approach rather than advancing it.
The value is knowing *why* it was always going to fail, which was not established
before.

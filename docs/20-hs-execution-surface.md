# 20 — HS execution: we already have it

Reframing that came out of reading `0001-sec2-postbl-plm-ss-cfg.patch` rather than
running anything.

## The primitive is a ROP chain, not a hardware-limited single write

`_kgspSec2PostblTimingFillPayload()` builds a 62 KB DMEM image
(`SEC2_POSTBL_TIMING_SIGNATURE_SIZE = 0xf800`): uniform `FILL_DWORD` everywhere
— Pry's uniform-fill-V — then specific dwords near the top of DMEM:

```
0xf754 = writeValue        0xf75c = 0x00000cbd
0xf76c = writeAddr         0xf774 = 0x00001fbd
0xf788 = 0x000010aa        0xf78c = 0x0000815a
0xf790 = 0x00008e18        0xf798 = 0x0000815a
0xf7b8 = 0x0000582d        0xf7c8 = 0x00000cbd
0xf7f4 = 0x00000ccb        0xf7f8 = 0x00007f2f
0xc0deca7e as marker
```

Those 16-bit values are falcon code addresses on the HS stack. **The "one
arbitrary write per Booter load" ceiling that shaped every experiment in
[doc 19](19-a100-reference-diff.md) is the length of this chain, not a property
of the hardware.**

> **Correction.** An earlier version of this doc claimed the gadget addresses
> were verified by checking that 8 of 9 fall inside the 60160-byte image. That
> check is close to vacuous — any 16-bit value below `0xeb00` passes it — and the
> real code region is only ~`0x8800` bytes, so `0x8e18` actually lands in zero
> padding. The addresses are **not** verified.

**The Booter image is encrypted**, so static disassembly is not available:

```
0x0000-0x8000   entropy 7.93-7.96 bits/byte   AES-encrypted
0x8000-0x9000   entropy 4.38, 56% zeros       tail + padding
0x9000-0xeb00   100% zeros                    padding
dbg != prod, first difference at byte 256     signature region
```

Running `envydis -m falcon -V fuc5` at the candidate offsets produces incoherent
output with a high unknown-instruction rate, consistent with ciphertext. The
falcon decrypts HS ucode at load with a hardware key; that key is not reachable.

What does hold up is structural, from the payload itself:

```
f754 writeValue   f758 c0deca7e   f75c 0cbd
f76c writeAddr    f774 1fbd
f788 10aa  f78c 815a  f790 8e18  f794 c0deca7e  f798 815a
f7a0 c0deca7e  f7a4 1fbd  f7b0 ffbc  f7b8 582d
f7c4 c0deca7e  f7c8 0cbd  f7d8 00000003  f7e0 1fbd
f7f4 0ccb  f7f8 7f2f
```

`c0deca7e` occurs **four times** (`f758`, `f794`, `f7a0`, `f7c4`) — the canary
re-placed at successive frame boundaries, which is Pry's HS stack-canary defeat.
`(c0deca7e, 0cbd)` recurs at `f758/f75c` and `f7c4/f7c8`; `1fbd` recurs at
`f774`, `f7a4`, `f7e0`. The chain is multi-frame with a repeating unit, and the
repeat is visible without knowing what any gadget does.

## Two levers this exposes

**1. `dmem.bin` — arbitrary payload with no rebuild.**

```c
#define SEC2_POSTBL_TIMING_DMEM_PATH "/lib/firmware/nvidia/ga100/gsp/dmem.bin"
os_open_and_read_file(SEC2_POSTBL_TIMING_DMEM_PATH, pSignatureVa, sigSize);
```

If that file exists it replaces the built-in payload wholesale. Chain iteration
becomes writing a file, not rebuilding a kernel module — the iteration loop for
this work is already in place.

**2. The chain runs at L3.** It set `EN_SW_OVERRIDE` (`0x820040`), which the fuse
bank refuses from the host. Extending it to N writes turns every "refused"
result in doc 19 into a retest at a privilege level we have not actually
exhausted — we only ever spent one write at a time.

## Ucode extracted

`fwsec/booter_load_ga100_prod.bin`, 60160 bytes, raw-deflate out of
`g_bindata_kgspGetBinArchiveBooterLoadUcode_GA100.c` (`IMAGE_PROD`, 34139 bytes
compressed). `recon/extract-booter.py` reproduces it.

Gadget offsets to disassemble (`file = imem + 0x30`):

| imem | file |
|---|---|
| `0x0cbd` | `0x0ced` |
| `0x0ccb` | `0x0cfb` |
| `0x10aa` | `0x10da` |
| `0x1fbd` | `0x1fed` |
| `0x582d` | `0x585d` |
| `0x7f2f` | `0x7f5f` |
| `0x815a` | `0x818a` |
| `0x8e18` | `0x8e48` |

`envydis -m falcon -V fuc5 -i` on the rig; the local checkout has no envydis.

## Cheap test available immediately

`0x21000` reads `0xbadf1100` (priv violation) on both the 170HX and the A100 —
not `0xbadf5040` (decode error). The address decodes and is priv-blocked rather
than absent.

`0x21000` is the Turing/Maxwell fuse base (`tu102/dev_fuse.h`:
`NV_FUSE_OPT_NVDEC_DISABLE 0x00021378`); Ampere moved it to `0x820000`. If the
legacy alias survives with its own PLM, an L3 write there may take where
`0x82057c` refuses.

```
0x82057c  ->  0x2157c
0x820580  ->  0x21580
```

One `FEAT_WR`, verified by reading the Ampere-base register. No new code needed.

## Legacy alias — negative

`FEAT_WR 0x2157c = 0`, twice per pass, four passes (the failed Booter loads put
GSP into its retry loop):

```
FEAT_WR begin addr=0x02157c val=0x00000000 cur=0xbadf1100
FEAT_WR 0x02157c=0x00000000 attempt=0 status=0xffff rd=0xbadf1100
FEAT_WR FAILED
FEAT_DUMP 0x820570: 00000000 00000000 00000549 00000001   <- 0x82057c unchanged
FEAT_DUMP 0x820580: 00000001 00000001 00000000 00000000   <- 0x820580 unchanged
```

The Turing-era fuse base does not provide a lower-privilege route to the Ampere
fuse shadow. `cur=0xbadf1100` also confirms the host cannot read the alias, which
was expected and is why the Ampere-base dump was the verification.

Closed. The remaining work is the chain itself.

## Why dmem.bin never ran — and the fix

Control run: the file loaded (`loaded 63488 bytes from
/lib/firmware/nvidia/ga100/gsp/dmem.bin`) on a clean cold boot, generated with
`--addr 0x820040 --value 1`. Afterwards `0x820040` read `0x00000000`. The chain
did not fire.

The generator was not at fault. The harness was never connected:

- `dmem.bin` is read exactly once, in `_kgspCreateSignatureMemdesc()`.
- The PLM-opening loop in `0001` runs on **every** boot when
  `_kgspSec2PostblTimingEnabled()`, and calls
  `kgspSec2PostblTimingRefillPayload()` **nine times**.
- `refill()` calls `_kgspSec2PostblTimingFillPayload()`, which regenerates the
  **built-in** payload.

So the file's contents are overwritten nine times before any probe runs, and
every `Cmp*` path has the same problem — all 7 refill call sites discard it.
`dmem.bin` was effectively dead code: a load path with nothing downstream of it.

`RAW_BOOTER` (`CmpRawBooter=N`) supplies the missing piece. It re-reads the file
into the signature buffer and calls `kgspExecuteBooterLoad_HAL()` **without**
refill, so the bytes that execute are exactly the bytes on disk. It runs after
the PLM loop, restores WPR2 (`0x1fa824/28`) before each load as the working
sequence does, and echoes `0xf754` / `0xf76c` back from the mapped buffer so the
log proves which payload actually ran.

### Incidental corroboration

`0001`'s PLM table already opens `0x008200fc` ("OPT_PLM") to `0xffffffff`. The
fuse-block PLM is therefore open during every experiment in
[doc 19](19-a100-reference-diff.md), and `OPT_*` writes still refused. That is
independent support for "PLM is not what blocks the OPT bank", arrived at from
the driver source rather than from the `FEAT_PLM` readback whose scope was
uncertain.

### First use

The control is the same one that just failed, run properly:

```
CmpRawBooter=1, dmem.bin generated with --addr 0x820040 --value 1
```

`0x820040` going `0 -> 1` proves a file-supplied chain executes. Only after that
does varying the chain mean anything.

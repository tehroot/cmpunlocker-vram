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

Verification: with `IMEM = file offset − 0x30`, 8 of the 9 distinct values are
inside the 60160-byte Booter Load image. `0xffbc` is the sole exception, so it is
data (a DMEM pointer or constant) rather than a return address — which is what a
real chain should look like.

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

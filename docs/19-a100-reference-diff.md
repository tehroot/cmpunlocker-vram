# 19 — A100 reference diff: the crippling, located

Live BAR0 capture from an uncrippled GA100 (`A100-SXM4-80GB`, `10de:20b2`,
VBIOS `92.00.9E.00.02`, `boot0=0x170000a1`, training Gen4 x16) diffed against
the CMP 170HX (`10de:20c2`, same `boot0`). Both `--wide` captures via
`recon/ga100-bar0-dump.c`; 170HX taken after a cold power cycle so the AON
island is clean.

Tooling: `recon/dump-diff.sh`, `recon/volatile-offsets.txt`.

## Method note — the volatility mask

Two back-to-back captures of the *same* card differ at 39 offsets (PTIMER,
XVE counters, `0x118e00`–`0x118e3c`, parts of `0x132xxx` / `0x137xxx`). Those
are live status, not configuration. A second self-diff on the 170HX added
`0x118e40` and `0x137640`. The union is the mask; with it applied a same-card
diff is empty, so every surviving line in the real diff is genuine.

Without this the diff carries 41 false positives, several of them inside the
UPHY range this investigation had been staring at.

## Closed

**`0xcb00` / the `0x118f78` gate — dead.** All four witnesses are identical on
a Gen4 part and a Gen2 part:

| | A100 (Gen4) | 170HX (Gen2) |
|---|---|---|
| `0x118f78` | `0x00000000` | `0x00000000` |
| `0x118e90` | `0x00028400` | `0x00028400` |
| `0x118e80` | `0x00000400` | `0x00000400` |
| `0x11823c` | `0x028a2a2a` | `0x028a2a2a` |

The routine does not execute on a part that trains Gen4. It cannot be what
makes a working part work. This supersedes the open contradiction left in
[doc 18](18-pri-mapping-and-the-advertise-path.md).

**`0x820584` — dead.** `0x00000001` on both. Drop from the fuse candidate list.

**`EN_SW_OVERRIDE` (`0x820040`) — not part of the crippling.** `0x00000000` on
both, including the Gen4 part. It is not a differentiator; it is only the lock
that would have to be picked to take the fuse route.

## The crippling fuses — confirmed

```
0x82057c  OPT_GEN23   A100 = 0x00000000    170HX = 0x00000001
0x820580  OPT_GEN3    A100 = 0x00000000    170HX = 0x00000001
```

Both were nominated as candidates in [doc 08](08-vbios-mac-fuse-map-external.md)
from an external Mac fuse map, and targeted speculatively by the `FUSE_OVR`
block in `0007-pcie-gen2.patch`. They are now confirmed on live silicon: clear
on a Gen4 part, set on the crippled one.

That inverts the prior reading of `FUSE_OVR`. The block was written off because
the writes did not land — with the *target itself* unverified. The target is now
verified. The lock is the open question, not the address.

Fuse-block mapping sanity check, recovered blind from the diff:

```
0x820378  NV_FUSE_OPT_NVDEC_DISABLE   A100 = 0x00000000   170HX = 0x0000001f
```

All five NVDEC engines fused off on the 170HX — a known-true fact about the
part, matching the one named offset the public `ga100/dev_fuse.h` still carries.
The fuse block is being read correctly.

48 fuses differ directionally in total, so the diff alone does not single these
two out; what does is that they were named before the data existed.

## The downstream enforcement — and why it matters more

No ROM references `0x82057c` / `0x820580` (searched all four images for both the
PRI and falcon `0x14000000 | pri` encodings; only the device-ID fuses appear, in
`283106.rom` at `0x34ec8` / `0x94ec8`). Consistent with the fuse being consumed
by hardware straps rather than ucode.

The straps land in XP registers **that are already known-writable** — `0007`
writes `0x8c2c0` and `0x8c040` today in the Gen2 path:

| offset | A100 | 170HX | delta |
|---|---|---|---|
| `0x8c040` | `0x80004c00` | `0x80084c00` | bit19 |
| `0x8c2c0` | `0x060711b2` | `0x068731b3` | bits 0, 13, 23 |
| `0x8c1c0` | `0x00040036` | `0x00220036` | bit18 clear→set 17,21 |
| `0x8c080` | `0x00001010` | `0x00000404` | |
| `0x8c140` | `0xffff00ff` | `0x00001818` | |
| `0x8c498` | `0x000f0040` | `0x00000000` | **zeroed on 170HX** |
| `0x8c49c` | `0x0040a855` | `0x00000000` | **zeroed on 170HX** |
| `0x8c4a0` | `0x0053c42f` | `0x0053c000` | low 11 bits |
| `0x8c4f0` | `0x00000669` | `0x00000449` | bits 5, 9 |

`0x068731b3 & ~0x00802001 == 0x060711b2` exactly. The XP delta is a small,
enumerable set — not a wall.

`0x8c498` / `0x8c49c` being fully zero on the 170HX and populated on the A100
reads as an equalization/preset table that is never filled on the crippled part.
Note this is *not* the `0xcb00` routine, which is closed above — some other
initialization populates them.

Advertisement layer, for reference:

| offset | A100 | 170HX | |
|---|---|---|---|
| `0x88084` LnkCap | `0x00457104` | `0x00456102` | max speed 4 → 2 |
| `0x880a4` LnkCap2 | `0x0180001e` | `0x00000006` | vector `0x0f` → `0x03` |
| `0x880a8` LnkCtl2 | `0x001e0004` | `0x00010002` | |
| `0x8809c` | `0x00070013` | `0x00070813` | bit11 |
| `0x880a0` | `0x00000006` | `0x00001400` | |
| `0x880b4` | `0x0114c809` | `0x01140009` | |
| `0x88114` | `0x800000ff` | `0x80000001` | |

(170HX `LnkCap2 = 0x6` is with the Gen2 patch active; the clean value is `0x2`.)

## The experiment this defines

Prior Gen3 attempts wrote `CAP2` and hoped. The new information is that the
enforcement bits downstream of the fuse are now *located*, and they are in
registers we can already write without the SEC2 primitive.

Ordering matters and has never been tried this way:

1. In the `GEN_EARLY` window, clear the XP enforcement bits to their A100
   values — `0x8c040` bit19, `0x8c2c0` bits 0/13/23.
2. Populate `0x8c498` / `0x8c49c` with the A100 values.
3. *Then* write `LnkCap2 = 0x0180001e` / `LnkCap` speed 4.
4. Publish via `0x8872c`, then retrain.

If the fuse gates what `CAP2` is permitted to hold, writes to bit3 are dropped
while the strap is set — which is exactly the failure mode every previous Gen3
attempt hit. Clearing the strap first is the untested step.

If that fails, the fuse route returns as primary, and the question narrows to
one thing: is `0x820040` a PLM-gated register or a hard fuse — the Q1 of
[doc 07](07-fuse-override-and-static-recon.md), still unanswered.

## Artifacts

- `a100-wide.txt`, `a100-wide-ver1.txt` — reference captures
- `170hx-wide.txt`, `170hx-wide-2.txt` — target captures
- `roms/a100-sxm4-80gb-G506.0212.00.01.rom` — A100 SXM4 VBIOS read from the
  BAR0 PROM aperture (`NVGI`), the only ROM matched to a card we also have live
  registers for
- `recon/volatile-offsets.txt` — the mask

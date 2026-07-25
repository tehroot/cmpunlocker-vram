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

> **Contamination warning.** `170hx-wide.txt` was captured with the Gen2 patch
> active. `0007` writes `0x8c040` (bits[19:18] = rate), `0x8c1c0`
> (`PL_LINK_RATE_VALUE`), `0x8c2c0` bit2, `0x880a8`, and publishes via
> `0x8872c`. Deltas at those offsets are **our own writes**, not the crippling —
> `0x8c040` bit19 in particular is the Gen2 rate we set, and reads 0 on the A100
> only because the A100 was never patched. A clean baseline requires a 170HX
> capture under the **stock** driver.

| offset | A100 | 170HX | delta | ours? |
|---|---|---|---|---|
| `0x8c040` | `0x80004c00` | `0x80084c00` | bit19 | **yes** |
| `0x8c1c0` | `0x00040036` | `0x00220036` | bits 17,18,21 | **yes** |
| `0x8c2c0` | `0x060711b2` | `0x068731b3` | bits 0, 13, 23 | bit2 only |
| `0x8c080` | `0x00001010` | `0x00000404` | | no |
| `0x8c140` | `0xffff00ff` | `0x00001818` | | no |
| `0x8c498` | `0x000f0040` | `0x00000000` | **zeroed on 170HX** | no |
| `0x8c49c` | `0x0040a855` | `0x00000000` | **zeroed on 170HX** | no |
| `0x8c4a0` | `0x0053c42f` | `0x0053c000` | low 11 bits | no |
| `0x8c4f0` | `0x00000669` | `0x00000449` | bits 5, 9 | no |

The uncontaminated set is `0x8c080`, `0x8c140`, `0x8c498`, `0x8c49c`, `0x8c4a0`,
`0x8c4f0`, and `0x8c2c0` bits 0/13/23. `0x068731b3 & ~0x00802001 == 0x060711b2`
exactly, and bit2 (the one `0007` clears) reads 0 on both, so those three bits
are genuine.

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

## On-card result — the straps are not the enforcement point

`CmpGen3Strap=0x7f`, `CmpGen3Early=1`, `CmpXveCmd=0xe`:

| bit | offset | pre | want | post | |
|---|---|---|---|---|---|
| 0 | `0x8c498` | `0x00000000` | `0x000f0040` | `0x00000000` | REVERTED |
| 1 | `0x8c49c` | `0x00000000` | `0x0040a855` | `0x00000000` | REVERTED |
| 2 | `0x8c4a0` | `0x0053c000` | `0x0053c42f` | `0x0053c000` | REVERTED |
| 3 | `0x8c4f0` | `0x00000449` | `0x00000669` | `0x00000449` | REVERTED |
| 4 | `0x8c2c0` | `0x068731b3` | `0x060711b2` | `0x060711b2` | **STUCK** |
| 5 | `0x8c080` | `0x00000404` | `0x00001010` | `0x00000404` | REVERTED |
| 6 | `0x8c140` | `0x00001818` | `0xffff00ff` | `0x00001818` | REVERTED |

Six of seven are read-only from the host — writes dropped with no effect. The
one that took (`0x8c2c0`, landing exactly on the A100 value) moved neither
`CAP` nor `CAP2`. The XP straps are not where gen is enforced, and
`0x8c498`/`0x8c49c` are not host-populatable.

**The publish path is the enforcement point.** The final line of the run:

```
after XVE  want=0x0000000e  XVE=0x0000000e  CAP=0x00456102  CAP2=0x00000006
```

`0x8872c` accepts and reads back `0xe`, but `LnkCap2` stays `0x6`. Bit3
(8.0 GT/s) is dropped between the trigger and the published vector. The doc 12
model `LnkCap2 = 0x2 | value` is incomplete; it is

```
LnkCap2 = (0x2 | value) & permitted
```

with bit3 clear in `permitted`.

**Confound, not yet excluded:** that run was a warm reload with `LnkCap2`
already `0x6`. "Bit3 is masked" and "the trigger fires once per reset" both fit
the observation. `GEN3_PUBSWEEP` (`CmpPubSweep=1`) separates them by sweeping
`0x8872c` across 11 values in a single boot and logging `LnkCap2` after each.

- `LnkCap2` tracks `0x2|v` except where bit3 is involved → trigger is live,
  bit3 masked, and the mask is the target.
- `LnkCap2` never moves after the first write → trigger is one-shot per reset,
  and the sweep must be redone cold, one value per boot.

## Clean 170HX baseline

From the `early=0` run, pre-write, resolving the contamination warning above:

```
CAP     = 0x00456101      (max speed 1)
CAP2    = 0x00000002      (2.5 GT/s only)
0x8c2c0 = 0x068731b3      bits 0/13/23 vs A100 are genuine
0x8c040 = 0x80004c00      bit19 confirmed as this patch's own Gen2 write
```

## PUBSWEEP result — the advertise layer is closed

Cold boot, `CmpPubSweep=1`, `CmpGen3Early=1`, `CmpXveCmd=0xe`. Entry state clean
(`CAP=0x00456101`, `CAP2=0x2`, `0x8872c=0`, `CYA0=0x068731b3`).

| v | rb | CAP | CAP2 | dropped |
|---|---|---|---|---|
| `0x00` | `0x00` | `…101` | `0x2` | `0x0` |
| `0x02` | `0x02` | `…102` | `0x6` | `0x0` |
| `0x04` | `0x04` | `…101` | `0x2` | `0x4` |
| `0x06` | `0x06` | `…102` | `0x6` | `0x0` |
| `0x08` | `0x08` | `…101` | `0x2` | `0x8` |
| `0x0a` | `0x0a` | `…102` | `0x6` | `0x8` |
| `0x0c` | `0x0c` | `…101` | `0x2` | `0xc` |
| `0x0e` | `0x0e` | `…102` | `0x6` | `0x8` |
| `0x0f` | `0x0f` | `…101` | `0x2` | `0xd` |
| `0x1e` | `0x0e` | `…102` | `0x6` | `0x18` |
| `0x3e` | `0x0e` | `…102` | `0x6` | `0x38` |

Three results:

1. **The trigger is live and re-triggerable.** `CAP2` toggles `0x2 ↔ 0x6` eleven
   times in one boot, `CAP` tracking `…01 ↔ …02`. The one-shot-per-reset
   hypothesis is excluded, so the warm-reload observation stands.
2. **`0x8872c` is a 4-bit field.** `0x1e` and `0x3e` both read back `0x0e`.
3. **Bit3 is never granted, for any input.** Every value requesting 8.0 GT/s
   reports `dropped` containing `0x8`. The ceiling is `0x6`.

Behaviour is `CAP2 = 0x6` iff `v & 0x2`, except `v=0x0f → 0x2`, so bit0 is a
qualifier of some kind (`v=0x3` untested).

The permitted mask is fixed upstream of `0x8872c` and is not reachable from the
advertise layer. Combined with the XP straps being host-RO, **every register-level
route to Gen3 on this card is now closed.** What remains is the fuse itself,
which is consistent: `OPT_GEN23=1` / `OPT_GEN3=1` clamping the grantable vector
is exactly this behaviour.

Also settled: `CYA0` read `0x068731b3` at entry, the clean value, so the earlier
apparent persistence of the `GEN3_STRAP` write was a warm reboot, not a
persistent domain. No such finding.

## Next — doc 07 Q1, now the only open route

`FUSE_OVR` (`CmpFuseOvr` / `CmpFuseTest` / `CmpFuseGen`) already implements the
chain and the control. Staged, because the block's own comment records that
several failed Booter loads in one run pushed GSP bootstrap into a retry loop:

| run | keys | question |
|---|---|---|
| A | `CmpFuseOvr=1` | does `0x820040` become `1`? — is `EN_SW_OVERRIDE` a register |
| B | `+ CmpFuseTest=1` | does an architecturally-RW OPT fuse take, with the gate open |
| C | `+ CmpFuseGen=1` | do `OPT_GEN23` / `OPT_GEN3` clear |

A failing is terminal for the fuse route. B taking while C rejects means the OPT
path is open and the gen fuses are specifically protected — a different problem
from a global lock.

## FUSE_OVR runs A and B

**Run A — `EN_SW_OVERRIDE` is a PLM-gated register.** Doc 07's Q1, answered.

```
begin  EN=0x00000000
write  0x820040 = 1   status=0xffff   rd=0x00000001
end    EN=0x00000001
```

It persists across a module reload — a later run reports `begin EN=0x00000001`.
This reverses [doc 08](08-vbios-mac-fuse-map-external.md)'s external claim that
`EN_SW_OVERRIDE` is inert on the 170HX.

`status=0xffff` while the write lands is the established pattern for this
primitive: the payload write happens regardless of Booter completion. Read `rd=`,
not `status=`.

Setting `EN` alone changes nothing else — `GEN3`/`GEN23` stay `1`, `CAP2` stays
`0x2`. It is the gate, not the value.

**Run B — OPT writes are still refused with the gate open.**

```
CTRL_OPT_NVDEC_DIS(0x820378)=0x0000001e  attempt=0  rd=0x0000001f
CTRL_OPT_NVDEC_DIS(0x820378)=0x0000001e  attempt=1  rd=0x0000001f
FAILED to set CTRL_OPT_NVDEC_DIS
```

`0x820040` takes and `0x820378` does not, through the same SEC2 primitive at the
same priv level. So this is not a global lock on the fuse block — it is
per-register, and `EN_SW_OVERRIDE=1` is necessary but not sufficient.

Run C (`CmpFuseGen=1`) would fail for the same reason; not run.

### Hypothesis: OPT readout vs a separate control bank

On several NVIDIA fuse blocks the override is a distinct write register
(`NV_FUSE_CTRL_OPT_*`) from the resolved read-only value (`NV_FUSE_OPT_*`). The
`FUSE_OVR` entry is *named* `CTRL_OPT_NVDEC_DIS` but writes `0x820378`, which is
`NV_FUSE_OPT_NVDEC_DISABLE` — the readout. Writing a readout would no-op exactly
as observed.

The public `ga100/dev_fuse.h` is stripped of every `CTRL_OPT` name, so the
candidate was found by value instead. `0x820c00`–`0x820c54` is a compact block
that differs between the parts and contains a `0x1f` mirroring `NVDEC_DISABLE`:

| offset | A100 | 170HX |
|---|---|---|
| `0x820c0c` | `0x00000000` | `0x00000001` |
| `0x820c1c` | `0x00000040` | `0x00000013` |
| `0x820c24` | `0x00000000` | `0x0000001f` |
| `0x820c38` | `0x00000000` | `0x000000ff` |
| `0x820c3c` | `0x00000001` | `0x000000ff` |
| `0x820c40` | `0x00000000` | `0x00000001` |
| `0x820c48` | `0x00000000` | `0x000000ff` |
| `0x820c4c` | `0x00000000` | `0x00000001` |
| `0x820c50` | `0x000000ff` | `0x00000001` |
| `0x820c54` | `0x00000000` | `0x00000001` |

Caveat: `0x820c50` is *inverted* (A100 high, 170HX low), which reads like a count
rather than a disable mask, so this may be `STATUS_OPT` floorsweeping summary
rather than a writable control bank. The `0x21000` fuse-ctrl region is
byte-identical across both parts, so the control is not there.

Settled on-card rather than by inference: write `0x820c24 = 0x1e` via `FEAT_WR`
and dump `0x820200 +0x400` via `FEAT_DUMP` in the same pass. If `0x820378`
follows, the control bank is found and the gen mirrors are next. If `0x820c24`
takes but `0x820378` does not, the block is independent state. If `0x820c24` is
refused, the fuse block is locked to this primitive entirely.

## The fuse surfaces, enumerated

`FEAT_WR 0x820c24 = 0x1e` → `rd=0x1f`, refused, both attempts. The dump header
also reports `FEAT_PLM=0xffffffff FEAT2=0xffffffff` — **the priv-level mask is
wide open, so PLM is not what blocks the write.** A `PLM_SWEEP` comparing
`0x820378` against `0x820040` would have shown nothing; dropped.

The `0x820c` hypothesis is settled by a header rather than inference:

```
NV_FUSE_STATUS_OPT_DISPLAY   0x00820C04   /* R-I4R */
```

`0x820Cxx` is the `STATUS_OPT` resolved-floorsweeping bank, architecturally
read-only. The inversion at `0x820c50` was the correct tell.

| surface | status |
|---|---|
| `OPT_*` `0x8201xx`–`0x8207xx` | readouts; refuse writes with PLM open |
| `STATUS_OPT_*` `0x820Cxx` | RO by architecture (header-confirmed) |
| `CTRL_OPT_*` | absent from the `0x820800`–`0x820bff` gap |
| `EN_SW_OVERRIDE` `0x820040` | **writable**, persists across module reload |
| `0x21000` | priv-blocked `0xbadf1100` on both cards |
| fuse macro `0x820000`–`0x820010` | untested |

`0x820800`–`0x820bff` holds no mirrored values; `0x820b20`–`0x820b6c` is
high-entropy per-die key material (differs between any two chips, not a
crippling signal). `0x820624` reads `0x1f` on both parts, so it is not an
`NVDEC_DISABLE` mirror either.

### The macro block

`0x820000`–`0x820010` matches the standard NVIDIA fuse-macro register file, and
is identical on both parts as control (not values) should be:

```
0x820000 = 0xe0040000   FUSECTRL   (CMD / STATE)
0x820004 = 0x000001fb   FUSEADDR
0x820008 = 0xa0802007   FUSERDATA
0x82000c = 0x00000000   FUSEWDATA
0x820010 = 0x00020607   FUSETIME
```

Next test: `FEAT_WR 0x820004` (`FUSEADDR`, a benign address register — writing it
disturbs no state). If it takes, the macro accepts writes and a sense/reload with
`EN_SW_OVERRIDE=1` becomes the route to making `OPT_*` re-resolve. If it refuses,
the whole fuse block except `0x820040` is closed to this primitive.

`FUSECTRL` (`0x820000`) is deliberately not written first — it carries the
command field and could start a sense cycle mid-boot.

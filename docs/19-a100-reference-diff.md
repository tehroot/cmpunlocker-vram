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

## The fuse macro accepts writes

`FEAT_WR 0x820004 = 0x1fc` → `rd=0x000001fc`. Confirmed in the same dump:

```
0x820000  e0040000   FUSECTRL   STATE=4 (idle), CMD=0
0x820004  000001fc   FUSEADDR   <-- our write
0x820008  a0802007   FUSERDATA
0x82000c  00000000   FUSEWDATA
0x820040  00000001   EN_SW_OVERRIDE (persisting)
```

`FUSECTRL` did not move on its own, so nothing auto-triggered.

Using the macro needs two writes in sequence (`FUSEADDR` then `FUSECTRL`), which
`FEAT_WR` cannot express — it is one address/value pair per invocation. Hence
`FUSE_MACRO` (`CmpFuseRd` / `CmpFuseRow` / `CmpFuseCnt`).

It issues **READ commands only**. It never writes `FUSEWDATA` and never issues
`CMD=WRITE`: burning a fuse is irreversible, needs programming voltage, and is
not the objective. The objective is to map which row and bit carry `OPT_GEN23` /
`OPT_GEN3`.

It first probes whether the macro takes a plain `GPU_REG_WR32`. If it does, rows
sweep in a loop rather than costing a Booter load per write, which is what makes
a full array dump practical.

Interpreting `hostwr`:

- `hostwr=1` → the macro is host-writable; the row sweep runs and dumps
  `FUSERDATA` per row.
- `hostwr=0` → the macro needs the SEC2 primitive for every write, and a sweep
  costs two Booter loads per row. Still possible, just slow.

Note the standing caveat on the earlier PLM claim: `FEAT_PLM=0xffffffff` was read
from the FEAT region's own mask registers, not from a mask covering `0x820378`.
"PLM is not the blocker" is therefore weaker than stated above — it holds for the
region the header reported, not necessarily for the OPT bank.

## FUSE_MACRO first run — the array reads, and it costs a power cycle

`CmpFuseRd=1 CmpFuseRow=0 CmpFuseCnt=16`:

```
FUSE_MACRO begin hostwr=1 CTRL=0xe0040000 ADDR=0x000001fc RDATA=0xa0802007 EN=0x00000001
row 0/1  0x53557c3d      row 8/9  0xc048ce89
row 2/3  0x23de954c      row a/b  0x00000168
row 4/5  0xf86a33e9      row c/d  0x02010200
row 6/7  0x86500003      row e/f  0x06090680
```

- **`hostwr=1`** — the macro takes plain `GPU_REG_WR32`, no Booter load per write.
- **`FUSEADDR` bit0 is ignored** — `addr` and `addr+1` return the same word, so
  the effective row is `addr >> 1`. Sweep steps by 2.
- Reads complete immediately: `spin=2`, `CTRL` back at `0xe0040000` (idle, CMD=0).
- `RDATA` varies per row pair, so these are real array reads, not a stale latch.

**Cost:** the sweep completes and prints `end`, then GSP bootstrap fails and the
card drops off the bus — every subsequent register read is `0xffffffff` and GSP
retries in a loop. Recovery is a cold power cycle. The failure is after the reads,
not during them, so one large sweep costs the same single power cycle as a small
one. Sweep the whole array in one run.

`FUSEADDR` powered up at `0x1fb`, so the array is roughly `0x200` addresses =
~256 distinct rows.

Next: `CmpFuseCnt=2048` from row 0 to dump the array, then correlate rows against
known `OPT_*` values to locate the bits behind `0x82057c` / `0x820580`.

## The fuse array, mapped

`CmpFuseCnt=1024` from row 0. The card did **not** wedge this time, so the
earlier drop-off was not a deterministic consequence of the sweep.

```
effective rows: 504 captured
bank pairs: 252   differing: 0   (perfect mirror)
0x20c2: 2 hits — (row 149, bit 17) and (row 405, bit 17)
```

- `FUSEADDR` ignores **bit 0 and bit 8**. Effective row is `addr >> 1`, and rows
  `0..255` mirror `256..511`. The real array is **256 rows × 32 bits = 8192 bits**.
- The device-ID field `0x20c2` is at **row 149, bit 17**, which anchors the
  array's bit numbering. `0x20b2` / `0x20f2` are absent, as they should be.
- **The banks are a perfect mirror — no repair or override bank is in play.**
  That was the one structural feature that could have offered a writable path
  into the array, and it isn't there.

Tooling: `recon/fuse-analyze.py`.

## Where the Gen3 question actually stands

Closed by evidence, not assumption:

| route | status |
|---|---|
| `0xcb00` / `0x118f78` gate | dead — identical on a Gen4 part |
| XP straps (`0x8c0xx`–`0x8c4xx`) | host-RO; the one writable reg moves nothing |
| publish path `0x8872c` | bit3 never granted, any of 11 inputs |
| `OPT_*` fuse readouts | refuse writes |
| `STATUS_OPT_*` `0x820Cxx` | RO by architecture |
| `CTRL_OPT_*` bank | not present |
| fuse array repair/override bank | not present (perfect mirror) |
| `EN_SW_OVERRIDE` `0x820040` | **writable and persistent** — but insufficient alone |
| fuse macro `FUSECTRL`/`FUSEADDR` | **writable**; READ works |

Two mechanisms remain untried:

1. **`FUSECTRL` `CMD=SENSE_CTRL` with `EN_SW_OVERRIDE=1`.** The only mechanism
   that could make `OPT_*` re-resolve rather than stay latched from power-on
   sense. Cheap, and the macro is already proven to accept commands. Risk is a
   hang recoverable by cold cycle.
2. **HS code execution** (Pry's route). Everything above uses a single arbitrary
   write per Booter load. Arbitrary code at L3 would reach the priv-blocked
   `0x21000` region, which is the one address space we cannot touch at all.

Burning fuses is not on the table: OTP, irreversible, needs programming voltage.
Locating the gen bits in the array would be diagnostic only — and with no
override bank, there is no mechanism to act on the location. That is why the
array mapping stops here rather than continuing to hunt single bits.

## FUSE_SENSE — negative, and the register-level surface is exhausted

```
begin  cmd=3 CTRL=0xe0040000 EN=0x1 GEN23=0x1 GEN3=0x1 CAP2=0x6
after  CTRL=0xe0040000 spin=0 EN=0x1 GEN23=0x1 GEN3=0x1 CAP2=0x6
optwr  GEN23=0x00000001 refused  GEN3=0x00000001 refused  CAP2=0x00000006
```

`spin=0` does not mean the command was dropped: `CMD=1` (READ) demonstrably
executes for us — `RDATA` tracked the row address across the whole array sweep.
The macro accepts and runs our commands. `SENSE_CTRL` just does not re-resolve
the OPT shadow, and the shadow does not become writable afterwards.

The hypothesis was that `OPT_*` is RO because it latched at power-on sense while
`EN_SW_OVERRIDE` was still 0. That is now disconfirmed: sensing again with `EN=1`
changes nothing.

### Status

Every register-level route to Gen3 on this part has been tested and closed:

| route | result |
|---|---|
| `0xcb00` / `0x118f78` gate | identical on a Gen4 A100 — never was the mechanism |
| XP straps `0x8c0xx`–`0x8c4xx` | 6 of 7 host-RO; the writable one moves nothing |
| publish path `0x8872c` | bit3 never granted across 11 inputs; ceiling is `0x6` |
| `OPT_*` readouts | refuse writes, with and without `EN_SW_OVERRIDE` |
| `STATUS_OPT_*` | RO by architecture |
| `CTRL_OPT_*` bank | does not exist on this chip |
| fuse array | 256 rows, perfect mirror, no repair/override bank |
| `FUSECTRL` `SENSE_CTRL` | executes; does not re-resolve `OPT_*` |

What was gained and is durable: `EN_SW_OVERRIDE` is a writable, persistent
register (doc 07 Q1, answered); the fuse macro is host-writable and its READ path
works; the array is mapped with a known bit-numbering anchor; and the crippling
is pinned to two named fuse bits confirmed against live Gen4 silicon.

### What is actually left

1. **HS code execution** — [Pry's route](pry_pdf.pdf). Every result in this
   document came from *one arbitrary write per Booter load*. Arbitrary code at L3
   removes that constraint and reaches `0x21000`, the priv-blocked region
   (`0xbadf1100` on both parts) that is the only address space never touched.
2. **VBIOS cross-flash** — a different SKU's signed image. Not examined in this
   effort; blocked by signature verification, but it is a distinct surface rather
   than a variation on the ones closed above.

Burning fuses remains off the table: OTP, irreversible, requires programming
voltage.

## Methodology correction — posted writes made REVERTED unreliable

Two consecutive `XVE_PERMIT` runs with the same mask:

```
run 1  b0 0x88c28  pre=0x00000000  post=0x00000000  REVERTED
run 2  b0 0x88c28  pre=0x0000000f  post=0x0000000f  STUCK
```

`0x88c28` reads `0x0f` at entry to run 2. Nothing else writes that register, so
**the run-1 write landed and the immediate readback returned the stale value.**
These are posted writes; a readback in the next instruction is not a valid test.

This invalidates the verdict method used throughout, because **every `REVERTED`
result in this document came from an immediate readback**. Specifically at risk:

- the six XP straps in the `GEN3_STRAP` table, recorded above as host-RO
- `0x8c498` / `0x8c49c`, whose "writes dropped entirely" reading fed the
  conclusion that the straps are fuse-held

The `FEAT_WR` / `FUSE_OVR` refusals are less suspect: those readbacks happen
after a Booter load, so far more time elapses. The `OPT_*` results are probably
sound. The XP strap results are not.

`GEN3_STRAP` and `XVE_PERMIT` now read back twice — once immediately, then again
after flushing 32 reads through `PMC_BOOT_0` — and the verdict uses the second
(`post2`). The first (`post`) is still logged so the delay is visible.

**The XP strap table above should be re-run before it is relied on.** What does
not change: forcing those registers, whether or not the writes landed, never
moved `CAP` or `CAP2`. The "straps are not the enforcement point" conclusion
rests on that, not on the `REVERTED` verdicts.

## XP3G — the override file, and OPT_MAGIC

A full (unfiltered) `--base-only` log surfaced a register file the greps had been
hiding, and a window neither dump covers.

```
XP3G_STATUS  0x8e100 + 4i      st0=0x00000000  st3=0x16680000
XP3G_OVR     0x8e110 + 4i      ovr0=0x00000001 ovr3=0x00000004
XP3G_VAL     0x8e120 + 4i      val0=0x00000000 val3=0x00200000
XP3G_PLM     0x8e1b0..0x8e1bc  0xffffffff -- already open, writes land
```

`XP3G_STATUS3` (`0x8e10c`) reads `0x16680000`, which is **exactly** the fuse at
`0x820520` that `0007` calls `OPT_MAGIC`. The status word mirrors the fuse, and
`OVR`/`VAL` are the mechanism for overriding it.

And that fuse is one of the biggest differences between the parts:

```
0x820520  OPT_MAGIC   A100 = 0x00200000   170HX = 0x16680000
                      XOR  = 0x16480000   bits 19, 22, 25, 26, 28
```

`0007` **already sets `VAL3 = 0x00200000`** — the A100 value. But `OVR3 =
0x00000004`, so only bit 2 is overridden, and none of bits 19/22/25/26/28 are
covered. Right register, right value, wrong mask.

`XP3G_OVR` (`CmpXp3g` / `CmpXp3gSlot` / `CmpXp3gOvr` / `CmpXp3gVal`) writes
`VAL` then `OVR` — that order, so there is no window where a live mask sits over
stale data — re-reads `STATUS` after a flush, and reports `CAP`/`CAP2`. Defaults
are slot 3, `OVR=0xffffffff`, `VAL=0x00200000`.

**`0x8e000` is in neither capture**, so XP3G was never diffed against the
reference. The window is now in `ga100-bar0-dump.c`'s `--wide` list for any
future reference capture.

Caveat worth stating up front: `STATUS` mirroring the fuse does not prove
anything downstream consumes `STATUS` for the link-speed decision. If `STATUS`
moves and `CAP2` does not, the override works and simply is not wired to gen.

### XVE window exhausted

`CmpXvePermit=0x3fc`, with the flushed re-read in place so verdicts are real:

| bit | reg | result |
|---|---|---|
| b2 | `0x88d48` | `0 -> 0x03` STUCK |
| b3 | `0x88ce0` | `0x02100002 -> 0x02100006` STUCK |
| b4 | `0x88ce4` | `0x3f -> 0x14` STUCK |
| b5 | `0x88d04` | already the A100 value |
| b6 | `0x88dcc` | `0x80000000 -> 0x8000000b` STUCK |
| b7 | `0x88c88` | want `0x00078004`, got `0x00078002` — **partial** |
| b8 | `0x8890c` x8 | REVERTED (genuine) |
| b9 | `0x88c3c` x4 | REVERTED (genuine) |

`CAP2 = 0x6` throughout. Five registers took their A100 values and the advertise
did not move, so none of them is the permitted mask.

`0x88c88` is informative beyond this hunt: bits 17-18 accepted, bit 2 refused, in
a single register. **The fuse holds down individual bits, not whole registers** —
which is why "is this register writable" was always the wrong question.

Bits 10-15 (`0xfc00`) wedged the link and were not retried; they are the weakest
candidates in the set and cluster near live link-control state.

Earlier, bits 0-1: `0x88c28` REVERTED-then-STUCK (the posted-write artifact) and
`0x88cd8` STUCK, both holding `0x0f` persistently, neither moving `CAP2`.

### XP3G override works — and is not the gate

`CmpXp3g=1` (slot 3, `OVR=0xffffffff`, `VAL=0x00200000`):

```
XP3G_OVR begin slot=3 ovr=0xffffffff val=0x00200000 ST=0x16680000
         OPT_MAGIC=0x16680000 PLM=0xffffffff CAP2=0x00000006
XP3G_OVR after OVR=0xffffffff VAL=0x00200000 ST=0x16680000->0x00200000
         STATUS MOVED  CAP2=0x00000006
GEN_EARLY after XVE want=0x0000000e XVE=0x0000000e CAP2=0x00000006
```

**The override works.** `STATUS3` moved off the fuse value to exactly the A100's
`0x00200000`, and held. `CAP2` did not follow, and a subsequent publish with
`0xe` still produced `0x6`.

So `XP3G_STATUS3` mirrors `OPT_MAGIC` and is fully overridable, but nothing
downstream of it decides the advertised link speed. The caveat stated when this
probe was written is the outcome: mirroring the fuse proved the fuse feeds it,
not that anything reads it for gen.

What is gained is a genuine capability rather than another refusal: **this is the
first mechanism found that defeats a fuse-derived value.** Every other surface
either refused writes or held specific bits down. If a fuse-mirrored register is
ever identified as gating something we want, XP3G is how it gets overridden.

### Limit reached with the data in hand

Slots 0-2 remain untried, but their target values are unknown: `0x8e000` is in
**neither** reference capture, so there is no A100 value to aim at. Slot 3 was
only actionable because `STATUS3` happened to equal a fuse we *did* capture.

That is the honest boundary of this line of work. Continuing it means a wider
BAR0 capture from a reference part — `0x8e000` is now in the dumper's `--wide`
list — rather than more guessing here.

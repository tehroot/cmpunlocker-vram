# 24 — Red team of doc 23

Every claim in [doc 23](23-gen3-proposal-and-red-team.md) checked against the
captured artifacts and the published headers in the driver tree. Verdicts first.

| # | doc 23 claim | verdict |
|---|---|---|
| D1 | SMBPBI mailbox at `0x660e0`, "untested" | **disproved** — wrong block on a GPU; and it has run on every boot |
| D2 | write `0x823824 = 0` (rec. #2) | **disproved** — `0x00000001` on the A100 too |
| D3 | `0x823800` block deltas are crippling | **disproved for 4 of 11** — they are our own writes |
| D4 | `0x823814` is an override target | **disproved** — published `R--4R` readout |
| W1 | "OPT bank is master-gated" | **contradicted** by the PLM readback |
| W2 | — (model doc 23 never considers) | sense-chain re-drive fits every observation |
| W3 | "app08's write lands" | **unverified**, and the whole of W1 rests on it |
| W4 | `LnkCap2` derived from `OPT_GEN23`/`OPT_GEN3` | behaviour solid, **provenance unobserved** |
| W5 | PHY calibration "unpopulatable" | data says *never measured*, not *held at zero* |
| W6 | redirect Booter → app08 context | probably a **wrong-engine** problem, not a gadget problem |
| C1 | Gen3+ tables host-RO | confirmed |
| C2 | `CAP2` ceiling `0x6`, re-triggerable | confirmed |
| C3 | MAC-rate route untried | **already tried** (`GEN3_TRY`), but blind |

---

## D1 — the msgbox has been read at the wrong address the entire time

`0x660e0` is `NV_THERM_MSGBOX_COMMAND` **on NVSwitch**. The THERM block does not
sit at the same base on a GPU. Both facts are in this tree:

```
nvswitch/ls10/dev_therm.h:  NV_THERM              0x067fff:0x066000
nvswitch/ls10/dev_therm.h:  NV_THERM_I2CS_SCRATCH 0x000660bc
hopper/gh100/dev_therm.h:   NV_THERM_I2CS_SCRATCH 0x000200bc
```

Same register, same block offset `0x0bc`, two bases: NVSwitch `0x66000`, GPU
`0x20000`. `MSGBOX_COMMAND` is block offset `0x0e0`, so on a GA100 it is

```
NV_THERM_MSGBOX_COMMAND  ->  0x000200e0
```

Doc 15 flagged the lr10 provenance as an inference; doc 23 dropped the caveat and
kept the address. The probe in `0007` has been reading a register in a different
block on every run.

Second error in the same section: the probe is **not** gated. It sits inside the
postbl block immediately after `GEN3_P0`, unconditional. It has executed on every
patched boot since it was added, so its result is already in the `dmesg` history —
"untested" is wrong twice over.

**Actions.** Point the probe at `0x200e0`/`e4`/`e8`/`ec`/`f0`. Add
`{ 0x0020000, 0x1000, "THERM" }` to `--wide` so the next reference capture covers
it — the A100 dumps do not, so there is currently no reference for this register.

## D2 — `0x823824` is identical on both parts

```
0x0823824   A100 0x00000001    170HX 0x00000001
```

The A100 was training Gen4 x16 with that bit set. Doc 23's recommendation #2 —
the second-highest-priority action in the document — writes it to `0`. The
argument for it (`0007` comment: "lone set bit, same value as `OPT_GEN3`, has the
shape of a disable bit") is a shape argument that the reference capture, already
in the repo, refutes.

## D3 — four of the eleven `0x8238xx` deltas are self-inflicted

`0001-sec2-postbl-plm-ss-cfg.patch`:

```c
GPU_REG_WR32(pGpu, 0x0082381cU, 0x88888888U);
GPU_REG_WR32(pGpu, 0x00823820U, 0x00000008U);
{ 0x00823804U, 0xffffffffU, "FEAT"  },      /* PLM open */
```
`0007`:
```c
{ 0x00823800U, 0xffffffffU, "FEAT_OVR_ECC_PLM" },
```

and the 170HX capture reads back exactly those four values. Full block, ours
marked:

| offset | A100 | 170HX | |
|---|---|---|---|
| `0x823800` | `0xffffff8f` | `0xffffffff` | **ours** (PLM) |
| `0x823804` | `0xffffff8f` | `0xffffffff` | **ours** (PLM) |
| `0x823808` | `0x00100180` | `0x00000381` | genuine |
| `0x82380c` | `0x00011110` | `0x00888888` | genuine |
| `0x823810` | `0x00145414` | `0x002aaaaa` | genuine |
| `0x823814` | `0xef8ff100` | `0x00000233` | genuine, **RO** (D4) |
| `0x823818` | `0x00000000` | `0x00000000` | — |
| `0x82381c` | `0x51422106` | `0x88888888` | **ours** (SM speed) |
| `0x823820` | `0x00000006` | `0x00000008` | **ours** (SM speed) |
| `0x823824` | `0x00000001` | `0x00000001` | — (D2) |
| `0x823828` | `0x00000007` | `0x00000000` | genuine |
| `0x82382c` | `0x00000001` | `0x0000000a` | genuine |

Doc 23 nominated `0x823824` (identical), `0x82382c` (real, but no target value
given — it is `1` on the A100, not `0`) and `0x823808` (real). It did not mention
`0x823828`, which is the cleanest zeroed-on-170HX delta in the block.

This is the failure mode doc 00's method note names verbatim: *"the patch's own
writes look like reference differences."* It has now happened three times.

## D4 — `0x823814` is a published read-only readout

```
ampere/ga100/dev_fuse.h:
  NV_FUSE_FEATURE_READOUT               0x00823814  /* R--4R */
  NV_FUSE_FEATURE_READOUT_ECC_DRAM_ENABLED   0x00000001
```

A *readout*, not an override, and read-only by architecture. Its delta is a
consequence of the fusing, not a lever.

---

## W1 — the PLM data contradicts master-gating

Published PLM field layout (`nvswitch/ls10/dev_falcon_v4.h`, identical shape
across the family):

```
READ_PROTECTION        3:0     per level 0..3
WRITE_PROTECTION       7:4     per level 0..3
READ_VIOLATION         8
WRITE_VIOLATION        9
SOURCE_READ_CONTROL   10
SOURCE_WRITE_CONTROL  11
SOURCE_ENABLE        31:12     ALL_SOURCES_ENABLED = 0x000fffff  (20 PRI masters)
```

`SOURCE_ENABLE` **is** the master-identity check. Now the captures:

```
             A100 (stock)      170HX (at capture)
0x8200d0     0xffffff8f        0xffffffff
0x8200d4     0xfffffffc        0xffffffff
0x8200d8..f4 8f / fc mix       0xffffffff
0x8200f8     0xffffffff        0xffffffff
0x8200fc     0xffffffff        0xffffffff
```

Decoding the A100's stock values: `0x...8f` = read all levels, write
`ONLY_LEVEL3_ENABLED`. `0x...fc` = read levels 2–3, write all levels.

On the 170HX every one of those twelve dwords reads all-ones: **all four
privilege levels for read and write, and all twenty PRI sources enabled.** The
OPT write was refused in that state. If the gate were master identity expressed
through the PLM, granting all twenty sources would have opened it.

Two corollaries worth recording:

- **`0001`'s `OPT_PLM` open has always been a no-op.** `0x8200fc` already reads
  `0xffffffff` on the *stock* A100. The one PLM the patch deliberately opens was
  never closed.
- `0001` writes only `0x8200fc`, yet all twelve read all-ones on the 170HX while
  the A100 restricts ten of them. Either the 170HX ships with the fuse-block PLMs
  open, or a probe run left them open before the capture. **Clean test:** cold
  boot, stock driver, capture `0x8200d0`–`0x8200fc`. Cheap, and it is a real
  part difference if it holds.

The caveat, stated plainly: this is decisive only if one of those twelve governs
`0x820520`. Nobody has established which PLM covers the OPT bank. But the burden
now sits on the master-gating claim, not against it.

## W2 — the model doc 23 never considers

**The `OPT_*` registers are continuously re-driven outputs of the fuse sense
chain, not a gated register file.** Under this model there is no "right master"
and nothing to redirect to. It fits every observation on record:

| observation | fits? |
|---|---|
| `EN_SW_OVERRIDE` `0x820040` writable and persistent | yes — a control flop |
| `FUSECTRL`/`FUSEADDR`/`FUSEWDATA` `0x820000`–`0x820010` writable | yes — control flops |
| every `OPT_*` readout refuses, PLMs wide open | yes — output, not storage |
| `STATUS_OPT_*` published `R-I4R` | yes |
| `SENSE_CTRL` executes, resolves nothing | yes — array unchanged ⇒ re-drive unchanged |
| no `CTRL_OPT` bank exists on this chip | yes — `EN_SW_OVERRIDE` has no data bank to override *from*, so it is vestigial here |
| fuse array is a perfect mirror, no repair bank | yes |

Note what this model does **not** get to lean on: `NV_FUSE_OPT_NVDEC_DISABLE
0x00820378` is published `RW-4R` — read/write by architecture — and it still
refused (doc 21). So "architecturally read-only" is not the explanation; "written,
then immediately re-driven" is, and it produces the identical symptom under a
flushed re-read.

**Discriminating experiment — `OPT_WRSWEEP`.** Walk `0x820100`–`0x8207fc`
(448 dwords). For each, read, flip exactly one bit, write, flush 32 reads through
`PMC_BOOT_0`, re-read, count. Prefer flips that are safe if they stick: on the
170HX `0x820378 = 0x1f`, so *clearing* a bit re-enables an NVDEC rather than
disabling anything.

```
0 of 448 stick      ->  structural (W2). No master to find. Doc 23 vector B is void.
n > 0 stick         ->  selective gating (doc 23's model). The n that stick are the map.
```

Either result is worth more than any further single-address attempt, and it is one
boot.

## W3 — "app08's write lands" was never verified

The master-gating finding is one inference deep: app08 does
`ld / or 0x200000 / st` on `0x14820520`, our write to `0x820520` is refused,
therefore two masters, one allowed. The unstated premise is that app08's store
takes effect.

The only evidence is that the A100 reads `OPT_MAGIC = 0x00200000`. But bit 21 is
set on the 170HX as well — `0x16680000` contains `0x00200000`; doc 00's own
decomposition has it. So the observation is equally explained by the fuse carrying
bit 21 on both parts and app08's store being exactly as inert as ours.

There is no measurement anywhere in the corpus that distinguishes those. The
entire "different master" edifice rests on it.

## W4 — `LnkCap2` provenance is unobserved

Two claims are fused in doc 23 §1. Separate them:

- *Behaviour* — `LnkCap2 = (0x2 | v) & permitted`, bit 3 never granted, 11 values
  swept, trigger live and re-triggerable. **Solid.**
- *Provenance* — "a hardware latch in the PCIe hard IP, derived directly from
  `OPT_GEN23`/`OPT_GEN3`". **Never observed.** No ROM references `0x82057c` or
  `0x820580` either (doc 19), so the fuse→latch link is as unwitnessed as the
  firmware→`LnkCap2` link that doc 23 uses absence-of-reference to rule out. The
  same argument cannot be evidence in one direction and inference in the other.

And the identification itself is name-first: doc 19 says so — *"48 fuses differ
directionally in total, so the diff alone does not single these two out; what does
is that they were named before the data existed"* — from an external Mac fuse map
(doc 08). Two of 48, chosen by name. That is a reasonable bet, not a measurement.

## W5 — the calibration tables are unmeasured, not held at zero

Doc 23: *"the PHY calibration is zeroed and unpopulatable. The link would have
nowhere to train to."* The numbers say something narrower and more useful.

```
0x8e09c  0x00ba00bd     0x8e0c8  0x00b400c1
0x8e0a0  0x00ba00c0     0x8e0cc  0x00c200b5
0x8e0a4  0x00b900c3     0x8e0d0  0x00b900cb
0x8e0a8  0x00c500b8     0x8e0d4  0x00bb00b8
```

Eight dwords, sixteen 16-bit values, one per lane, every one distinct and all
clustered in `0x00b4`–`0x00cb`. A fuse mirror does not produce sixteen
near-but-different values; a **measurement** does. Same shape in the rate table:
`0x8e010`–`0x8e04c` is fifteen entries of `4` and one of `3` (`0x8e03c`, lane 11).
A fused capability would not have one odd lane.

So this is the output of a per-rate PHY calibration pass that runs on the A100 and
does not run here. The distinction matters because it changes the target: not
"write the table" (confirmed RO, `took=0/16`) but "find what runs the pass."
Doc 19 already reached exactly this for the XP pair — *"some other initialization
populates them"* — and doc 23 collapsed it back into a fuse holding a table down.

Doc 19's own caveat also survives and doc 23 dropped it: those coefficients are
from an A100-SXM4 board, and per-lane calibration is board-derived, so they were
never the right values to write here even if the write had taken.

## W6 — app08 may not run on the falcon we have code execution on

The VBIOS FALCON UCODE TABLE (doc 17) targets app08 at `0x01 DEVINIT` —
enumerated separately from `0x05 PMU` and `0x07 GSP`. Our ROP chain executes in
the SEC2 Booter.

Nowhere in the corpus is it established which physical falcon executes the
`DEVINIT`-target ucode. If it is not SEC2, then "redirect control flow from the
Booter into app08's code" is not a gadget problem — there is no control-flow edge
to find, because the code is not in SEC2's IMEM. And PRI master IDs are per
engine, so a master-gating model *predicts* SEC2 can never be the accepted master
regardless of what it executes.

Doc 23 lists the blockers as encrypted-image gadget discovery and shared
stack/DMEM. The first blocker is upstream of both: identify the engine. That is
offline work on the ucode table, not card time.

---

## C3 — the MAC-rate route is not untried, it is unmeasured

Doc 23 does not mention it, and doc 19 says *"prior Gen3 attempts wrote `CAP2`
and hoped."* `GEN3_TRY` in `0007` does more than that:

```c
GPU_REG_WR32(pGpu, 0x0008c040U, (sMaxRate & ~0x000C0000U) | (0x3U << 18));
GPU_REG_WR32(pGpu, 0x0008c1c0U, 0x00340036U);
GPU_REG_WR32(pGpu, 0x000880a8U, ...);            /* LnkCtl2 target speed */
```

It sets the MAC rate field to 3 and the PL link rate, i.e. it asks the LTSSM to
advertise Gen3 independently of what config-space `LnkCap2` reports. That is the
right experiment. It was run and it did not produce Gen3.

But it was run **blind.** Without `LTSSM_STATE` there is no way to tell

```
never entered Recovery.Equalization    ->  the rate request is being clamped
entered RECOVERY_EQZN and fell back    ->  the request works, EQ fails
```

apart. Those two lead to completely different next moves. This is why the msgbox
matters, and why D1 is the top of the list — the one instrument that separates
them has been pointed at a register in the wrong block since it was written.

Side note on `0x8c040`: the A100 reads `0x80004c00`, bits[19:18] = `0`, while
training Gen4. So the field is an override/CYA, not a max-rate readout. The
Gen2-era naming in the patch is misleading and should not be used as evidence
about what the A100 does.

---

## Ground doc 23 calls exhausted and never looked at

`0x8b000`–`0x8b7ff` — **193 differing dwords**, the largest non-board-derived
delta in the whole capture, analysed in no document. Structure:

```
8 blocks, stride 0x100, base 0x8b000
each block: 12 dwords at +0x050..0x07c, 12 dwords at +0x0d0..0x0fc
values: pairs of 11-bit fields, both parts populated (not a zeroing)
all 8 blocks byte-identical within a part
```

Populated on both parts, so not the same class as the `0x8e` zeroing — but 8
identical copies of a 24-dword table that differs between a Gen4 part and a Gen1
part is worth one structured pass. `0x8b980` (the register app08's link routine
touches, doc 21) is `0x0200a067` on both — identical, so that specific address is
not it.

`0x8d` — 9 deltas, unexamined. `0x8d90c` is `0x0000ffff` / `0x0000000f`, a
lane-enable mask tracking link width, not a Gen3 lever. `0x8d114`, `0x8da10`,
`0x8da14`, `0x8da18` are unaccounted.

`0x118` — 48 deltas in the PGC6/AON island, unexamined. Most are pointer tables
(`0x13f5xxxx` vs `0x0ff6xxxx`), but `0x118fb0` (`0xc0000703`/`0xc0000107`),
`0x118fc0` (`0x0000004f`/`0x000007ff`) and `0x118e00` (`0x160`/`0x060`) are
small-field straps in the island that survives warm reboot.

---

## Corrected priority

1. **Fix the msgbox address** to `0x200e0` and grep the existing `dmesg` history
   for what the `0x660e0` probe has been returning. One rebuild, no risk.
2. **`OPT_WRSWEEP`** — decides W1 vs W2, i.e. decides whether doc 23's vector B
   is a real vector or a phantom. One boot.
3. **`LTSSM_STATE` during a `GEN3_TRY` run**, if the mailbox is live. Settles
   "clamped" vs "attempted and failed EQ" — the one question that has been
   unanswerable since doc 13.
4. **`0x823828` / `0x82382c` / `0x823808`** from the Booter — the real deltas,
   with the A100 values as targets (`7`, `1`, `0x00100180`). Not `0x823824`.
5. **`0x8b0xx` structured pass** — 193 dwords nobody has read.
6. **Identify the falcon that runs `DEVINIT`-target ucode** before any gadget
   work. Offline, and it may close vector B outright.

## What doc 00 needs

- "Why every software write fails" states master-gating as settled. Downgrade to
  a hypothesis, add W2 as the competing model and `OPT_WRSWEEP` as the decider.
- Add the `0x823800` block to the contamination method note — `0x82381c`,
  `0x823820` (values) and `0x823800`, `0x823804` (PLMs) are ours.
- The XP3G calibration row should read "never measured on this part", not
  "held at zero by the fuse" (W5).
- Tooling table: `--wide` is missing `0x20000` (THERM) — no reference capture
  exists for the mailbox.

# 25 — Counter-cases to doc 23

[Doc 24](24-red-team-of-23.md) says what is wrong with [doc 23](23-gen3-proposal-and-red-team.md).
This says what replaces it. One counter-case per case doc 23 makes, each with the
evidence and the probe that settles it.

The new result driving most of this: **app08's write and our write are separated
by a boot phase, not (necessarily) by a master identity — and the phase has a
published witness we already captured on both parts.**

---

## 0. The phase result

Three facts, all from the driver tree and the existing captures:

```
NV_PGC6_AON_SECURE_SCRATCH_GROUP_05(0) = 0x00118234   GFW_BOOT
  PROGRESS 7:0    COMPLETED = 0xff                    (tu102 + ga102 dev_gc6_island*)

0x0118234    A100 0x000003ff    170HX 0x000003ff      PROGRESS = 0xff on both
```

```c
/* kernel_gsp_frts_tu102.c:481 — FWSEC runs on GSP, not SEC2 */
status = kgspExecuteHsFalcon_HAL(pGpu, pKernelGsp, pPreparedCmd->pFwsecUcode,
                                 staticCast(pKernelGsp, KernelFalcon), NULL, NULL);
```

The driver only ever loads two ucodes onto silicon: **FWSEC onto GSP** and the
**Booter onto SEC2**. `app08` is neither — the VBIOS ucode table targets it at
`0x01 DEVINIT` (doc 17), which runs in the pre-driver GFW boot sequence. By the
time either driver-side path executes, `GFW_BOOT_PROGRESS` already reads
`COMPLETED` on both parts.

So the shape of doc 23 §2 is right — one write lands, ours does not — but the
variable it names is not the only candidate, and the phase variable is the one we
can actually witness. Doc 23's assumption 2 raised "a closed window" and dismissed
it with *"`EN_SW_OVERRIDE` was set before the Booter ran, and the OPT bank still
refused."* That is a non-sequitur: `EN_SW_OVERRIDE` being set says nothing about
whether the fuse-block write port is still open. The window model is untouched by
it.

**Why this matters more than the master question.** A master can in principle be
impersonated. A phase that has already completed cannot be re-entered by anything
running after it — which reorders every vector below.

---

## 1. Counter to §1 — "LnkCap2 is hardware-composed"

**Doc 23:** no firmware references `0x880a4` or `0x8872c`, therefore the mask is a
hardware latch derived directly from `OPT_GEN23`/`OPT_GEN3`, therefore no firmware
change can move the advertise.

**Counter:** the behaviour is established, the provenance is not, and the
conclusion overreaches on the part that is not established.

- *Established:* `LnkCap2 = (0x2 | v) & permitted`, bit 3 never granted, 11 values
  swept in one boot, trigger live and re-triggerable. Nothing here disputes it.
- *Not established:* that `permitted` comes from `OPT_GEN23`/`OPT_GEN3`. No ROM
  references those either (doc 19). Doc 23 uses absence-of-reference to rule the
  firmware path out and then infers the fuse path — from absence-of-reference.
- *Not established:* that the fuse→latch path is what firmware cannot touch. The
  claim "no firmware modification can change the advertised speed" is only sound
  if the latch is combinational from OTP. If any part of it is sampled at a
  boot-phase event, firmware inside that phase reaches it.

**Also note the identification is name-first.** Doc 19: *"48 fuses differ
directionally in total, so the diff alone does not single these two out; what does
is that they were named before the data existed"* — from the external map in doc
08. Two of 48, chosen by name.

**Settles it:** the offline app08 diff in §5 below. If the higher-rate path is
skipped by a conditional that reads a fuse, the disassembly gives the fuse address
directly, and the name-first identification either confirms or falls.

## 2. Counter to §2 — "the OPT bank is master-gated"

**Doc 23:** app08 writes `0x820520`, our identical Booter write is refused, both
are L3 falcon writes, therefore master identity is the check; therefore N writes
do not help; therefore find app08's master.

**Counter: three independent problems.**

**(a) The PLM data contradicts it.** `SOURCE_ENABLE` (bits 31:12, twenty PRI
masters, `nvswitch/ls10/dev_falcon_v4.h`) *is* the master check. At capture the
170HX read `0xffffffff` across `0x8200d0`–`0x8200fc` — all four levels R/W, all
twenty sources — and the write was still refused. Also `0x8200fc` already reads
all-ones on the *stock* A100, so `0001`'s `OPT_PLM` open has always been a no-op.

**(b) The premise is unverified.** Nothing shows app08's store *lands*. The
evidence is the A100 reading `OPT_MAGIC = 0x00200000` and app08 OR-ing in
`0x200000` — but bit 21 is set on the 170HX too (`0x16680000` contains it). "The
fuse carries bit 21 on both parts and app08's store is as inert as ours" fits the
same data.

**(c) A simpler model fits everything.** `OPT_*` are re-driven sense-chain outputs,
not storage. Control flops in the same block are writable and persistent
(`EN_SW_OVERRIDE 0x820040`, `FUSECTRL`/`FUSEADDR`/`FUSEWDATA`
`0x820000`–`0x820010`); every readout refuses; `STATUS_OPT_*` is published
`R-I4R`; `SENSE_CTRL` resolves nothing because the array never changed; there is
no `CTRL_OPT` bank for `EN_SW_OVERRIDE` to override *from*.
`NV_FUSE_OPT_NVDEC_DISABLE 0x820378` is published `RW-4R` and still refuses, so
"architecturally RO" is not it — "written, then re-driven" is, and it is
indistinguishable from refusal under a flushed re-read.

**Settles it:** `OPT_WRSWEEP` (§8, probe 2). `0/448` ⇒ model (c), no master
exists to find, doc 23 vector B is void. Nonzero ⇒ selective gating, and the ones
that stick are the map.

## 3. Counter to §3 — "every Gen3+ table is host-RO, so there is nowhere to train to"

**Doc 23:** five tables populated on the A100, zero here, all refuse writes;
therefore even a Gen3 advertise has no calibration behind it.

**Counter:** the RO half is right and the "held at zero by the fuse" half is not
what the numbers say.

```
0x8e09c  0x00ba00bd     0x8e0c8  0x00b400c1
0x8e0a0  0x00ba00c0     0x8e0cc  0x00c200b5
0x8e0a4  0x00b900c3     0x8e0d0  0x00b900cb
0x8e0a8  0x00c500b8     0x8e0d4  0x00bb00b8
```

Sixteen 16-bit values, one per lane, every one distinct, all inside
`0x00b4`–`0x00cb`. A fuse mirror does not produce sixteen near-but-different
values. Neither does it produce `0x8e010`–`0x8e04c` = fifteen entries of `4` and
one of `3` (`0x8e03c`, lane 11). Both are the signature of a **measurement**.

So the table is *never measured on this part*, not *held down*. Doc 19 reached
exactly this for the XP pair — *"some other initialization populates them"* — and
doc 23 collapsed it back.

Two consequences doc 23 misses:

- The target moves from "write the table" (confirmed RO, `took=0/16`) to "find
  what runs the pass." That is firmware behaviour, which is reachable in a way a
  fuse mirror is not.
- Doc 19's caveat still stands and doc 23 dropped it: those coefficients came off
  an **A100-SXM4** board and per-lane calibration is board-derived, so they were
  never the right values for this board even if the write had taken. `XP3G_LANE`
  was testing a mechanism, not a fix.

**Settles it:** the offline app08 diff in §5.

---

## 4. Counter to vector A — SMBPBI

**Doc 23:** mailbox at `0x660e0`, probe coded, untested, check `dmesg`.

**Counter: wrong address, and it is not untested.**

```
nvswitch/ls10/dev_therm.h:  NV_THERM              0x067fff:0x066000
nvswitch/ls10/dev_therm.h:  NV_THERM_I2CS_SCRATCH 0x000660bc
hopper/gh100/dev_therm.h:   NV_THERM_I2CS_SCRATCH 0x000200bc
```

Same register, block offset `0x0bc`, two bases — NVSwitch `0x66000`, GPU
`0x20000`. `MSGBOX_COMMAND` is block offset `0x0e0`:

```
GA100 NV_THERM_MSGBOX_COMMAND  =  0x000200e0
```

Doc 15 flagged the lr10 provenance as an inference; doc 23 kept the address and
dropped the caveat. And the probe is ungated — it sits directly after `GEN3_P0`
inside the postbl block, so it has run on every patched boot since it was added.
Its `0x660e0` result is already in the `dmesg` history.

**Counter-case:** this is not vector A, it is vector 0. Without `LTSSM_STATE`,
§C3 below cannot be resolved at all, so everything else is guesswork about what
already happened.

**Run:** probe 1 (§8), then `grep MSGBOX` on the existing history for the
`0x660e0` value as a control.

## 5. Counter to vector B — "redirect execution into app08's context"

**Doc 23:** find gadgets in the encrypted Booter, understand booter→fwsec→app08
control flow, jump into app08's OPT write.

**Counter: there is no control-flow edge, and there may be no destination.**

- **No edge.** The driver runs FWSEC on **GSP** (`kgspExecuteHsFalcon_HAL(...,
  staticCast(pKernelGsp, KernelFalcon), ...)`) and the Booter on **SEC2**. app08
  is `0x01 DEVINIT`, executed in the GFW boot phase, before either. There is no
  "booter → fwsec → app08" chain to walk; those are three different execution
  epochs, two of them on different falcons.
- **Possibly no destination.** Under §2(c) there is no accepted master at all.
- **Wrong axis anyway.** §0 says the difference is phase. Nothing running after
  `GFW_BOOT_PROGRESS = COMPLETED` re-enters it by jumping.

**Counter-vector B′ — replay into the boot phase.** The one mechanism that
executes register writes *inside* the pre-driver phase without touching a signed
image is the AON/BSI boot script that the island replays on GC6 exit. Status:
unproven here, and the control registers are not in the published headers. What is
published anchors the block:

```
NV_PGC6_BSI_SECURE_SCRATCH_14   0x001180f8      (tu102 + ga102)
```

and the captures already cover `0x118000`–`0x118fff` on both parts. Two
observations from that window worth one pass:

```
0x118038  0x009c1000   identical both parts
0x11803c  0x00118e90   identical both parts   <- a PRI address, and it is the
                                                 register doc 17's 0xcb00 routine
                                                 was supposed to write
0x118060..0x1180bc     PLM-shaped run (0xffffffcc / cf / 88 / 8f)
```

`0x11803c` literally holding `0x118e90` reads like a sequencer's target-address
register. Recon before anything else: locate `RAMCTRL`/`RAMDATA`-shaped pairs in
the captured window, confirm against the A100, and only then consider writing.

**Do this first, offline, zero card time:** the two `app08` images are plaintext
and both extracted (`fwsec/app08_170hx_imem.bin`, the A100 build). Doc 17 already
found the 170HX build references **76 PHY registers the A100 build never touches**.
Diff the two around the `0x8e0xx` writes and find the conditional that decides
whether the higher-rate calibration pass runs. If it reads a fuse, that fuse
address is the answer to §1 and §3 simultaneously — and it costs nothing.

## 6. Counter to vector C — VBIOS / signing

**Doc 23:** most direct, but `LnkCap2` is hardware-composed so changing app08
cannot change the advertise; and the PHY tables are fused to zero so a modified
app08 would read zero and write zero. Blocker is image signing.

**Counter: doc 23 argues itself out of its own vector, on the two premises §1 and
§3 just weakened. Under the corrected reading it is the *coherent* vector, for a
different reason.**

- §0: the boot phase is the only context where the accepted writes happen, and
  VBIOS is the only code that runs there.
- §3: the calibration tables are not read from a fuse and written back — they are
  *measured*. A modified app08 that runs the pass produces values; it does not
  copy zeros.
- §1: the fuse→`permitted` path is unwitnessed, so "changing app08 cannot change
  the advertise" is an assumption, not a result.

What does not change: signing is still the blocker, and it is still a different
class of problem. But the ordering argument in doc 23 — "pursue it only if 1–4 are
negative" — was built on the two premises above.

**Cheap precursor, no signing:** §5's offline app08 diff. It tells you whether
there is anything worth signing *for* before any crypto work starts.

## 7. Counter to vector D — "feature-override shadow from Booter"

**Doc 23:** write `0x823824 = 0`, plus `0x82382c` and `0x823808`, via the ROP
harness.

**Counter: the primary target is identical on both parts, and four of the block's
eleven deltas are our own writes.**

| offset | A100 | 170HX | |
|---|---|---|---|
| `0x823800` | `0xffffff8f` | `0xffffffff` | **ours** — PLM open (`0007`) |
| `0x823804` | `0xffffff8f` | `0xffffffff` | **ours** — PLM open (`0001`) |
| `0x823808` | `0x00100180` | `0x00000381` | genuine |
| `0x82380c` | `0x00011110` | `0x00888888` | genuine |
| `0x823810` | `0x00145414` | `0x002aaaaa` | genuine |
| `0x823814` | `0xef8ff100` | `0x00000233` | genuine — **`NV_FUSE_FEATURE_READOUT`, published `R--4R`** |
| `0x823818` | `0x00000000` | `0x00000000` | — |
| `0x82381c` | `0x51422106` | `0x88888888` | **ours** — `0001` SM speed |
| `0x823820` | `0x00000006` | `0x00000008` | **ours** — `0001` SM speed |
| `0x823824` | `0x00000001` | `0x00000001` | **identical — doc 23's rec. #2** |
| `0x823828` | `0x00000007` | `0x00000000` | genuine — cleanest zeroed delta, **not in doc 23** |
| `0x82382c` | `0x00000001` | `0x0000000a` | genuine — target is `1`, doc 23 gives none |

**Counter-target list**, in this order, A100 values as targets:

```
0x823828 = 0x00000007      zeroed here, set there
0x82382c = 0x00000001
0x823808 = 0x00100180
```

Skip `0x823824` (identical) and `0x823814` (published read-only). Note the whole
block is a *readout* block by the one name published for it, so expect refusal —
this is a cheap test, not a likely lever, and it should be ranked accordingly.

---

## 8. Probes

Four changes. Two read-only, one host-write sweep, one dumper region.

**Probe 1 — msgbox, corrected.** Replace the `0x660E0` block in `0007`:

```c
/* NV_THERM base is 0x20000 on a GPU, 0x66000 on NVSwitch. Same register
   NV_THERM_I2CS_SCRATCH sits at 0x200bc (gh100) and 0x660bc (ls10), so the
   block offset of MSGBOX_COMMAND (0x0e0) puts it at 0x200e0 here. The old
   0x660e0 was the NVSwitch address -- see docs/24. */
{
    NvU32 mbCmd = GPU_REG_RD32(pGpu, 0x000200E0U);
    NvU32 mbD1  = GPU_REG_RD32(pGpu, 0x000200E4U);
    NvU32 mbD2  = GPU_REG_RD32(pGpu, 0x000200E8U);
    NvU32 mbD3  = GPU_REG_RD32(pGpu, 0x000200ECU);
    NvU32 mbD4  = GPU_REG_RD32(pGpu, 0x000200F0U);
    NvU32 mbOld = GPU_REG_RD32(pGpu, 0x000660E0U);   /* control */
    NV_PRINTF(LEVEL_ERROR,
              "SEC2_DEBUG: GEN3_P0 MSGBOX cmd(0x200e0)=0x%08x "
              "e4=0x%08x e8=0x%08x ec=0x%08x f0=0x%08x old(0x660e0)=0x%08x "
              "(0xbadf -> walled; else STATUS=[28:24] INTR=[31]; "
              "next: NULL_CMD=0x80000000 GET_CAP_DWORD=0x80000201 "
              "PAGE_6_LTSSM=0x80000621)\n",
              mbCmd, mbD1, mbD2, mbD3, mbD4, mbOld);
}
```

**Probe 2 — `OPT_WRSWEEP`.** Host context, one boot, decides §2. Not the Booter
path: 448 Booter loads is not a probe, and the question is whether the bank accepts
*any* write with the PLMs open. Escalate to the ROP only for addresses that stick.

```c
/* CmpOptSweep=1 -- is the OPT readout bank writable at all?
   Single-bit flip per dword, flushed re-read (posted writes, docs/00).
   Direction chosen so a stuck write is benign: clear a set bit where the
   fuse reads nonzero (e.g. 0x820378 = 0x1f re-enables an NVDEC), set bit0
   otherwise. */
{
    NvU32 sweep = 0, a, took = 0, tried = 0;
    (void)osReadRegistryDword(pGpu, "CmpOptSweep", &sweep);
    if (sweep != 0)
    {
        for (a = 0x00820100U; a < 0x00820800U; a += 4U)
        {
            NvU32 cur = GPU_REG_RD32(pGpu, a), want, rd, fl;

            if ((cur & 0xBADF0000U) == 0xBADF0000U)   /* priv-blocked */
                continue;
            want = (cur != 0) ? (cur & ~1U) : 1U;
            if (want == cur)
                continue;

            GPU_REG_WR32(pGpu, a, want);
            for (fl = 0; fl < 32U; fl++)
                (void)GPU_REG_RD32(pGpu, 0x00000000U);
            rd = GPU_REG_RD32(pGpu, a);
            tried++;
            if (rd == want)
            {
                took++;
                NV_PRINTF(LEVEL_ERROR,
                          "SEC2_DEBUG: OPT_WRSWEEP TOOK 0x%06x 0x%08x->0x%08x\n",
                          a, cur, rd);
                GPU_REG_WR32(pGpu, a, cur);           /* restore */
            }
        }
        NV_PRINTF(LEVEL_ERROR,
                  "SEC2_DEBUG: OPT_WRSWEEP end took=%u/%u PLM(0x8200fc)=0x%08x "
                  "GFW(0x118234)=0x%08x\n",
                  took, tried, GPU_REG_RD32(pGpu, 0x008200FCU),
                  GPU_REG_RD32(pGpu, 0x00118234U));
    }
}
```

```
took=0/N   -> model (c). No master exists. Vector B is void, vector C is the
              only remaining software path.
took>0     -> selective gating. The list is the map; escalate each to the
              ROP path and check CAP2.
```

**Probe 3 — phase witness**, one line in the existing `GEN3_P0` print:

```c
GPU_REG_RD32(pGpu, 0x00118234U)   /* GFW_BOOT: PROGRESS[7:0], COMPLETED=0xff */
```

Both captures read `0x000003ff`, so this is a regression witness rather than a
differentiator — but every future "the write refused" line should carry the phase
it refused in.

**Probe 4 — dumper region.** `recon/ga100-bar0-dump.c`, `wide[]`:

```c
{ 0x0020000, 0x1000, "THERM (SMBPBI mailbox at 0x200e0)" },
```

There is currently **no reference value** for the mailbox — the A100 captures do
not cover `0x20000`. Without it, a plausible-looking read on the 170HX cannot be
distinguished from a plausible-looking read on any GA100.

## 9. Counter-priority

Doc 23's order was: SMBPBI → `0x823824` → SMBPBI phase 0.5 → gadgets → signing.

```
1. app08 offline diff (170HX vs A100, around the 0x8e0xx writes)
      no card time, no signing; answers §1 and §3 together, and tells you
      whether vector C has a destination
2. Probe 1 + Probe 3, and grep the existing dmesg for the old 0x660e0 value
      one rebuild, read-only
3. OPT_WRSWEEP
      one boot; decides whether vector B exists
4. LTSSM during GEN3_TRY, if the mailbox is live
      the question open since doc 13
5. 0x823828 / 0x82382c / 0x823808 from the Booter
      cheap, low expectation, a readout block by its one published name
6. 0x8b000-0x8b7ff structured pass
      193 differing dwords in no document
7. BSI/AON replay recon in the captured 0x118000 window
      only if 3 says a write can land somewhere
```

Gadget discovery in the encrypted Booter does not appear. Under §5 it has no
destination to reach and no edge to reach it by; it should not be attempted until
step 3 says otherwise.

## 10. Doc 23's assumptions, resolved

| # | doc 23's assumption | status |
|---|---|---|
| 1 | `LnkCap2` hardware-composed | behaviour holds; **provenance still unwitnessed** (§1) |
| 2 | OPT bank master-gated, not a window | **inverted** — the phase witness is published and the window argument was dismissed by a non-sequitur (§0) |
| 3 | Booter and app08 have different masters | **may be moot** — different *phases*, and possibly different falcons (§5) |
| 4 | PHY tables zeroed by fuses, not by firmware logic | **firmware logic is now the better reading** (§3) |
| 5 | `0x823824` shares the OPT gating | **void** — identical on both parts (§7) |
| 6 | Gen3 needs EQ we cannot measure | **measurable**, at `0x200e0` (§4) |

## 11. What would falsify this document

- `OPT_WRSWEEP` returning `took>0` kills §2(c) and puts doc 23's master model back
  in front.
- `0x200e0` reading `0xbadf1100` closes §4 and leaves §C3 permanently blind from
  driver context; the fallback is SMBus, which is a different kind of work.
- The app08 diff showing the calibration pass is *unconditional* in both builds
  would mean the difference is in the data it reads, which puts §3 back toward
  doc 23's reading.
- The fuse-block PLMs reading restricted values on a **cold-boot stock-driver**
  170HX capture would mean the all-ones values were ours after all, and §2(a)
  weakens to "we opened them and it still refused" — which is a weaker claim, but
  not a dead one.

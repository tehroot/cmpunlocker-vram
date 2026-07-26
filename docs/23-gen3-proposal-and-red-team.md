# 23 — Gen3 proposal: where we are, where we can't go, what remains

> **Read with [doc 24](24-red-team-of-23.md).** Four claims below are refuted
> from artifacts already in the repo: the SMBPBI mailbox is `0x200e0` on a GPU,
> not `0x660e0` (and the probe is ungated, so it has already run); `0x823824` is
> `0x00000001` on the A100 too, so recommendation #2 is void; four of the eleven
> `0x8238xx` deltas are our own writes; `0x823814` is a published `R--4R`
> readout. §2's master-gating is contradicted by the PLM readback. §3's
> "unpopulatable" should read "never measured here".

Reframing from the review of docs 00–22, couching everything against the goal:
**arbitrary commands to retrain the PCIe link to Gen3 on the CMP 170HX.**

---

## What works

VRAM unlock + PCIe Gen2, reproducible on Debian 13 / AMD 9950X AM5. Tag
`gen2-am5-known-good` (`f27a663`), patches `0001`–`0008`.

```
64 GB visible          (stock: 8 GB)
PCIe Gen2              (stock: Gen1)
```

Mechanism: SEC2 Booter signature-payload exploit → one arbitrary HS/L3 MMIO write
per Booter load → opens PLMs, extends PMA, drives Gen2 advertise + retrain.

File-driven ROP harness (`dmem.bin` + `RAW_BOOTER`) executes verbatim without
rebuild. Control passed: `EN_SW_OVERRIDE` went `0 → 1` from cold start.

---

## The three findings that close the register-write approach

These are tested on-card against a live A100-SXM4-80GB (Gen4 x16).

### 1. LnkCap2 is hardware-composed, not firmware-set

PUBSWEEP established: `LnkCap2 = (0x2 | value) & permitted` with bit3
(8.0 GT/s) permanently clear in `permitted`. Full app08 disassembly
(23155 instructions) confirms **no firmware references `LnkCap2` (`0x880a4`)
or the publish trigger (`0x8872c`).** The mask is a hardware latch in the PCIe
hard IP, derived directly from `OPT_GEN23`/`OPT_GEN3` fuses.

**Consequence:** no firmware modification, no devinit table edit, no VBIOS patch
can change the advertised link speed. The advertise is composed in silicon, not
in software. Any route that goes through "write the advertise differently" is
closed by construction.

### 2. The OPT bank is master-gated, not privilege-gated

app08 writes `OPT_MAGIC` (`0x820520`) unconditionally every boot. Our identical
SEC2 Booter payload write to the same address is refused. Both are privileged
falcon-context writes. Only one lands.

This retrospectively explains three failed approaches: `EN_SW_OVERRIDE=1`, the
fuse-block PLM being open, and `SENSE_CTRL` re-sensing. None addressed what was
actually being checked.

**Consequence:** extending the ROP chain from 1 write to N writes doesn't help.
The Booter's master identity is what's being refused, not its privilege level.
More capability in the wrong context is still the wrong context.

### 3. Every Gen3+ configuration table is read-only from the host

The A100 diff located five tables that are populated on a Gen4 part and zero
on the 170HX:

| table | region |
|---|---|
| `0x8c498` / `0x8c49c` | XP |
| `0x8890c`..`0x88928` | XVE per-lane |
| `0x88c3c`..`0x88c48` | XVE per-lane |
| `0x8e010`..`0x8e04c` | XP3G per-lane rate |
| `0x8e094`..`0x8e0a8`, `0x8e0c8`..`0x8e0d4` | XP3G per-rate calibration |

All refuse host writes (verified with flushed re-read). The XP3G override
registers (`OVR`/`VAL`) accept writes but only override `STATUS3`/`OPT_MAGIC` —
which we already know is master-gated. The data tables themselves are RO.

**Consequence:** even if we could advertise Gen3, the PHY calibration is zeroed
and unpopulatable. The link would have nowhere to train to.

---

## Closed routes

Each tested on-card and closed, not assumed. (Full evidence in doc 00 and doc
19. Highlighting the closures relevant to this proposal.)

| route | result |
|---|---|
| `LnkCap2` publish trigger | bit3 never granted, any input, live and re-triggerable |
| OPT bank writes (Booter) | refused — wrong master |
| `EN_SW_OVERRIDE` | writable but `0` on both parts, not a differentiator |
| XP3G per-rate tables | 0/16 writes took, RO |
| XP straps | host-RO; one writable register moved nothing |
| XVE window | five registers took A100 values, `CAP2` unmoved |
| Legacy fuse alias `0x21000` | Turing-era base, writes don't reach Ampere shadow |
| `FUSECTRL SENSE_CTRL` | executes; does not re-resolve `OPT_*` |
| `0xcb00` / `0x118f78` gate | all four witnesses identical on a Gen4 part |
| devinit table modification | devinit only reads advertise, never writes it |
| app08 modification (ROM) | `LnkCap2` is hardware-composed; changing app08 doesn't change it |

---

## Remaining vectors

### A. SMBPBI mailbox reachability

SMBPBI `GET_PCIE_LINK_INFO` (opcode `0x21`) exposes read-only `LTSSM_STATE` and
per-lane, per-direction Gen3 EQ coefficients. The mailbox is at `0x660e0`, deep
inside the 16 MB BAR0 aperture.

**Status:** probe coded in `0007` (lines 531–553), runs unconditionally. Output
is `dmesg | grep "GEN3_P0 MSGBOX"`. **Untested.**

**Why it matters:** if reachable, it gives us in-band telemetry for any future
Gen3 train attempt — LTSSM state, EQ coefficients, failure mode. Without it we
can't diagnose why a train fails. Cheap test, no writes, no risk.

**Action:** check `dmesg` on the target. If `0xbadf1100`, mailbox is priv-blocked
from PL0 and the instrument is moot. If plausible, proceed to Phase 0.5 (NULL_CMD
then GET_CAP_DWORD).

### B. Redirect execution to app08's context

The OPT bank accepts writes from app08's master identity. The ROP harness
executes in the Booter's master identity. If we can redirect control flow from
the Booter into app08's code or into a context that shares app08's master
identity, the OPT bank becomes writable.

**What's needed:**
1. Gadget addresses in the Booter image — currently unverified (image is AES-
   encrypted, entropy 7.93-7.96 bits/byte).
2. Understanding of the execution chain: how does control pass from booter →
   fwsec → app08 → devinit? Is there a shared stack, shared DMEM region, or
   jump table?
3. A jump target in app08's IMEM that performs the OPT write and returns or
   continues init without wedging.

**Why this is hard:**
- The Booter image is encrypted. Static disassembly produces garbage. Gadget
  discovery requires empirical probing — one trial per ~40 s (file copy +
  modprobe), most trials will hang GSP.
- app08 is plaintext but has `halt_baddata()` regions where ghidra_falcon's
  Sleigh lacks opcodes. The 376-instr link-setup routine (`ca9b`–`cb00`) is
  only partially decompiled.
- Even if we reach app08's code, we need the master identity to persist, not
  just the instruction pointer.

**Risk profile:** high. Requires new code in the driver, empirical gadget
discovery, and on-card testing. May not work. May wedge the card.

### C. VBIOS / signed image modification

If the goal is "change what app08 computes and writes," the VBIOS route is
the most direct. But:
- `LnkCap2` is hardware-composed — changing app08 can't change the advertise.
- The PHY calibration tables (`0x8e010`..`0x8e04c`) are populated by firmware
  at boot and are RO afterwards. If app08 writes them during init, a modified
  app08 could populate them. But they're currently fused to zero, so app08
  reads zero and writes zero.
- Image signing (OMGVflash) is the blocker. The `*_sec.bin` HS tails are the
  target for signature analysis.

**Why this is the most honest path:** it's the one that doesn't require
circumventing master-gating or hardware latches. It requires changing the
firmware that the hardware trusts. But it's also a different class of problem
(crypto, signing infrastructure) from everything attempted so far.

### D. Booter context can reach targets other than OPT

The master-gating finding only applies to the OPT bank. It doesn't mean every
HS register is master-gated. There may be registers the Booter context can
write that, when written, cause the hardware to re-evaluate the `permitted`
mask or re-trigger the fuse read path.

**Candidates (all untested from Booter context):**
- `0x823824` — feature-override shadow, `0x00000001`, same value as `OPT_GEN3`
  (doc 19, FEAT_DUMP block at `0x823800`..`0x82382c`). Pry 2.1 puts "a PCIe
  boot-speed (Gen3-disable) bit" here.
- `0x823808`/`0x82382c` — unaccounted in Pry, in the same block.
- `FUSECTRL SENSE_CTRL` — executed, didn't re-resolve, but only tested from
  host context.

**Action:** use the ROP harness to write these from the Booter context, not the
host. The host refused writes to `0x823800`+; the Booter might succeed.

---

## Recommendation

Priority order, by expected return on effort:

1. **SMBPBI reachability** — one `dmesg` grep. If it works, we gain a
   measurement instrument for every subsequent attempt. If it doesn't, we close
   doc 15.

2. **Feature-override shadow from Booter** — write `0x823824 = 0`, `0x82382c`,
   and `0x823808` via the ROP harness. These are in the same block Pry identifies
   as having a "PCIe boot-speed bit." The host can't write them, but the Booter
   might. If `CAP2` moves, that's the answer.

3. **SMBPBI Phase 0.5** — if step 1 succeeds, send NULL_CMD + GET_CAP_DWORD.

4. **Gadget probing** — only if steps 1–3 are negative. Empirical search for
   read/RMW gadgets in the encrypted Booter image. High effort, uncertain
   payoff.

5. **VBIOS signing analysis** — the honest long shot. Different class of
   problem. Only worth pursuing if 1–4 are negative and the goal is worth the
   risk profile.

---

## Assumptions that could be wrong

Red-team these:

1. **"LnkCap2 is hardware-composed."** Evidence: full app08 disassembly has no
   reference to `0x880a4` or `0x8872c`. Could be wrong if another ucode image
   (not extracted, not in the VBIOS) sets it, or if the hardware reads the
   publish trigger from a different address space. The PUBSWEEP data is
   conclusive about behavior (`CAP2 = 0x6` ceiling), but not about provenance.

2. **"OPT bank is master-gated, not privilege-gated."** Evidence: app08 writes
   `0x820520`, Booter writing the same address is refused. Could be wrong if
   there's a timing window — app08 runs during early init, Booter runs after
   driver load. A closed window, not a master check. But `EN_SW_OVERRIDE` was
   set before the Booter ran, and the OPT bank still refused, so a simple
   "override enable → write" model doesn't fit.

3. **"The Booter and app08 have different master identities."** Evidence: same
   privilege (L3), same architecture, one write lands and one doesn't. Could be
   wrong if the difference is in DMEM state, WPR configuration, or some other
   context that we can replicate rather than redirect.

4. **"The PHY calibration tables are zeroed by fuses, not by firmware logic."**
   Evidence: A100 has values, 170HX has zero, host writes refused. Could be
   wrong if app08's code path skips populating them on the 170HX (conditional
   on `OPT_GEN23`/`OPT_GEN3`), and a modified app08 could populate them.

5. **The `0x823824` feature-override shadow has the same master-gating as the
   OPT bank.** Evidence: it's in the same `0x823800` block. Could be wrong if
   it's a different class of register than `OPT_*`.

6. **"Gen3 requires equalization that Gen2 doesn't, and we can't measure it."**
   Evidence: SMBPBI is the only instrument, untested. Could be wrong if the
   link can train Gen3 without explicit EQ (some topologies auto-EQ).

# 18 — Falcon addresses are PRI addresses, and what that opened

> On-hardware, AM5 rig, CMP 170HX `10de:20c2`. Supersedes the "out of reach" claims in
> [doc 02](02-pcie-gen-investigation.md), [doc 12](12-gen3-attack-plan.md),
> [doc 13](13-gen3-synthesis.md) and [doc 17](17-app08-phy-asymmetry.md).

## The result

**Falcon external addresses are PRI addresses with the target in the top byte:**

```
falcon_address = 0x14000000 | pri_address
```

`0x14118f78` is **PRI `0x118f78`** — inside the 16 MB BAR0 aperture, host-readable and
host-writable. Every doc in this repo that called it a "reset-latched strap beyond the aperture,
out of reach" was reading a Falcon address as a BAR0 offset.

### Evidence

Eight registers `app08` references, matching this driver's own constants after stripping
`0x14000000` (excluding the ones later added to `0007`, which would be circular):

| falcon | PRI | what the driver calls it |
|---|---|---|
| `0x14088088` | `0x88088` | `PCIE_GEN2_LINK_CTRL_STATUS_ADDR` |
| `0x1408841c` | `0x8841c` | `PRIV_MISC_1` |
| `0x14088610` | `0x88610` | `VSEC_HIERARCHY` |
| `0x1408c1c0` | `0x8c1c0` | `PL_LINK_RATE` |
| `0x1408c2c0` | `0x8c2c0` | CYA_0 — the `DIS_G2` register |
| `0x1408c300` | `0x8c300` | CYA_1 |
| `0x14820520` | `0x820520` | `OPT_MAGIC` |
| `0x149a0204` | `0x9a0204` | FBPA (`0001` opens the PLM at `0x9a0148`) |

Corroboration that doesn't depend on our own constants: **`0x118xxx` is a published PRI block** —
`NV_PGC6_AON_SECURE_SCRATCH_GROUP_*` (`0x118128`, `0x1182cc`, `0x1183a4`), `NV_PGC6_BSI_SECURE_SCRATCH_*`
(`0x1180f0`), `NV_PGC6_SCI_*` (`0x118df4`, `0x118f54`). Stripping the prefix lands inside a named
block, not empty space.

Confirmed on silicon: the reads return real values, not `0xbadf1100` or `0xffffffff`.

**Caveats.** The "try a different prefix" control is degenerate — every address-like immediate in
`app08` is `0x14xxxxxx`, so alternatives have zero candidates by construction. `0x137xxx` has no
published name on any chip; that part of the mapping rests on the block-structure argument.

## What the gate reads on this card

`app08` gates its PHY routine on `0x11823c[11:10] == 2 && 0x118f78[30] == 1` ([doc 17](17-app08-phy-asymmetry.md)).
Both are now readable:

```
0x11823c = 0x028a2a2a   [11:10] = 2   condition 1 SATISFIED at stock
0x118f78 = 0x00000000   bit30   = 0   condition 2 fails at stock
0x12e0   = 0x00000020   bit0    = 0   outer branch takes the gate path
```

`0x118f78` properties, measured:

- **Writable at PL0** — plain `GPU_REG_WR32`, no Booter needed (ten sweep values, `rd == want` each).
- **Persists across a warm reboot** with no regkey set. It is in the always-on island, matching
  Pry §6.1. A **cold** power cycle clears it.
- **Has a live side effect** — setting bit30 changed `0x118e80` from `0x00028400` to `0x00000400`.

### The contradiction

With `[11:10]==2`, `bit30==1` persisted into the next boot, and `0x12e0[0]==0`, the routine
**still did not run**:

- `0x118e90` reads `0x00028400`; if `0xcb00` had executed its opening RMW it would read `0x00068c00`.
- Witnesses only `0xcb00` writes — `0x9a0090`, `0x9a0154`, `0x9a103c`, `0x13744c` — are all zero.

Unresolved. This is the one place the static model and the hardware disagree.

## The advertise publish

`0x8872C` is a **publish trigger**, not a data register. Established:

- Writing any nonzero value re-publishes; the written value does **not** select the vector.
  `0x6`, `0xa` (Gen1+Gen3, no Gen2 bit) and `0xe` all publish `CAP2 = 0x6`.
- It re-triggers repeatedly within one boot — which makes in-boot sweeps possible.
- It publishes `0x6` **even with `DIS_G2` still set**. The Gen2 advertise does not depend on `DIS_G2`.

### Publish-path diff

Snapshot 1024 dwords, poke the trigger, snapshot again. Base `0x88000` (PCFG):

```
0x88084 CAP    0x00456101 -> 0x00456102   output
0x880a4 CAP2   0x00000002 -> 0x00000006   output
0x8860c VSEC   0x00000800 -> 0x00000801   side effect
0x8872c        0x00000000 -> 0x00000006   the poke
```

Base `0x8c000` (XP): **zero changes.**

Two things follow. Nothing in PCFG or XP is an intermediate in the publish path. And **that
`0x8860c` line resolves doc 09's mystery** — the beta logged `VSEC_DEVICE booter FAILED` yet later
read `0x801`. It was never the write; the publish sets that bit, and
`NV_PCFG_XVE_REGISTER_WR_MAP` marks `0x60c` read-only.

**Method limitation, stated so it isn't repeated:** a diff finds *outputs*, not *inputs*. The publish
reads its source and writes `CAP`/`CAP2`; a source register never changes, so no diff can surface it.
Running the diff on further blocks is pointless.

## The fuse block, closed

`0x118f78` link-config fields — `FWSEC_COMPARISON.md` records devinit RMW-ing it with masks
`0x3000`/`0x2000`/`0xc000`/`0x8000`. Swept `0x1000 0x2000 0x3000 0x4000 0x8000 0xc000 0xf000
0x40000000 0x4000f000 0xffff`. All written successfully, all published `CAP2 = 0x6`.

`0x820000`–`0x8207ff` dumped:

- **`0x8200d0`–`0x8200fc`: twelve PLMs, all `0xffffffff`, fully open.** OPT writes still rejected.
  **The OPT write block is not PLM-gated** — there is no missing gate of that kind.
- Control: `NV_FUSE_OPT_NVDEC_DISABLE` (`0x820378`), published `RW-4R` with `DATA 4:0 RWIVF`,
  rejects a write with `EN_SW_OVERRIDE` set. The block is globally RO, independent of which fuse.
- `EN_SW_OVERRIDE` (`0x820040`) is settable via the Booter and reads back `1`, but does not open OPT
  writes. Reads `0` again after a cold cycle.
- No fuse controller. `0x820068`/`0x82006c` hold `0x00824344`/`0x008243fc`, which look like pointers,
  but both targets read zero and `0x824000`–`0x8240ff` is entirely `badf5040`.

Note the Booter primitive already writes at HS/L3 ([doc 13](13-gen3-synthesis.md)), so HS privilege
is **not** what the OPT block is withholding.

### Fuse map notes

| addr | value | reading |
|---|---|---|
| `0x8204d8`, `0x82056c` | `0x000020c2` | PCI device ID as fuse shadows — the SKU identity bits |
| `0x82057c` | `1` | `OPT_GEN23` |
| `0x820580` | `1` | `OPT_GEN3` |
| `0x820584` | `1` | third gen-adjacent fuse, never investigated |
| `0x8207d4`–`0x8207ec` | `0x5` ×8 | consecutive group, unidentified |
| `0x8205d0`,`0x8205d4`,`0x8205e8` | `0x00c03000` | repeated value |

## Probe harness

All in `0007`, regkey-gated, default off:

| key | effect |
|---|---|
| `CmpFeatDump=1` `CmpFeatBase=` | dump `0x400` bytes, skip all-zero rows |
| `CmpFeatAddr=` `CmpFeatVal=` | one Booter write to any address, readback + `CAP`/`CAP2` |
| `CmpSweep=1` | sweep values into `0x118f78`, re-publish and read `CAP2` per value |
| `CmpDiff=1` `CmpDiffBase=` | snapshot 1024 dwords, publish, report every changed dword |
| `CmpTryGen3`, `CmpGen3Early`, `CmpXveCmd`, `CmpGen3Cya*`, `CmpGen3Misc1*`, `CmpPlm*` | earlier ladders, [doc 16](16-gen2-cap-reversion-fix.md) |

Because the publish re-triggers in-boot, a sweep of N values costs one boot rather than N.

## Corrections issued

| doc | claim | status |
|---|---|---|
| 02 | `0x14118f78` is a PHY strap outside the aperture | **wrong** — PRI `0x118f78`, host R/W |
| 12, 13 | strap `0x14118f78` ">16 MB, reset-latched, unreachable" | **wrong** — same |
| 17 | the routine's registers are "Falcon-only" | **wrong** — all are PRI, all reachable |
| 09 | `VSEC_DEVICE` write "FAILED" but later read `0x801` | explained — the publish sets it; `0x60c` is RO |
| 13 | `LnkCap2` is a live reflection of the XP clamp | already corrected in [doc 16](16-gen2-cap-reversion-fix.md); reconfirmed here — the publish ignores `DIS_G2` |

## Open

1. **The gate contradiction** — every readable precondition satisfied (all three conditionals on the
   98-step path pass), routine still doesn't run.
2. Whether `app08` re-executes on a warm reboot at all. `0x118e80`/`0x118e90` swapped values between
   two consecutive boots, which suggests something reprograms them, but it is not proof.
3. `0x820584`, the `0x8207d4`–`0x8207ec` group, and the `0x00c03000` triple are unidentified.
4. `0x137xxx` naming — the only part of the address mapping without independent corroboration.
5. The single runtime byte change `0xd9 → 0x99` in the `0x132a0x` source record — what clears bit 6.

## Addendum — the `0x132a0x` lane-map thread, closed

Chased on the theory that a SKU-specific data table fed the PHY registers. It does not. Recorded in
full because the method errors are more reusable than the result.

### The packer

`app08` IMEM `0x24e1`–`0x25ee` packs bytes from a DMEM structure into PRI registers:

```
0x132a00 = (old & ~2) | (S[0x36] << 28)
0x132a04 = S[0x37] | S[0x38]<<8  | S[0x39]<<16 | S[0x3a]<<24
0x132a08 = S[0x3b] | S[0x3c]<<8  | S[0x3d]<<16 | S[0x3e]<<24
0x132a0c = S[0x3f] | S[0x40]<<8  | S[0x41]<<16 | S[0x42]<<24
0x132a10 = S[0x43] | S[0x44]<<8  | S[0x45]<<16 | S[0x46]<<24
0x132a14 = S[0x47] | S[0x48]<<8  | S[0x49]<<16 | S[0x4a]<<24
```

`S` is a **structure base in `$r0`** (`mov $r0 0x269` at IMEM `0x23e6`), not absolute DMEM. These are
field offsets. All six destinations are host-readable and host-writable PRI under the mapping above.

Live values on this card:

```
0x132a00=100d0001 0x132a04=9901001b 0x132a08=030a0c0d
0x132a0c=10000000 0x132a10=0000000c 0x132a14=0f25000a
```

Inverting gives the runtime record `1b 00 01 99 0d 0c 0a 03 00 00 00 10 0c 00 00 00`.

### Why it is not the crippling

The same record appears in **all three** ROMs — 170HX `20c2`, A100 `20b0`, `20bb` — byte-identical
except that the ROM images carry `d9` where the runtime holds `99` (bit 6 cleared during execution).
It is not SKU-specific.

### Two method errors, both worth not repeating

**Offset-matching shifted images.** The three DMEM images differ in size (`0x2ae0` / `0x2920` /
`0x2af0`) and their content is byte-shifted relative to each other, so a fixed-offset comparison is
meaningless. A naive dword diff of 170HX vs `20bb` reports 1323 of 2744 dwords differing, nearly all
alignment artifact. An apparent SKU difference at image offset `+0x40` (`1f`-filled on the 170HX,
`00..08` on both full parts) looked offset-stable and was not — **content-match these images, never
offset-match them.**

**Reading `D[$rX+disp]` as an absolute address.** `D[$r0+0x40]` is a struct field, not `DMEM[0x40]`.
Conflating the two is what connected the `+0x40` image difference to this packer in the first place;
they are unrelated addresses that shared a number.

### Path to the gate

For completeness, the CFG walk from `app08`'s entry to the guarded call at IMEM `0x15a4` is 98 steps
with exactly three conditionals:

| IMEM | test | on this card |
|---|---|---|
| `0xd016` | `bra b 0xd00f` | a memory-clear loop, not a gate |
| `0x1560` | `DMEM[0x34] & 3 != 0 → skip` | passes — `0x17fc`/`0x16d0`/`0x1810`, aligned in all three builds |
| `0x1591` | `0x12e0 & 1 != 0 → skip` | passes — measured `0x12e0 = 0x00000020`, bit0 = 0 |

Nothing on the path explains the routine not executing with the gate satisfied. Still open.

### Net

Combined with the three-way code control in [doc 17](17-app08-phy-asymmetry.md), both the code and
this data structure are common across a crippled and two uncrippled parts. The firmware comparison
is exhausted; the SKU difference is in fuse values, and the fuse block is globally read-only with all
twelve OPT PLMs open and HS privilege already available.

What would supply new information: **live registers from an uncrippled GA100.** Every "is this the
crippling?" question in this thread would have been one query against a working part. Also cheap and
unfinished: `0x820584` and the `0x8207d4`–`0x8207ec` group, the last unidentified fuse-map entries.

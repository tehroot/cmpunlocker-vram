# 15 — SMBPBI msgbox: the measurement instrument the Gen3 work is missing

> Found while verifying a citation in [doc 13](13-gen3-synthesis.md). The Gen3 problem right now is a
> **measurement** problem, not a control problem — Phase 1 exists solely to learn whether the LTSSM ever
> attempts 8 GT/s, and the one wedge we have measured nothing (it booted `pci=noaer`). SMBPBI is a
> read-only telemetry service that answers exactly that, plus per-lane Gen3 EQ coefficients.
> **Status: reachability untested.** The whole thing gates on one read-only dword: `0x660e0`.

## Why doc 13 filed this wrong
Doc 13 §"EQ reality" lists `Coeff`/`FS`/`LF`/`USE_PRESET` as "RO SMBPBI telemetry" — under **dead ends**.
Factually right, strategically backwards. We do not need to *write* EQ presets to answer Phase 1; we need to
*read* what the link did. Read-only is the correct instrument for the question actually blocking us.

## The service
`NV_MSGBOX_CMD_OPCODE_GET_PCIE_LINK_INFO = 0x21` (`smbpbi.h:94`), pages selected by ARG1 (`:189-196`):

| Page | Returns | `smbpbi.h` |
|---|---|---|
| 0 | `LINK_SPEED[2:0]` (1=2.5, 2=5.0, **3=8.0**, 4=16.0), `LINK_WIDTH[6:4]`; EXT_DATA `CORRECTABLE_ERROR_COUNT[15:0]` | `:1288`, `:1295`, `:1380` |
| 1 | replay count (EXT_DATA `[31:0]`) | `:1308` |
| 2 / 4 | NAKs sent / RX count | — |
| **6** | **`LTSSM_STATE[4:0]`** — 19 states, `DETECT`=0x0 … `RECOVERY`=0x3, **`RECOVERY_EQZN`=0x4**, `L0`=0x5 … `ILLEGAL`=0x1f | `:1328-1348` |
| **8** | **TX local EQ**: `PRESET[3:0]`, `USE_PRESET[4]`, `FS[10:5]`, `LF[16:11]`; EXT_DATA `PRECUR[5:0]`, `MAINCUR[11:6]`, `POSTCUR[17:12]` | `:1351-1354`, `:1392-1394` |
| **9** | **RX remote EQ**: same fields, link-partner side | `:1357-1360`, `:1397-1399` |

Pages 8/9 take `ARG2 = LANE_IDX[3:0] | SPEED_SELECT[5:4]`, and `SPEED_SELECT` enumerates
**`GEN_3 = 0x0`**, `GEN_4 = 0x1`, `GEN_5 = 0x2` (`:472-479`, `:486-493`). So the card can be asked, per lane
and per direction: *what Gen3 EQ coefficients did this link negotiate?*

## Command ABI
`CMD = OPCODE[7:0] | ARG1[15:8] | ARG2[23:16] | STATUS[28:24] | RSVD[29] | INTR[31]`
(`:54`, `:102`, `:397`, `:530`; constructor `NV_MSGBOX_CMD()` at `:2326`). Protocol: write `CMD` with
`INTR=1`, poll until `STATUS == SUCCESS (0x1f)`, read the data register. Status codes `:530-549` —
`ERR_OPCODE` 0x2, `ERR_ARG1` 0x3, `ERR_ARG2` 0x4, `ERR_NOT_SUPPORTED` 0x8, `ERR_BUSY` 0xa, `SUCCESS` 0x1f.

| Want | CMD word |
|---|---|
| `GET_CAP_DWORD` cap 2 → bit14 = `GET_PCIE_LINK_INFO` available (`:871`) | `0x80000201` |
| PAGE_0 link speed + width | `0x80000021` |
| **PAGE_6 `LTSSM_STATE`** | `0x80000621` |
| **PAGE_8 TX EQ, lane 0, Gen3** | `0x80000821` |
| PAGE_8 TX EQ, lane 3, Gen3 | `0x80030821` |
| **PAGE_9 RX EQ, lane 0, Gen3** | `0x80000921` |
| `NULL_CMD` (servicing no-op) | `0x80000000` |

The CAP query is the cheap gate: one transaction says whether this SKU's firmware implements the service
before anything else is attempted.

## Transport — the pivotal unknown
SMBPBI is an *out-of-band* interface: an external SMBus master writes a mailbox register and an on-die
microcontroller (PMU on GA100; SOE on NVSwitch) services it. **There is no in-band client in the open tree** —
`grep -rn "NV_MSGBOX_CMD" --include=*.c src/` returns nothing, and the header lives under
`arch/nvalloc/common/inc/oob/`.

But the mailbox is a **BAR0 register**:

```
NV_THERM_MSGBOX_COMMAND   0x000660e0  /* RW-4R */   nvswitch/lr10/dev_therm.h:45-49
  _DATA  30:0     _INTR  31:31   (INTR_PENDING = the doorbell)
```

`0x660e0` is at 64 KB — deep inside the 16 MB aperture, reachable by **both** driver-context `GPU_REG_RD32`
and the SEC2-postbl ROP. The open question is whether the servicing task cares *who* rang the doorbell. If it
polls/interrupts on `0x660e0` regardless of origin, the whole instrument works in-band with no SMBus wiring.

**Caveats, stated plainly:**
1. `0x660e0` is from **lr10 (NVSwitch)**, not GA100 — Ampere's `dev_therm.h` is curated out of the published
   headers, same as `dev_fuse.h`'s OPT offsets. This is the identical cross-SKU Rosetta-Stone inference
   [doc 10](10-gen2-breakthrough-hypothesis.md) used for the CYA registers and validated on-card — but it is
   an inference until read.
2. lr10 publishes **only** `MSGBOX_COMMAND`. `DATA_IN`/`DATA_OUT`/`EXT_DATA`/`MUTEX` offsets are not in any
   header here. Adjacent dwords (`0x660e4`, `0x660e8`, `0x660ec`, `0x660f0`) are the obvious candidates —
   same bisection as `DIS_G2`.
3. A non-poisoned read proves **reachability**, not **servicing**. Servicing needs a write + poll (Phase 0.5).
4. If the 170HX VBIOS strips the PMU SMBPBI task, this is moot — `GET_CAP_DWORD` says so in one transaction.

## Staged test
**Phase 0 (read-only, in the `gen3-phase01-probe` patch).** `GPU_REG_RD32` of `0x660e0` + the four adjacent
dwords, printed on the `GEN3_P0` line.
- `0x660e0` returns a plausible value → mailbox is host-reachable → Phase 0.5.
- `0xBADF1100` → priv-poisoned from driver context, same class as `0x85080`. Falls back to SMBus (slot pins
  B5/B6, or a board header). Note this is *also* a useful datapoint for doc 13's pivot: it would show the
  poison wall is broader than the BIF/XVE block.

**Phase 0.5 (one write, no link risk).** `NULL_CMD` (`0x80000000`), poll `STATUS`. `SUCCESS` ⇒ serviced
in-band. Then `GET_CAP_DWORD` (`0x80000201`) and check bit 14. Writing a mailbox command register is not a
link or config change — it cannot wedge the link — but it is a write, so it is staged separately from the
read-only Phase 0 per doc 13's discipline.

**Phase 1 use.** With the instrument live, the advertise test and any train attempt gain real oracles:
`PAGE_6` LTSSM state (did it enter `RECOVERY_EQZN`?), `PAGE_0` speed, `PAGE_8/9` Gen3 EQ coefficients per
lane. Combined with AER (boot **without** `pci=noaer`), the next wedge becomes a measurement instead of an
anecdote — which is exactly what doc 13 §"The wedge" says is missing.

## Adjacent opcodes — probe targets, not levers
- **`REGISTER_ACCESS = 0x11`** (`:73`), `ARG1` WRITE=0 / READ=1 (`:142-143`). Looks like a poison-wall bypass;
  probably isn't — `NV_MSGBOX_CMD_REGISTER_READ(addr)` puts `addr` in **`ARG2`, which is 8 bits**
  (`:2351-2356`). That is a selector into a firmware-side whitelist, not an arbitrary 32-bit address, and the
  index table is not in the open header. Worth a 256-index sweep once the mailbox is proven — `STATUS`
  distinguishes `ERR_ARG2` from `SUCCESS`, so the sweep is self-describing — but it is reconnaissance.
- **`ACCESS_WP_MODE = 0x17`** (`:79`), `GET`/`SET` via ARG1 `[8:8]` (`:333-335`), state
  `DISABLED = 0x5A` / `ENABLED = 0xA5` in ARG2 `[23:16]` (`:450-452`), capability bit `CAP_1[22]` (`:801`).
  Controls a write-protect mode whose scope is not established here. `GET` first; do not `SET` blind.
- `SCRATCH_READ/WRITE/COPY` (0x0d–0x0f), `ASYNC_REQUEST` (0x10), `GET_NVLINK_INFO` (0x1a) also present.

## Cross-refs
- The wedge this instruments: [doc 13](13-gen3-synthesis.md) §"The wedge" · Gen3 plan: doc 13 §Plan
- Cross-SKU header inference method: [doc 10](10-gen2-breakthrough-hypothesis.md)
- Probe: `driver/patches/0007-pcie-gen2.patch` `GEN3_P0` block on branch `gen3-phase01-probe`
- Source: `driver/.build/open-gpu-kernel-modules-610.43.03/src/nvidia/arch/nvalloc/common/inc/oob/smbpbi.h`,
  `src/common/inc/swref/published/nvswitch/lr10/dev_therm.h`

# 01 — cmpunlocker: the tool and the unlock mechanism

## The card
The CMP 170HX is a physically complete **GA100** die (same silicon as an A100) sold as a crypto-mining
card, with capabilities restricted in firmware/OTP fuses:
- **Compute** throttled (SM/FMA ~1/32; further tensor-core dispatch gating remains even after unlock)
- **Memory** exposed as 8 GB or 10 GB of the physically-present HBM2e (unlockable to 64/40 GB)
- **PCIe** limited to Gen1 x4 (~0.85–1 GB/s); x16 width blocked by missing board caps
- **NVLink** disabled (fuse + missing PCB bridge components + firmware)

## The repo (`cmpunlocker-vram`)
```
install.sh          # entry point — detects card (0x20C2/0x2082), profile, driver, Secure Boot off
remove.sh           # uninstaller (restores stock)
driver/VERSION      # supported nvidia-open: 610.43.03 / 610.43.02
driver/build.sh     # downloads open-gpu-kernel-modules-<ver>, applies patches, builds .ko, installs
driver/patches/     # the six unlock patches (unified diffs against NVIDIA's C source)
common/constants.yaml  # magic values (device ids, SS0/SS1, per-profile CFG1/LMR/fb_bytes)
```
It installs **only patched open kernel modules** (not the full NVIDIA userspace), requires
**Secure Boot OFF** (modules are unsigned), and re-applies the unlock **at every GSP boot**.

## The unlock mechanism — SEC2 Booter signature-payload exploit
Read from `driver/patches/0001-sec2-postbl-plm-ss-cfg.patch` (injected into `kernel_gsp.c`).

### 1. Arbitrary register write via the signed Booter
`kgspSec2PostblTimingRefillPayload(pGpu, pKernelGsp, writeAddr, writeValue)` crafts a fake "signature"
payload in the GSP signature memdesc. When the genuine, NVIDIA-signed **SEC2 Booter** executes it,
a flaw in how it processes the payload makes it perform `MMIO[writeAddr] = writeValue`:
- `writeValue` encoded at payload offset `0xf754`
- `writeAddr` encoded at payload offset `0xf76c`
- executed by `kgspExecuteBooterLoad_HAL(...)`

This is a **confused-deputy / input-validation exploit of signed HS code** — the Booter's own
signature stays valid; it's abused via attacker-controlled *data*. It is *not* a signature forgery
and does *not* modify any firmware. (The community's Falcon-security doc claimed "no runtime exploit
through signed code exists" on Ampere — cmpunlocker is exactly such an exploit, one that doc missed.)

### 2. Open the PLMs
A loop over four Protection Level Mask registers, using the Booter-write to force each to `0xffffffff`:
```
plmTable[] = {
  { 0x001fa7cc, 0xfffff0ff, "WPR_CFG" },
  { 0x009a0148, 0xffffffff, "FBPA"    },
  { 0x001fa7c4, 0xffffffff, "WPR"     },
  { 0x00823804, 0xffffffff, "FEAT"    },   // QUADRO_WR_SEC gate (community)
};
```
(WPR2 lo/hi at `0x001fa824`/`0x001fa828` are saved/restored around this.)

### 3. Write the unlock config (direct host writes, now that PLMs are open)
```
GPU_REG_WR32(0x0082381c, 0x88888888);   // SS0  — SM speed / feature override (compute)
GPU_REG_WR32(0x00823820, 0x00000008);   // SS1
GPU_REG_WR32(0x009a0204, cfg1Value);     // FBPA_CFG1 — memory geometry
GPU_REG_WR32(0x00100ce0, lmrValue);      // MMU_LMR   — memory geometry
```
Per profile (device-id gated):
| card (devid) | unlock | CFG1 (`0x009a0204`) | LMR (`0x00100ce0`) | fb_bytes |
|---|---|---|---|---|
| 8 GB (`0x20C2`) | 64 GB | `0x02779000` | `0x0000020B` | `0x1000000000` |
| 10 GB (`0x2082`) | 40 GB | `0x02669000` | `0x0000028A` | `0x0A00000000` |

### 4. Expose the memory to the OS
Rewrites `GspStaticConfigInfo.fb_length` and extends the last FB region's `limit`
(so GSP-RM presents the enlarged VRAM); `patch 0003` (late-PMA) registers the high FB region into the
PMA allocator so the extra VRAM is usable; then `kgspSec2PostblTimingRebuildStockSignature` restores
the genuine signature so real GSP still boots.

### The other patches
- `0002-booter-verify` — Booter verification defines + non-fatal status handling.
- `0003-late-pma` — expose the unlocked FB region to the PMA allocator (`osinit.c`).
- `0004-bar0-pramin-clamp` — keep the BAR0 PRAMIN window in range after FB enlarges.
- `0005-ce-scrub-workarounds` — copy-engine scrub fixes for the remapped memory.
- `0006-persistent-sw-state` — `NV_FLAG_PERSISTENT_SW_STATE` so state persists.

## Why this reaches compute + memory but not PCIe gen (the key principle)
cmpunlocker's single superpower is **"write any PLM-protected register."** That maps onto:
- **Compute** — a fuse sets the default throttle, but `SS0`/`SS1` (`0x0082381c`/`0x00823820`) are
  *writable feature-override* registers → the exploit overrides them.
- **Memory** — the HBM is *physically present*; geometry is a *writable config register*
  (`CFG1`/`LMR`) → the exploit reconfigures the controller to expose it.
- **PCIe gen** — gated by an **OTP fuse feeding a hardware-set `LnkCap2`** that firmware never writes.
  There is **no register to override** (see doc 02/03). A register-write primitive can't touch it, and
  a fuse can't be rewritten (`FUSE_EN_SW_OVERRIDE = 0` on this SKU).
- **NVLink** — fuse-disabled *and* the physical bridge hardware isn't populated on the board.

So the dividing line across everything is **register/PLM (beatable) vs fuse/physical (not)**.

## Note vs. the public state of the art
The 170th-street community docs list the 170HX **memory unlock as "UNPROVEN"** and the compute
feature-override registers as **"read-only on CMP,"** yet `cmpunlocker-vram` does both — because its
Booter-PLM-open primitive defeats the PLM protection the community treated as a wall. The repo is
**ahead of the publicly documented community state** on compute and memory (but not PCIe/NVLink,
which are the fuse/physical side of the line).

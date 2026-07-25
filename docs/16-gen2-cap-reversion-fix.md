# 16 — LnkCap2 reversion: why Gen2 failed on the AM5 host, and the in-window fix

> **Status: FIXED and measured on-card.** Gen2 trains in **10 ms**. The confirmed doc-10 mechanism was
> correct but was being applied at the wrong *time*: `LnkCap2` reverts `0x6 → 0x2` between the `0007`
> boot block and `nv.c` device init, and after the reversion every write that could restore it is
> rejected. Fix = drive the upstream retrain from inside the `kernel_gsp_tu102.c` late block.
> **Corrects [doc 13](13-gen3-synthesis.md)'s "`LnkCap`/`LnkCap2` = live reflection of the XP clamp".**

## Rig
| | |
|---|---|
| CPU / platform | AMD 9950X, Granite Ridge AM5 |
| OS / kernel | Debian 13 trixie, 6.12.88+deb13 |
| Driver | nvidia-open 610.43.03 |
| GPU | CMP 170HX `10de:20c2` @ `01:00.0`, riser, **x4** width |
| Upstream port | `00:01.3` |

## Problem
The doc-10 sequence (`0x8C2C0[2]` DIS_G2 clear · `0x8C040[19:18]` MAX_RATE=2 · `0x8872C`=6 ·
`TargetLinkSpeed=2` both ends · upstream Retrain-Link) did **not** produce Gen2 here. Both existing
drivers of it failed:
- userspace `tools/retrain.sh`
- in-driver `nv_cmp170hx_retrain_gen2()` in `kernel-open/nvidia/nv.c`, called from device init
  (`driver/patches/0008-pcie-gen2-probe-retrain.patch:18-179`, hooked at `:188`)

## Root cause — the reversion window
`LnkCap2` (BAR0 `0x880A4`) reaches `0x6` inside the `0007` boot block and is back to `0x2` by `nv.c`.

| t | Site | CAP `0x88084` | CAP2 `0x880A4` | LC2 `0x880A8` |
|---|---|---|---|---|
| 25.317 s | `GEN2_LATE` entry/exit (`kernel_gsp_tu102.c`) | `0x00456102` | `0x00000006` | `0x00000002` |
| 26.038 s | `nv.c` device init | `0x00456101` | `0x00000002` | `0x00000001` |

Once reverted, every restore path is rejected:

| Write | Want | Readback | Why |
|---|---|---|---|
| `0x880A8` | `0x000f0002` | `0x00000001` | `0x88xxx` PLM re-locked after the Booter window |
| `0x8C1C0` | `0x00240036` | `0x00340036` | clamped |
| GPU cfg LnkCtl2 | `0x0002` | `0x0001` | cannot target a speed `LnkCap2` does not advertise |

Only `0x8C2C0` and `0x8C040` still accept writes at that point, and **neither moves CAP2** — confirmed
twice: post-boot via `recon/retrain-diag.sh`, and in-driver via the bisect steps at
`0008-pcie-gen2-probe-retrain.patch:91-106`.

Asymmetry worth recording: the **DIS_G2 clear persists** across the window (`CYA0` stays `0x068731b3`),
but **MAX_RATE reverts 2 → 3**.

## KEY INSIGHT — the cap follows the trained rate
**`LnkCap2` is not re-clamped on a timer. It reverted because the link never trained at Gen2 inside the
window.** Train Gen2 and the reversion stops — and the previously-rejected writes start working.

| | before (Gen1) | after (Gen2) |
|---|---|---|
| CAP2 at `nv.c` | `0x00000002` | `0x00000006` |
| GPU LnkCtl2 readback | `0x0001` — rejected | `0x0002` — accepted |
| stepA `0x8C1C0` | rb `0x00340036` — clamped | rb `0x00240036` — took |
| stepB `0x880A8` | rb `0x00000001` — rejected | rb `0x00010002` — partial |

This **corrects doc 13** §"Confirmed Gen2 model": "`LnkCap`/`LnkCap2` = live reflection of the XP clamp
(on-card `CAP2 0x2→0x6` when `DIS_G2`/`MAX_RATE` written)". Measured twice here: **DIS_G2 clear +
MAX_RATE=2 alone provably do NOT move CAP2.** The cap reflects the *trained rate*, not the XP clamp.

## Fix — retrain from inside the window
Drive the upstream retrain in the **late block of `kernel_gsp_tu102.c`** (after `BooterLoad` completes),
not from `nv.c`. `kernel_gsp_tu102.c` is RM core with no `struct pci_dev`, so the upstream bridge is
reached through RM's own OS PCI abstraction:

```c
void *osPciInitHandle(NvU32 domain, NvU8 bus, NvU8 slot, NvU8 function,
                      NvU16 *pVendorID, NvU16 *pDeviceID);   /* g_os_nvoc.h:675 */
osPciReadByte() / osPciReadWord() / osPciWriteWord()
```

Algorithm (`driver/patches/0007-pcie-gen2.patch:369-458`):
1. Scan buses `0 .. gpuGetBus(pGpu)-1`, dev `0..31`, fn `0..7` for a **header type 1**
   (`cfg 0x0E & 0x7F == 1`) function whose **secondary bus number** (`cfg 0x19`) equals `gpuGetBus(pGpu)`.
2. Walk its capability list from `0x34` for cap ID **`0x10`** (PCIe cap).
3. `cap+0x30` LnkCtl2: `TargetLinkSpeed[3:0] = 2`.
4. `cap+0x10` LnkCtl: set **Retrain-Link bit5**.
5. Poll GPU `LinkCtrlStatus` BAR0 **`0x88088` [19:16]** for `>= 2`; 20 × `osDelay(10)`.

## Result
```
GEN2_LATE retrain up=00:01.3 cap=0x58 lc2 0x0041->0x0042 t=10 ms STAT=0x10420040 speed=2
          CAP=0x00456102 CAP2=0x00000006
```
Persists to device init: `LnkSta=0x1042` → **speed=2, width=4**.

## Secondary bug fixed in 0008
The success test required `PCI_EXP_LNKSTA_DLLLA`. This endpoint has **`LnkCap` bit20 `DLLLARC` clear**
(`LnkCap=0x00456101`), so `DLLLA` always reads 0 — the check could never report success even on a
trained link. Now gated on the port actually advertising `DLLLARC`
(`0008-pcie-gen2-probe-retrain.patch:50`, `:161-162`).

## Host prerequisites (found the hard way)
- **`iomem=relaxed` on the kernel cmdline** is required for any **userspace** tool that mmaps
  `/sys/bus/pci/devices/*/resource0`. Debian ships `CONFIG_IO_STRICT_DEVMEM=y`, so
  `pci_mmap_resource()` returns `-EINVAL` via `iomem_is_exclusive()`.
  **Not** needed for the in-driver path, which uses `ioremap()`.
  ⚠ This **falsifies** `recon/gen3-probe.sh:22-23,184-186`, which attributes that `EINVAL` to
  OcuLink ("On the R530/OcuLink this is the known EINVAL"). It is a kernel hardening option, not a
  transport property.
- **Multi-GPU slot-order bugs in the userspace helpers:**
  - `tools/retrain.sh` `find_gpu()` (`:27`) returns the *first* match, and the early-exit at `:15`
    reads `nvidia-smi … | head -1` — i.e. GPU0, which may be a different card.
  - `install.sh:61` profile detection has the same `head -1` issue → **pass `--profile` explicitly.**

## Open / next
- Stability not yet validated under load — AER correctable counters after a stress run.

## Cross-refs
- Confirmed mechanism: [doc 10](10-gen2-breakthrough-hypothesis.md) · corrected claim:
  [doc 13](13-gen3-synthesis.md) §"Confirmed Gen2 model" · Gen3 plan: [doc 12](12-gen3-attack-plan.md)
- Measurement instrument for the Gen3 follow-on: [doc 15](15-smbpbi-msgbox.md)
- Patches: `driver/patches/0007-pcie-gen2.patch` (late-block retrain),
  `driver/patches/0008-pcie-gen2-probe-retrain.patch` (bisect + DLLLARC gate)
- Post-boot diagnostic: `recon/retrain-diag.sh`

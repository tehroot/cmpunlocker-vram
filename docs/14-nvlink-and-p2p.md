# 14 — NVLink + PCIe P2P (the multi-GPU bandwidth path)

> Goal: fast multi-170HX for LLM (tensor/pipeline parallel). NVLink (GA100 native ~600 GB/s) would be ideal;
> PCIe P2P is the reachable enabler. 7-agent investigation. Verdict: **NVLink is closed (traced, not assumed);
> P2P is the path and is now a concrete patch (`0008`, branch `p2p-prototype`).**

## Bandwidth context (why the PCIe generation barely matters for TP)
Per direction (8b/10b Gen1-2, 128b/130b Gen3):
| Link | GB/s/dir | bidir |
|---|---|---|
| Gen2 x4 (OcuLink dock) | 2.0 | 4.0 |
| Gen2 x16 (slot + caps mod) | 8.0 | 16 |
| Gen3 x16 | ~15.75 | ~31.5 |
| **NVLink3 (GA100 native)** | 300 | **600** |

NVLink is ~40× Gen2 x16, ~19× Gen3 x16. **Tensor parallelism (all-reduce every layer) is NVLink-class or bust** — no PCIe gen closes it. So Gen3 is low-EV *for the multi-GPU goal* (even if it worked). The PCIe levers that matter: **P2P** (direct GPU↔GPU DMA), **pipeline parallelism** (activations at layer boundaries, orders of magnitude less traffic than TP — tolerates PCIe), and large batch.

---

## NVLink — CLOSED (five independent walls, traced)

Init IS attempted on GA100 — `PDB_PROP_KNVLINK_ENABLED=TRUE` (`g_kernel_nvlink_nvoc.c:241`), real GV100 HAL, no devid gate. It stops at **discovery**: `discoveredLinks` returns empty from GSP; `knvlinkStateLoad` bails at `kernel_nvlinkstate.c:506`. Why the Gen2 method (chicken-bit + timing) does **not** transfer:

1. **No host-writable enforcement register.** GA100 CPU-side NVLink tree = **0 `GPU_REG_RD/WR32`, 89 `knvlinkExecGspRmRpc`**. Discovery/enable/floorsweep/MINION/train all run in **signed GSP-RM**. Gen2 fell because `DIS_G2` was in the host's own BAR0 aperture; NVLink enforcement isn't host-reachable.
2. **No chicken-bit idiom in the IP.** Reconstructed the redacted GA100 NVLink IP from the nvswitch `lr10`/`ls10` Rosetta Stone (`nvlinkip_discovery.h`, `dev_nvlipt_lnk_ip.h`, `dev_nvldl_ip.h`, `dev_minion_ip.h`): it has PHY-tuning overrides + LTSSM/SLSM force actions, but **no "force-link-present / override-floorsweep"** register. The nearest analog (`ENABLE_NVLIPT 0x08c8`, RW) is seeded from discovery-validity (reflects the fuse) and virt-locked — and is nvswitch, not the GA100 GPU (whose analog is GSP-internal).
3. **Fuse shadow hard-RO.** `FUSE_NVLINK_DIS = 0x7` (all 3 IOCTRL groups; `docs/08:88`), `EN_SW_OVERRIDE = 0`. Same register class as `OPT_GEN23`, which failed the HS-ROP write ×2 on-card (`PCIE_GEN1_LOCK.md:148`). Offset unpinned (curated out of every `dev_fuse.h`).
4. **Live-poll terminal.** You *can* forge links at one CPU-RM DRAM point (`kernel_nvlink.c:1916` inject `discoveredLinks`/`maxNumLinks`, zero `vbiosDisabledLinkMask`) and walk past the bail. It survives one hop, then three real-hardware re-queries evict it: floorsweep `ARE_LINKS_TRAINED` RPC (`kernel_nvlinkcorelibtrain.c:1430`), the `bIsLinkActive` training gate (`:697,:785`), and the `bRxDetected` PHY poll (`nvlink_discovery.c:328-374`). P2P finally needs `numPeerLinks>0` (`kernel_nvlink.c:586`) = a real trained remote. **MINION must train a real PHY and report ACTIVE/SAFE — a live poll, not a cacheable mask.** Even single-card loopback needs that trained PHY the fuse denies.
5. **Physical.** 170HX board `900-11001-0108` is a **distinct mining PCB** (A100 = `699-2G509`). NVLink is in **neither card's VBIOS** (off-die: on-die IOCTRL discovery + fuse + GSP). Bridge fingers **reported depopulated** (`docs/01:90`) — *empirical confirm = visual check of the card's top edge*.

Enumeration reachable-but-useless: `PTOP DEVICE_INFO2 0x22800` (`dev_top.h:28`) is <16 MB (SEC2-reachable) but `R--4A` read-only descriptor — no enable bit.

**Framing (`FWSEC_COMPARISON.md:47`):** 170HX and A100 run **byte-identical firmware — the entire difference is fuse return values** (`FUSE_PCIE_GEN23_DIS`, `FUSE_NVLINK_DIS`, `FUSE_SS_*`, geometry) **+ the stripped PCB.** NVLink is the fuse/physical side of the line, categorically *harder* than PCIe-gen (its enforcement isn't even a register we can reach).

---

## PCIe P2P — THE PATH (patch written: `0008`, branch `p2p-prototype`)

Baremetal GA100 is not virtual → caps decided **CPU-side** in `CliGetSystemP2pCaps` → `p2pGetCapsStatus` (`p2p_caps.c:784`), fully patchable (the `_GSPCLIENT` path is virtual-only). Order: MIG → C2C → NVLink (fails, fused) → **PCIe `_kp2pCapsGetStatusOverPcie` (`p2p_caps.c:392`)** — the only path for two 170HX.

**The real gates (corrects prior recon):**
| Gate | Loc | Blocks us? |
|---|---|---|
| `PDB_PROP_KBIF_P2P_READS/WRITES_DISABLED` | `kernel_bif_tu102.c:99` | **NO** — set only under SR-IOV or the `ForceP2P` regkey; default FALSE on baremetal. Prior recon was wrong to name these. |
| device-ID gate | — | **NO** — none exists in `bus/`,`bif/`,`p2p/`. |
| Chipset reachability (`ChipsetInitialized`, peer-read/write-capable + no common switch) | `p2p_caps.c:446,562` | **YES** — host-dependent. |
| GSP-reported `pcieP2PReadCaps/WriteCaps` | `p2p_caps.c:581`, from `gpu.c:2494` RPC (closed GSP) | **YES** — CPU-side overridable. |

**The patch (`0008-pcie-p2p.patch`, 2 devid-gated hunks, `0x20C2`/`0x2082`):**
- `p2p_caps.c` `_kp2pCapsGetStatusOverPcie` after :616 → force read/write status `NV0000_P2P_CAPS_STATUS_OK`, `status=NV_OK` (upstream of the GSP caps-cache store → overrides chipset check + GSP value at once).
- `gpu.c` `_gpuInitPcieP2PCapability` after :2519 → force `pGpu->pcieP2PReadCaps/WriteCaps = OK` (defense-in-depth).
- Effect: connectivity resolves `PCIE_PROPRIETARY` → `cudaDeviceCanAccessPeer` = 1. Verified `0001–0008` apply-clean + C-sanity; not yet compiled/run (needs build rig + 2 cards).

**BAR1 = 64 MB is fine — no resize needed or possible.** GA100 PCIe P2P uses the **mailbox** path: a moving window of `64 KB × 8 peers = 512 KB` (`g_kern_bus_nvoc.h:169,173`), not full-FB mapping. Fits in 64 MB trivially. The **large-BAR / BAR1-direct** path (geohot's other half) is **doubly unavailable**: code stubbed to GH100+ for Ampere (`kbusIsPcieBar1P2PMappingSupported_d69453` → FALSE), and hardware ReBAR advertises **64 MB only** (`recon-...203358.txt:57`). Not needed — mailbox is the right GA100 mechanism.

**Validation (do in order; step 1 needs no rebuild):**
1. **`ForceP2P=0x11` regkey**, stock driver, two cards: `nvidia.NVreg_RegistryDwords="ForceP2P=0x11"` → `nvidia-smi topo -p2p rw` (`OK` between GPUs) + CUDA `p2pBandwidthLatencyTest` moving data card-to-card. This exercises the exact CPU-side path the patch makes permanent → if it works, the patch is **guaranteed sufficient.**
2. **Patch:** `git checkout p2p-prototype && sudo ./install.sh` (applies `0001–0008`), reload, re-run the two tests — no regkey.

**Residual unknown:** whether GSP honors the mailbox-setup RPCs (`INTERNAL_HSHUB_PEER_CONN_CONFIG`, `BUS_SETUP_P2P_MAILBOX_*`, driven from `kbusCreateP2PMappingForMailbox_GM200`) for `0x20c2` once caps report OK. Both hunks are CPU-side; the mailbox programming runs through GSP. Plausibly not devid-gated (generic silicon config). **The `ForceP2P` test settles it before any rebuild.**

---

## Net
- **NVLink: out** — RO-shadow fuse + GSP-mediated enforcement (no host register) + live-poll training terminal + depopulated bridge. Not softwareable.
- **P2P: viable, patched** — one small CPU-side devid-gated patch, 64 MB BAR1 sufficient via mailbox, no BAR surgery. `p2p-prototype` + the no-build `ForceP2P` proof, waiting on card #2.
- For TP specifically: even with P2P, PCIe is NVLink-starved → **pipeline parallel + P2P + batch** is the realistic multi-170HX strategy, not tensor parallel.

## Cross-refs
- Fuse/override architecture: [doc 07](07-fuse-override-and-static-recon.md), [doc 08](08-vbios-mac-fuse-map-external.md) · unlock mechanism: [doc 01](01-cmpunlocker-and-unlock-mechanism.md)
- Gen2/Gen3 (the other PCIe levers): [doc 10](10-gen2-breakthrough-hypothesis.md), [doc 13](13-gen3-synthesis.md)
- Patch: `driver/patches/0008-pcie-p2p.patch` on branch `p2p-prototype`
- Firmware diff: `driver/.build/tools/FWSEC_COMPARISON.md`

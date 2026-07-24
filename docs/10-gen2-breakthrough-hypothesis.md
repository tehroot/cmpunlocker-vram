# 10 — The Gen2 breakthrough: confirmed mechanism + how it was found

> **Status: CONFIRMED and reproduced.** The creator's PCIe **Gen2** unlock is real. This doc records the
> minimal working sequence (from the shipping `tools/retrain.sh`), why each piece is load-bearing, and —
> reconstructed from NVIDIA's own published headers + the RM — **how the chicken bits were discovered.**
> This supersedes the earlier HYPOTHESIS framing; the inference in the first draft (CYA override + a
> correctly-timed post-boot retrain, not a fuse attack) was correct. It also overturns the field manual's
> "double-locked wall" conclusion ([`recon/PCIE_GEN1_LOCK.md`](../recon/PCIE_GEN1_LOCK.md)) for the Gen2 case:
> the CYA/RM-sequence path the manual dismissed as "writable but inert" was inert only because the retrain
> was never driven from the correct end at the correct time.

## The confirmed minimal sequence
Source: [`cmpunlocker/tools/retrain.sh`](../../cmpunlocker/tools/retrain.sh) (+ `cmpretrain.service`, a systemd
oneshot that fires it late, after multi-user). It runs entirely **post-boot from userspace** — no HS/Booter
write is needed for the Gen2 flip itself. Over BAR0 (`resource0` mmap) plus config space (`setpci`):

```python
# 1. XP chicken bits (BAR0 MMIO)
w(0x8C2C0, r(0x8C2C0) & ~(1 << 2))          # clear DIS_G2  (XP CYA: Gen2-disable chicken bit)
w(0x8C040, (r(0x8C040) & ~0xC0000) | (2<<18))  # MAX_RATE [19:18] <- 2   (LINK_CONFIG_0)
w(0x8872C, 0x6)                              # LTSSM nudge

# 2. Target Gen2 on BOTH ends (config space, PCIe cap)
for br in (upstream_port, gpu):
    ctl2 = pci_read(br, cap+0x30, 2)         # Link Control 2
    pci_write(br, cap+0x30, 2, (ctl2 & ~0xF) | 0x2)   # TargetLinkSpeed [3:0] <- 2 (Gen2)

# 3. Retrain — driven from the UPSTREAM port, not the endpoint
ctl = pci_read(upstream_port, cap+0x10, 2)   # Link Control
pci_write(upstream_port, cap+0x10, 2, ctl | 0x20)     # Retrain-Link bit5

# 4. Verify
sta = pci_read(gpu, cap+0x12, 2)             # Link Status; CurrentLinkSpeed [3:0] == 2  → 5.0 GT/s
```

**Timing matters:** the service waits (`sleep`/nvidia-smi-ready poll) so the write lands *after* the RM's
first link derivation, not during boot — otherwise the RM re-clamps it.

## Why each piece is load-bearing
| Step | Register | Why it's necessary |
|---|---|---|
| Clear `DIS_G2` | `0x8C2C0` bit2 | XP-block CYA "disable Gen2" chicken bit; while set, the LTSSM won't advertise/accept Gen2 regardless of target |
| `MAX_RATE = 2` | `0x8C040` [19:18] | LINK_CONFIG_0 ceiling the PHY/LTSSM trains to; the field is 2 bits (values 0–3), so it physically encodes higher than Gen2 |
| `LTSSM = 6` | `0x8872C` | nudges the link-state machine so the new config is picked up |
| Target on **both** ends | LnkCtl2 `cap+0x30` [3:0] | a link trains to `min(both ends' TargetLinkSpeed)`; setting only the GPU is insufficient |
| Retrain from **upstream** | LnkCtl `cap+0x10` bit5 | **the conceptual crack** — an endpoint cannot retrain itself; the link partner (root/switch downstream port) must re-initiate. This is why the beta "advertised Gen2 but trained Gen1": everything was set except *who pulls the trigger* |

The fuse (`OPT_GEN23 @ 0x82057c`) and the reset-latched strap (`0x14118f78`) are **never touched.** The whole
win is bypassing the *enforcement* (CYA/LTSSM) rather than defeating the *source* (fuse/strap) — which is why
the field manual's fuse/strap closure is airtight yet Gen2 still fell.

## How the chicken bits were found (reconstructed methodology)
The names are **semantic** (`DIS_G2`, `MAX_RATE`), not positional ("bit 2 of 0x8C2C0"). That alone proves a
*named-register source*, not blind poking. Four legs:

### Leg 1 — Adjacent chips' headers as a Rosetta Stone
NVIDIA ships hardware-ref headers in the open kernel modules. Who gets a `dev_nv_xp.h` (the `0x8Cxxx` XP block
that holds the CYA/gen chicken bits):

| Chip | XP header |
|---|---|
| maxwell/gm107, gm200 | `dev_nv_xp.h` ✅ |
| pascal/gp102 | `dev_nv_xp.h` ✅ |
| turing/tu102 | — ❌ |
| **ampere/ga100** | **— ❌ (your card)** |
| hopper/gh100 | `dev_nv_xpl.h` (renamed block) |

NVIDIA **redacted the XP block exactly at Turing/Ampere** — the datacenter parts, right where the gen lock
lives — but left Maxwell/Pascal intact, and the block barely changes across generations. Pascal literally
hands you the CYA neighborhood and the chicken-bit idiom:
```c
#define NV_XP_PL_CYA_1(i)                        (0x0008C300+(i)*4)  /* RW-4A */
#define NV_XP_PL_CYA_1_BLOCK_HOST2XP_HOLD_LTSSM   4:4                /* RWIVF */
```
`0x8C2C0` sits just below that `0x8C300` CYA array. Overlay `gp102/dev_nv_xp.h` onto GA100's MMIO and most of
the `0x8Cxxx` space is named for free — from the chips NVIDIA *didn't* redact.

### Leg 2 — GSP-RM disassembly for the GA100-specific bit positions
The published Pascal header is partial (names the CYA array, not the rate fields). The GA100 specifics —
`DIS_G2 = 0x8C2C0[2]`, `MAX_RATE = 0x8C040[19:18]`, the `PRIV_MISC_1` gen override bits — are unpublished for
every chip. Those come from disassembling the RM link-speed routine, which reads/writes these exact offsets
with real masks; matching the masks against the Pascal field layout recovers semantics. Our notes already cite
the addresses (`RMPcieLinkSpeed 0x4DFF358`, `fn 0x4DFC854`) — proof this leg was walked.

### Leg 3 — NVIDIA left the *intent* in the open source (breadcrumbs)
They stripped the register map but not the machinery that drives it. In the shipped source:
```c
NvBool PDB_PROP_CL_PCIE_FORCE_GEN2_ENABLE;                       // g_chipset_nvoc.h
#define NV_REG_STR_RM_PCIE_LINK_SPEED_ALLOW_GEN2_ENABLE  0x1     // nvrm_registry.h
NV_STATUS clPcieGetRootGenSpeed_IMPL(...);  (*setPcieLinkSpeed)(...);  // HAL vtable
```
Grep `GEN2` → find the property → find the HAL that consumes it → chase it into the GSP binary to the actual
registers. The open source tells you *what to look for* even though the offsets are gone.

### Leg 4 — On-hardware bisection (7 layers → 3 registers)
The beta patch was a kitchen sink (PLM opens, fuse write, PHY-rate force, `PRIV_MISC_1` CYA, VSEC gate,
LnkCtl2, LTSSM, late re-apply). The shipping `retrain.sh` is **three MMIO writes + an upstream retrain.** That
collapse is the fingerprint of on-card bisection: flip one candidate, retrain, read `LnkSta`, keep only what
moves the negotiated speed.

### The conceptual crack (not a register)
Realizing the **retrain must be driven from the upstream port** — an endpoint advertising Gen2 does nothing
until the link partner re-initiates training. That's a PCIe-spec insight and is probably what actually landed
it once the chicken bits were already known.

**Net weighting:** ~70% reading NVIDIA's redaction gaps (Pascal header + open-source Gen2 scaffolding),
~20% RM disasm for GA100 bit positions, ~10% on-card bisection — then the upstream-retrain realization.

## Register reference (confirmed roles)
| Addr / name | Role | Confirmed status |
|---|---|---|
| `0x8C2C0` bit2 — DIS_G2 (XP CYA) | Gen2-disable chicken bit | **clear it** — load-bearing |
| `0x8C040` [19:18] — MAX_RATE (LINK_CONFIG_0) | rate ceiling; 2-bit field (0–3) | **set 2** for Gen2 |
| `0x8872C` — LTSSM | link-state nudge | write 6 |
| LnkCtl2 `cap+0x30` [3:0] — TargetLinkSpeed | per-port target; both ends | set 2, **both ends** |
| LnkCtl `cap+0x10` bit5 — Retrain-Link | re-initiate training | **from upstream port** |
| LnkSta `cap+0x12` [3:0] — CurrentLinkSpeed | result | reads 2 (=5.0 GT/s) on success |
| `PRIV_MISC_1 0x8841c` bits 11–16, 30/31 | CYA **GEN2/3** override EN/VAL pairs | present but *not* in the minimal path — the XP-block route won instead |
| `0x85080 [23:20]` / `0x85084 [3:0]` | supported source (poison-walled) / re-derived allowed mask | RM re-clamps `0x85084` from `0x85080` each retrain (the timing constraint) |
| `OPT_GEN23 0x82057c` / strap `0x14118f78` | gen fuse / reset-latched strap | **never touched** by the working path |

## What this means for Gen3 (the open frontier)
Several of the levers are 2+ bits wide and the fuse is named `GEN2**3**`:
- `MAX_RATE` (`0x8C040[19:18]`) is a 2-bit field — value **3** is expressible, and the PHY is the same GA100
  SerDes that does **Gen4** on an A100 (same die), so it is physically capable of Gen3 (8 GT/s).
- `PRIV_MISC_1` bits 11–16 are documented (field manual) as **GEN2/3** override EN/VAL pairs — a likely
  `DIS_G3`-equivalent / Gen3 EN/VAL lives here or as another XP CYA bit.
- The unknowns Gen3 adds over Gen2: **link equalization** (Gen3 requires the EQ phases + presets/coefficients
  that Gen1→Gen2 does not) and whether a **Gen3 supported-cap override** exists analogous to `DIS_G2`.

Gen3 theorycraft is being pursued separately (see doc 12 when written).

## Does it port to the R530 / OcuLink?
The mechanism is post-boot userspace, so it *should* port — but OcuLink re-enumeration/hot-plug timing may
shift the "after first derivation" window, and the retrain must find the correct **upstream port** (the
OcuLink switch/host bridge, which `retrain.sh` derives via sysfs parent). Even a Gen2 win on OcuLink x4 is only
~2 GB/s; the bandwidth payoff is on a direct x16 slot. Confirm the *mechanism* on the main rig.

## Cross-references
- On-hardware beta result (advertised Gen2 / trained Gen1): [doc 09](09-onhw-pcie-gen-beta-result.md)
- The "double-locked wall" field manual (now partially overturned for Gen2): [`recon/PCIE_GEN1_LOCK.md`](../recon/PCIE_GEN1_LOCK.md)
- Fuse-override architecture / `EN_SW_OVERRIDE`: [doc 07](07-fuse-override-and-static-recon.md)
- Avenue survey: [doc 06](06-pcie-gen-attack-avenues.md)
- Shipping tooling: `cmpunlocker/tools/retrain.sh`, `cmpunlocker/tools/cmpretrain.service`, `driver/patches/0007-pcie-gen2.patch`

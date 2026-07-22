# recon/ — pre-hardware reconnaissance for the CMP 170HX

> **`probe-170hx.sh` modifies nothing.** It only reads: PCIe config space (`lspci` / `setpci`
> *reads*, never `=`), sysfs/procfs, `nvidia-smi` queries, and `dmesg`. No register writes, no BIOS
> or PCIe changes, no driver load/unload, and it never runs the cmpunlocker unlock. Safe on a stock,
> unpatched card.

## Why this exists
To gather reconnaissance from a **stock 170HX** *before* you have your own hardware, so the first-pass
experiment plan ([`docs/05`](../docs/05-open-questions-and-hardware-tests.md) /
[`06`](../docs/06-pcie-gen-attack-avenues.md) / [`07`](../docs/07-fuse-override-and-static-recon.md))
can be refined against real values instead of static inference. Hand the script to any community member
with a 170HX; they run it and send back one text file. Nothing on their card is touched.

The two highest-value answers it returns without any hardware modification:
1. **The fused PCIe-gen ceiling** (doc 05 Test 1) — Gen1-only vs Gen1+Gen2.
2. **The advertised BAR1 / Resizable-BAR ceiling** — whether ReBAR can *ever* map the unlocked FB, or
   is hardware-capped like the PCIe gen.

## Safety — what it does and does not do
- **Does:** read PCIe config space, sysfs, `nvidia-smi`, `dmesg`; optionally (`--fuse`) read a small,
  fixed set of fuse/feature MMIO registers via a **read-only** `mmap` of BAR0.
- **Does NOT:** write any register or config value; change Above-4G / ReBAR / any BIOS setting; load,
  unload, or patch drivers; run the unlock; sweep unknown MMIO ranges.
- Config-space/sysfs reads carry no more risk than running `lspci`. The opt-in `--fuse` MMIO reads are
  still read-only and target only documented-safe registers; if Secure Boot / kernel lockdown is on, the
  `mmap` simply fails (harmless) and that section is skipped.

## Prerequisites
- Linux, `lspci` + `setpci` (the `pciutils` package); `nvidia-smi` if the driver is installed.
- `sudo` (for full `lspci -vvv`, `setpci`, `/proc/iomem`, `dmesg`).
- **For `--fuse` only:** Secure Boot / kernel lockdown **OFF**, and `python3`. Ideally run with the GPU
  otherwise idle.

## Running it
```bash
sudo ./probe-170hx.sh          # safe recon — do this first
sudo ./probe-170hx.sh --fuse   # also read targeted fuse/feature regs (opt-in, SB/lockdown off)
```
Output is teed to `recon-170hx-<timestamp>.txt`. Send that file back.

## What it collects, and how to read it

### §2 — PCIe link / generation  → doc 05 Test 1, doc 02
Reads `LnkCap` (`CAP_EXP+0c`), **`LnkCap2` (`CAP_EXP+2c`)**, `LnkCtl/Sta` (`+10`), `LnkCtl2/Sta2` (`+30`)
and decodes the `LnkCap2` supported-speeds vector (bits 7:1):

| Decoded `LnkCap2` | Meaning |
|---|---|
| `Gen1` only | The fuse allows only 2.5 GT/s → **software gen unlock is a dead-end**; realistic ceiling is Gen1 x16 (~4×) via the caps mod. |
| `Gen1 Gen2` | The fuse allows Gen2 → **Gen2 x16 (~8×) is on the table** with the caps mod; doc 05 Test 0/2 become worth running. |

This is the single most decisive cheap check and needs no patched driver.

### §3 — BAR layout / Resizable BAR / Above-4G  → the ReBAR question
- **`Physical Resizable BAR` → `BAR 1 … supported:`** lists the BAR1 sizes the GPU advertises. The
  **largest entry is the hard ceiling** on how much VRAM the CPU can map — almost certainly hardware/
  VBIOS-set, the ReBAR analogue of the fused PCIe-gen cap. If it tops out below the unlocked FB
  (64 GB / 40 GB), ReBAR can never expose the unlocked memory in full, no matter the BIOS setting.
- **BAR base addresses** (`/sys/.../resource`, `lspci` Region): a BAR mapped at `start ≥ 0x1_0000_0000`
  means Above-4G Decoding is genuinely on and working.
- **`dmesg` BAR lines**: `BAR 1: no space` / `can't reserve` / `failed to assign` = the BIOS couldn't
  fit the window (Above-4G off, or MMIO too small) — a setup problem, not the patch.
- **`nvidia-smi` BAR1**: current mapped BAR1 total/used.

Interpretation vs. the three ReBAR failure modes:
| Observation | Diagnosis |
|---|---|
| BAR1 stuck at 256 MB, no large size in supported list | Above-4G/ReBAR off in BIOS *or* GPU advertises no large BAR1 |
| ReBAR supported list tops out (e.g. 8/16/32 GB) below unlocked FB | BAR1 hardware-capped — the ReBAR analogue of the gen fuse |
| `dmesg` shows BAR assignment failure | MMIO allocation problem (Above-4G / motherboard window) |

### §1 — Identity
Device ID (`0x20c2`/`0x2082`), driver, VBIOS, and **stock `memory.total`** (grounds "is the 64 GB real
memory or a geometry view").

### §4 — PCIe capabilities
`DevCap2`/`AtomicOps`/`ACS` context for BAR and P2P behavior.

### §5 — Fuse / feature reads (opt-in `--fuse`)  → doc 07
Reads the exact approach-#1 recon targets: `EN_SW_OVERRIDE` (`0x820040`), the gen-fuse candidate status
words (`0x820c14`, `0x820d38`, `0x823814`), the stock SS0/SS1/CFG1/LMR values, and the `LnkCap2` PRIV
mirror. On a **stock** driver, PLM-protected rows read `0xbadfXXXX` — which is itself informative (it maps
what is PLM-walled without the patch). Positively identifying the gen bit still needs a 170HX-vs-A100
diff, but this pre-stages the target set (doc 07).

## What each result decides
| Result | Closes |
|---|---|
| `LnkCap2` = Gen1-only | PCIe-gen software unlock is dead → focus on x16 caps mod + retimer (doc 06 #4) |
| `LnkCap2` = Gen1+Gen2 | Gen2 reachable → doc 05 Test 0/2 worth running on the card |
| ReBAR BAR1 max < unlocked FB | ReBAR can't expose full unlocked VRAM → BAR1 is hardware-capped |
| ReBAR BAR1 max ≥ unlocked FB but stuck small | Setup/BIOS issue (Above-4G / CSM), not a hardware cap |
| `--fuse` rows readable / `0xbadf` map | Pre-stages the doc 07 fuse-diff target set for when the card arrives |

## Returning results
Send back the generated `recon-170hx-<timestamp>.txt` (and, if `--fuse` was run and the repo's read-only
`fusedump` is available, a `fusedump <bdf> 0x820000 0x825000` dump). Nothing in the file reflects a change
to the card — it is all observed state.

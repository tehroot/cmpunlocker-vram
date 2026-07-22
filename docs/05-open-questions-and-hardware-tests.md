# 05 — Open questions and hardware tests

What static analysis + community research could **not** settle, and the concrete on-card experiments
to run when the 170HX arrives. Ordered by value / decisiveness.

## Prerequisites on the card's host
- Linux, root, **Secure Boot / kernel lockdown OFF** (required for BAR0 mmap and unsigned modules).
- cmpunlocker installed (nvidia-open 610.43.0x + patched modules), for the privileged-read/override
  options that need the Booter/PLM primitive.
- Confirm the card's BDF: `lspci -D -d 10de:`.

---

## Test 0 (decisive, requires patched driver) — Booter `LnkCap2` write experiment
cmpunlocker's Booter exploit is an *arbitrary HS-privileged PRIV write*
(`kgspSec2PostblTimingRefillPayload` + `kgspExecuteBooterLoad_HAL`) — exactly the capability the
community said would be needed to attack PCIe gen, and a channel their host-side attempts (`setpci`,
`nvapoke`, root-port retrain) never used. So instead of *inferring* from static RE, we can directly
test whether `LnkCap2`'s supported-speeds vector is writable at Heavy-Secure level.

This probe is **opt-in and isolated** — it is NOT in the shipping unlock. Build it in with:
```
sudo ./install.sh --pcie-probe        # or: CMPUNLOCKER_PCIE_PROBE=1 sudo driver/build.sh
```
It lives in `driver/patches/experimental/0007-pcie-gen-probe.patch` so a normal install can't be
destabilised by it.

**What the probe does** (during GSP boot, *before* the SS0/SS1/CFG1/LMR unlock writes so those stay the
last touches and the compute/memory unlock is unaffected):
1. Reads `LnkCap` (`0x14088084`), `LnkCap2` (`0x140880a4`), `LnkCtl/LnkSta` (`0x14088088`).
2. Writes the Gen1–4 supported-speeds vector (`0x1E`, bits 4:1) into `LnkCap2` via the Booter payload.
3. Logs a host read-back. It does **not** retrain the link in-driver — retrain is the safer userspace
   root-port step (Test 2).

**dmesg** (`sudo dmesg | grep PCIeProbe`):
```
SEC2_DEBUG: PCIeProbe BEFORE: LnkCap=0x... LnkCap2=0x... LnkCtlSta=0x... (host PRIV read; 0xbadf..=PROT-walled)
SEC2_DEBUG: PCIeProbe WRITE LnkCap2: booter=OK wrote=0x0000001e host_readback=0x... (verify: setpci -s <bdf> CAP_EXP+2c.l)
```

**The in-driver `host_readback` is only a hint — do not judge on it.** `LnkCap2` is PROT-walled from host
MMIO and cmpunlocker opens the WPR/FBPA/FEAT PLMs, not the BIF/XVE one, so the read may return
`0xbadfxxxx` even when the Booter write landed. **Judge from userspace after boot instead:**
```
sudo setpci -s <bdf> CAP_EXP+2c.l     # LnkCap2 supported-speeds vector (authoritative)
sudo lspci -vvv -s <bdf> | grep -E "LnkCap2:|LnkSta:"
```
Then, only if the vector actually changed, do the safer userspace root-port retrain (Test 2) and re-read
`LnkSta`.

**Interpretation — the decisive signal is `LnkSta` negotiated speed, NOT the cap value:**
- **`LnkCap2` unchanged in `setpci`** → the Booter write was rejected → fuse/PROT model confirmed; PCIe
  gen is a software dead-end. (Most likely outcome — `FUSE_PCIE_GEN23_DIS=1`, `EN_SW_OVERRIDE=0`.)
- **`LnkCap2` shows the new vector but `LnkSta` stays at 2.5 GT/s after a retrain** → the *advertised cap*
  is writable but the PHY/LTSSM is still fuse-clamped → **still a dead end**. A stuck cap alone does NOT
  mean speed is unlockable.
- **`LnkSta` shows >2.5 GT/s after retrain** → genuinely viable; this would overturn doc 02's fuse verdict.
  Proceed to characterise the fused ceiling and the x16 caps mod.

Direct hardware measurement supersedes the static analysis either way — but only the `LnkSta` result, not
the cap write, is proof.

**If Test 0 fails** (write rejected, or cap sticks but `LnkSta` stays Gen1): the ranked fallback avenues
and the full first-pass plan are in [doc 06](06-pcie-gen-attack-avenues.md); the deepest software gamble
(the fuse-override-enable attack) and its pre-staged on-card recon are in
[doc 07](07-fuse-override-and-static-recon.md).

---

## Test 1 (highest value, cheap) — read `LnkCap2`: settle Gen1-vs-Gen2 ceiling
The single most decisive cheap check. `LnkCap2`'s supported-speeds vector **is** the fused ceiling
(firmware never writes it — doc 03).
```
sudo lspci -vvv -s <bdf> | grep -iE "LnkCap:|LnkCap2:|LnkSta:"
sudo setpci -s <bdf> CAP_EXP+2c.l     # LnkCap2 raw (supported speeds in bits 7:1)
sudo setpci -s <bdf> 0x84.l           # LnkCap (max link speed in bits 3:0: 1=Gen1,2=Gen2,...)
nvidia-smi --query-gpu=pcie.link.gen.max,pcie.link.gen.current --format=csv
```
- **LnkCap2 shows Gen1+Gen2** → the fuse allows Gen2; cmpunlocker's "Gen2" is plausible (reaching the
  fused max), and Gen2 x16 (8 GB/s/dir) is on the table with the caps mod. Proceed to Test 2.
- **LnkCap2 shows Gen1 only** → Gen2 isn't negotiable; the cmpunlocker "Gen2" claim is overstated; the
  realistic PCIe ceiling is Gen1 x16 (4 GB/s/dir) via the caps mod. Skip Test 2/3 (speed is fused).

## Test 2 — directed speed change (does the PHY train above Gen1?)
Only meaningful if Test 1 shows Gen2 is in the supported set. This is the community's Path-2 method
(root-port directed retrain). Standard `pcie_set_speed` approach:
```
# find the upstream root port / bridge for <bdf>, then set its Target Link Speed = 2 (Gen2)
sudo setpci -s <root_port> CAP_EXP+30.w=...2      # LnkCtl2 target speed
sudo setpci -s <root_port> CAP_EXP+10.w=...       # set Retrain Link bit (bit 5) in LnkCtl
sudo lspci -vvv -s <bdf> | grep LnkSta            # 5GT/s? or stays 2.5GT/s?
```
Also try cmpunlocker's build if you have it (it claims to do this + endpoint-side work); check
`sudo dmesg | grep -iE "SEC2_DEBUG.*(Root port|14118f78|PLM)"`. Host-dependent: some root ports don't
support directed speed change.

## Test 3 (verification, not discovery) — fuse-block dump + A100 diff
Confirm the fuse story first-hand and pin the exact PCIe fuse (community's `FUSE_PCIE_GEN23_DIS`
address was unspecified). Use `driver/.build/fusedump/`:
```
gcc -O2 -o fusedump fusedump.c
sudo ./fusedump <bdf> 0x820000 0x825000 > fuse_170hx.txt   # fuse aperture
# do the same on an A100 (bare-metal PCIe A100 w/ root, or a public dump), then:
diff fuse_170hx.txt fuse_a100.txt
```
- Rows that differ = candidates; expect `EN_SW_OVERRIDE (0x820040)=0` and a PCIe/gen fuse bit set on
  the 170HX vs clear on the A100. Note: `0xbadf....` = PLM-protected (userspace can't read); the
  cmpunlocker patched driver (PLMs open) can read those via added `SEC2_DEBUG` prints.

## Test 4 — verify the compute + memory unlock actually lands
The part cmpunlocker genuinely does (and is ahead of the public community state). After install +
**cold reboot**:
```
nvidia-smi                                          # 8GB card → ~65536 MiB
sudo dmesg | grep SEC2_DEBUG                         # PLMs → 0xffffffff; SS0/SS1/CFG1/LMR writes
./gpu_burn -m 63500 -d 30                            # 0 memory errors on ~63 GB
nvidia-smi --query-gpu=clocks.max.sm --format=csv    # full SM clocks
```

## Test 5 (exotic / speculative) — the retimer speed avenue
The only remaining *speed* path if the fuse caps negotiation. A protocol-aware PCIe **retimer
interposer** (Astera Aries / TI DS160PR810 class) that rewrites the Rate ID in TS1/TS2 ordered sets.
Viable **only if** the GPU's serdes physically trains above the fused rate (likely, being A100
silicon, but unproven). Requires a custom interposer PCB; multi-month hardware research, not a recipe.

## The x16 width mod (independent, community-proven)
Solder the **~24 missing 0402 0.22 µF AC-coupling capacitors** on lanes 4–15. Restores x16 width
(4× bandwidth on its own). Yields x16 at **Gen1** unless Test 1/2 also unlock Gen2. This is the
highest-leverage *physical* mod and doesn't depend on any firmware/fuse question.

---

## Open questions the hardware can't fully answer either
- **Tensor-core sub-throttle:** community reports ~200 TFLOPS actual vs ~312 theoretical even after
  the compute unlock — a "dispatch-level hardware gate" with no compiler workaround. cmpunlocker's
  "173 TFLOPS BF16" is the unlocked-but-still-tensor-throttled number. Enforcement point (Falcon /
  VBIOS power table / hardwired) unestablished.
- **Gen-fuse depth (policy vs PHY):** Test 0 answers whether `LnkCap2` is register-writable.
  If REJECTED, whether the serdes is *physically* Gen-capable (retimer-beatable) vs PLL-fused (dead)
  can only be answered by the retimer experiment itself.

## Firmware-frontier (if pursuing the signing angle — low probability)
GA100 uses **FalconUCodeDescV2** (Turing lineage, the generation OMGVflash broke) — the community's
"most actionable" firmware-signing lead. The FwSec HS secure tails (`fwsec/fwsec_*_sec.bin`) are the
target; `ghidra_falcon` + `ghidra` (now installed) is the tool. Honest assessment: this is the
"pretty pretty impossible / multi-month" frontier — high value if cracked (would enable VBIOS mods),
low probability. Note the FwSec is byte-identical 170HX↔A100, so any vuln would be **GA100-generic**,
not 170HX-specific.

## Bottom line for planning
- **Software:** compute + memory unlock works (cmpunlocker). PCIe gen is a *suspected* fuse dead-end
  per static analysis. **Test 0** (Booter `LnkCap2` write experiment) is the definitive test — software
  unlock is viable only if `LnkSta` negotiates above Gen1 after a retrain; a rejected write, or a stuck
  cap with `LnkSta` unchanged, confirms the fuse model.
- **Hardware:** the **x16 caps mod** is the concrete win (4×, demonstrated). Gen2 (another 2×) hinges
  on Test 0/1. A retimer is the only speed avenue beyond that, and it's exotic/unproven.
- **Realistic PCIe ceiling:** 4 GB/s/dir (Gen1 x16, demonstrated) to 8 GB/s/dir (Gen2 x16, if the
  fuse allows) to 16 GB/s/dir (Gen3 x16, only if Test 0's `LnkSta` clears Gen1) — pending hardware validation.

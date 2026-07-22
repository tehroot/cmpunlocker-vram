# 06 — PCIe-gen attack avenues (first-pass experiment plan)

> **Supporting information for the first on-card experimentation pass on the CMP 170HX (GA100), to run
> when the physical card arrives.** Extends [doc 05](05-open-questions-and-hardware-tests.md); assumes
> cmpunlocker installed. This is the strategic map; the deep dive on avenue #1 and the static recon
> already done are in [doc 07](07-fuse-override-and-static-recon.md).

## Where this picks up

The entry point is doc 05 **Test 0** — the corrected HS-Booter `LnkCap2` write probe
(`driver/patches/experimental/0007-pcie-gen-probe.patch`, opt-in via `sudo ./install.sh --pcie-probe`).

Test 0 asks: can the HS-Booter arbitrary write set `LnkCap2` (`0x140880a4`), and does `LnkSta` then
negotiate above Gen1?
- **Yes** (LnkSta clears 2.5 GT/s after a retrain) → PCIe gen is on the table; stop here and characterise.
- **No** (write rejected, *or* the cap sticks but `LnkSta` stays Gen1) → the fuse/PROT model holds, and
  the avenues below are the ranked fallback.

**Key diagnostic from a Test 0 failure:** the Booter *can* write PLM-gated registers — that is the entire
compute/memory unlock. So a `LnkCap2` write failure means the gen cap is protected **stronger than a
PLM** (a fuse latch or hardwired PROT). That steers the search off register-poking and toward one of
three layers: (a) the fuse-enforcement mechanism, (b) the link-training layer *below* the advertised cap,
or (c) escalating the primitive itself.

## Before you have a card — read-only recon
Hand any community member with a stock 170HX the read-only probe
[`recon/probe-170hx.sh`](../recon/probe-170hx.sh) (guide: [`recon/README.md`](../recon/README.md)). It
modifies nothing and returns, without hardware changes, the two ceilings that gate this whole plan: the
**fused PCIe-gen ceiling** (Test 1 — Gen1-only vs Gen1+Gen2) and the **advertised BAR1 / Resizable-BAR
ceiling** (whether ReBAR can ever map the unlocked FB). Run this first; it may settle avenues before the
card arrives.

## Do these first — cheaper than any attack

- **Pin the exact fuse semantics (doc 05 Test 1).** Does `FUSE_PCIE_GEN23_DIS` gate Gen2 *and* Gen3, or
  only Gen3+? `setpci -s <bdf> CAP_EXP+2c.l`. If Gen2 is actually *allowed*, the whole problem collapses
  to an already-reachable tier + the x16 mod — no exotic attack needed for the 8 GB/s bracket.
- **x16 width mod** (doc 05). Solder the ~24 missing 0402 AC-coupling caps on lanes 4–15. ~4× on its own,
  community-proven, orthogonal to gen. This is the concrete bandwidth win while everything below stays
  theoretical.
- **NVLink** — a *parallel* bandwidth path (GA100 has it; may be PLM/fuse-gated like compute). Caveat: it
  only helps GPU↔GPU with a physical bridge, not host↔GPU over the slot, so limited utility for a single
  card in a normal slot.

## Software avenues (build on the Booter primitive)

### #1 — Attack the fuse-override *enable*, not the fuse value  — *best software gamble*
The reason fuses look unwritable is `NV_FUSE_EN_SW_OVERRIDE = 0` (`0x820040`). But that is the value, not
the *nature*: if `EN_SW_OVERRIDE` is a PLM-gated **register** (not a hard fuse), the Booter can open the
fuse-block PLM, set it to 1, then write the `OPT_PCIE_GEN` shadow — attacking the mechanism that makes
fuses immutable rather than the fuse itself. This is the same override trick cmpunlocker already does for
SM features, one level down. **Full mechanism, the four go/no-go unknowns, the static fuse-map findings,
and the targeted on-card recon are in [doc 07](07-fuse-override-and-static-recon.md).**

### #2 — Escalate arbitrary-write → HS code execution
Today the exploit is "make signed Booter perform one MMIO write." If the crafted payload can be pushed to
a control-flow hijack (overwrite a saved return / function pointer via the arbitrary write → ROP inside
the signed Booter), you get *arbitrary execution at Heavy-Secure*: re-sense fuses, reprogram the PHY PLL
block (`0x14118xxx`, currently gated behind signed-HS ucode), or no-op the gen clamp in closed GSP-RM.
Hardest, biggest force multiplier, GA100-generic if it lands.

### #3 — Poke the closed GSP path: `BUS_SET_PCIE_SPEED`
`NV2080_CTRL_CMD_BUS_SET_PCIE_SPEED` (Gen1–6) is declared and dispatched but its `_IMPL` lives in closed
GSP-RM — untested against this card. Issue it for Gen3/4 and observe whether GSP honours or clamps it. If
the clamp is a *software policy check* (reads the fuse, refuses) rather than the PHY physically refusing,
then combined with #2 it's a no-op target. Cheap experiment; likely clamps; currently an open item.

## Hardware avenues

### #4 — Retimer interposer  — *only path currently known to be physically real*
Works at the link-training layer, *below* the fuse: a protocol-aware retimer (Astera Aries / TI
DS160PR810 class) rewrites the Rate ID in the TS1/TS2 ordered sets during training, forcing both ends to
negotiate up. Viable **iff** the SerDes physically trains above the fused rate — plausible (A100 silicon)
but unproven. Custom interposer PCB, multi-month; it does not fight the fuse, it sidesteps it.

### #5 — Board-strap check
Some PCIe gen limits are set by board straps (resistors / boot GPIOs), not fuses. Low probability given
the fuse finding, but *cheap* to check and re-strap. Diff the 170HX board's strap config against an A100's.

### #6 — Fault/glitch injection on the HS signature verify
Voltage/clock-glitch the Falcon during the RSA-3072 signature check to bypass it and load modified
VBIOS/GSP-RM (OMGVflash-adjacent). Serious rig, precise timing, physical access. Real technique, low
probability, high payoff (enables actual firmware mods, GA100-wide).

## Honest dead ends
- **Un-blow the eFuse** — `GEN23_DIS=1` is a *blown* bit; eFuses blow 0→1 and are irreversible.
- **RAM-patch the VBIOS alone** — defeated by DMA-time signature verification, no TOCTOU window (unless
  paired with #6 glitching).
- **More host-MMIO pokes at the link registers** — PROT-walled; community-exhausted.

## First-pass ordering (by info-per-effort)
1. **Test 1** — read `LnkCap2`/`LnkSta` via `setpci`. Settles Gen1-vs-Gen2; may end the whole question.
2. **Test 0** — HS-Booter `LnkCap2` write. The novel software channel nobody has tried.
3. **#1 recon** — `fusedump` the 4 candidate fuse registers (doc 07). Cheap, high-information.
4. **#3** — `BUS_SET_PCIE_SPEED` Gen3/4 and observe. Cheap.
5. **#1 write-test** — `EN_SW_OVERRIDE = 1`, only if recon warrants. The go/no-go for #1.
6. **x16 caps mod** — the concrete bandwidth win regardless of the gen outcome.
7. *Long-horizon:* #2 (HS code-exec), #4 (retimer), #6 (glitch).

## Probability (honest)
- **Cracks open** only if a full chain holds: `EN_SW_OVERRIDE` is a register (#1/Q1) **and** its PLM is
  Booter-openable **and** gen reads an overridable shadow **and** the override survives a re-latch.
- **Most likely** the first pass confirms the fuse dead-end for software and the practical outcome is
  **Gen1 x16 (~4×) via the caps mod**, with Gen2 x16 (~8×) only if Test 1 shows the fuse allows Gen2.
- The retimer (#4) is the only credible path to Gen3 x16, and it's exotic/unproven.

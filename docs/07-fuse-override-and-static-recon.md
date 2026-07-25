# 07 — Fuse-override attack (#1) & static fuse-map recon (first-pass experiment support)

> **Superseded — see [doc 00](00-current-state.md) for current state.**
> **Q1 is answered: `EN_SW_OVERRIDE` (`0x820040`) IS a writable, persistent register.** It did not open the fuse route -- the OPT bank turned out to be master-gated, not privilege-gated ([doc 21](21-app08-opt-magic.md)).


> **Supporting information for the first on-card experimentation pass on the CMP 170HX (GA100).** Deep
> dive on avenue **#1** from [doc 06](06-pcie-gen-attack-avenues.md), the card-less static analysis
> already run against the VBIOS, and the targeted on-card recon it pre-stages. Read doc 06 first for
> where this fits; builds on the firmware map in [doc 03](03-firmware-reverse-engineering.md).

## The mechanism — how a fuse value reaches the hardware

A fuse value flows through a pipeline, and the attack lives in the middle of it:

1. **Raw eFuse array** (OTP). Physical one-time bits; blown = 1; **irreversible** (blow 0→1 only). On the
   170HX, `FUSE_PCIE_GEN23_DIS = 1` is a *blown* bit — attacking the raw fuse is off the table.
2. **Sense at cold reset** → each value latched into an **OPT shadow register** (`NV_FUSE_OPT_*`,
   `0x8200xx–0x8207xx` / `0x824xxx`). This is what the chip reads at runtime.
3. **These shadows are marked writable.** In GA100 `dev_fuse.h` every `NV_FUSE_OPT_*` is `RW-4R` with a
   `RWIVF` data field — latches *designed* to be overrideable (bring-up/test).
4. **A global gate decides whether OPT writes take effect: `NV_FUSE_EN_SW_OVERRIDE` (`0x820040`).**
   Community read it as `0`. `1` = writes to OPT registers override the sensed fuse value.
5. **Consumers** (HW blocks / firmware) read the sensed/OPT value.

The entire "fuse = dead end" verdict rests on step 4 reading 0. Note: GA100's published `dev_fuse.h` is a
**curated subset** — 110 defines, all OPT sensed-values, **no** controller / enable / PLM defines. NVIDIA
stripped the programming machinery from open source, so the override/PLM addresses come from live probing,
not the tree.

## Why this is cmpunlocker's trick one level down (and why it's harder)

The compute unlock writes SS0/SS1 (`0x82381C`/`0x823820`) — the **feature-override path for SM features**,
FEAT-PLM-gated (`0x823804`), which the Booter opens. That works because SM speed is a *soft* limit: no
fuse override needed. Approach #1 is the identical pattern applied to the **fuse-override path**: the
`NV_FUSE_OPT_*` registers are the override path for fuse *values*, gated by `EN_SW_OVERRIDE` + a fuse-block
PLM. **But gen is a fuse, so it is strictly harder** — there is an extra wall (`EN_SW_OVERRIDE = 0`) that
the compute unlock never had to climb.

## The four load-bearing unknowns (each a go/no-go gate)

- **Q1 (master) — is `EN_SW_OVERRIDE` (0x820040) a *register* or a *fuse*?** A PLM-gated register whose
  reset value is 0 → the Booter opens the fuse PLM, writes `0x820040 = 1`, and the OPT-override path opens.
  A raw fuse reading 0 → "0" means *unblown*, and enabling it needs physical fuse programming (VPP, gated /
  absent on production boards). Dead.
- **Q2 — is the `0x820xxx` fuse block behind a PLM the Booter can open?** cmpunlocker opens FEAT/FBPA/WPR —
  none govern the fuse block, which is usually protected more strongly. Unknown whether the primitive reaches it.
- **Q3 — does the gen consumer read the OPT shadow (overridable) or the raw fuse *directly* (bypass)?**
  Product-differentiation/security fuses are frequently wired to read direct, *specifically to defeat this
  attack*. If `GEN23_DIS` is one of those, override changes nothing.
- **Q4 — re-latch semantics.** The PHY latches gen at cold reset, before software. A post-boot OPT write
  only matters if a warm retrain re-reads it — and a re-sense may overwrite the OPT value unless override
  is applied *after* sense and persists (override-wins).

**Crack condition:** Q1 register **and** Q2 openable **and** Q3 shadow-read **and** Q4 override-persists.
**Kill condition:** any one of Q1 fuse / Q3 direct / Q4 sense-wins.

## What the VBIOS already tells us (static, card-less)

Decoded the fuse-check tables in `fwsec/inner_170hx.rom`. **Entry format: 12-byte `{flags, register,
bitmask}`**; the firmware walks them — *read `register`, test `bitmask`, branch*. Findings:

- **`EN_SW_OVERRIDE` (0x820040) is real and wired into the fuse pipeline.** It appears as a genuine table
  entry — `register = 0x00820040, bitmask = 0x00000001` (bit 0, the enable) — with a redundant copy at
  `+0x60000`. So the override gate is architecturally present and firmware-consulted; it is not a community
  abstraction. (Community read the bit as 0 on the 170HX.)
- **~50 fuse-block registers enumerated.** The gen-disable bit lives in a packed **status/readout** word,
  read bit-by-bit. Prime candidates:
  | Register | Refs in ROM | Note |
  |---|---|---|
  | `0x820c14` | **50×** (bit 0x01, 0x02, 0x04, 0x08, 0x10 …) | main packed feature/floorsweep status word |
  | `0x820d38` | **24×** | second heavily-read status word |
  | `0x823814` | 2× | `NV_FUSE_FEATURE_READOUT` (RO; only ECC-DRAM bit documented) |
- **The gen bit cannot be pinned statically.** The tables carry no semantic labels, and the fuse *values*
  are not in the ROM at all — the VBIOS is byte-identical 170HX↔A100 (doc 03), so the crippling is in
  silicon. Distinguishing the gen bit needs the live 170HX-vs-A100 fuse diff. (This is why doc 05 left
  `FUSE_PCIE_GEN23_DIS`'s address unspecified — it is genuinely not statically determinable.)
  **Since resolved on-hardware:** the beta Gen2 log pins the gen-fuse OPT register at **`OPT_GEN23 =
  0x82057c`** (reads `0x1` = Gen2/3 disabled) — see [doc 09](09-onhw-pcie-gen-beta-result.md).
- **Corroboration for Q3 (unfavorable).** app `0x08`'s code references the PHY block (incl. `0x14118f78`)
  and `LnkCtl/LnkSta` (`0x14088088`) but **not** `LnkCap2` (`0x140880a4`). Matching doc 03: the gen cap is
  hardware-latched at reset and consumed outside firmware — i.e. pointing to raw-fuse-direct consumption.

## Targeted on-card recon — the payoff

Static work converted a blind sweep into a **4-register check**. Prereqs (doc 05): cmpunlocker installed,
Secure Boot / lockdown off, `fusedump` built, **both a 170HX and a reference A100**.

1. `fusedump <bdf> 0x820000 0x825000` on the **170HX** and the **A100**; also read `0x820c14`, `0x820d38`,
   `0x823814`, `0x820040`. If any read `0xbadf…`, re-read privileged via the patched driver's post-PLM-open
   `SEC2_DEBUG` prints (`fusedump/DUMP_TARGETS.md` Tier 3).
2. **Diff the status words** → the bit that differs between 170HX and A100 **is** `FUSE_PCIE_GEN23_DIS`
   (answers "which register + which bit").
3. **Read `0x820040`** → `EN_SW_OVERRIDE`'s real value (expect 0).
4. **Go/no-go write test (Q1):** with the fuse-block PLM open (Booter primitive), write `0x820040 = 1` and
   read back.
   - **Sticks** → `EN_SW_OVERRIDE` is a register (Q1 pass) — the single biggest result of the investigation.
     Then write the identified gen OPT/shadow row, force a userspace root-port retrain, read `LnkSta`
     (tests Q3/Q4 in one shot).
   - **Rejected** → it is fuse-side; #1 is dead. Fall back to doc 06 avenues #2 / #4 / #6.

## Status: Q1 is UNTESTED — the gate has never actually been opened
> **Correction (supersedes an earlier "leans dead" verdict).** The on-hardware evidence to date does
> **not** resolve Q1, because the test has never been run correctly:
> - The beta Gen2 branch ([doc 09](09-onhw-pcie-gen-beta-result.md)) wrote the gen fuse `OPT_GEN23` — now
>   pinned on-hardware at **`0x82057c`** — directly to `0`, and it stayed `1` ("FAILED to set OPT_GEN23").
>   **But that branch never wrote `EN_SW_OVERRIDE` (`0x820040`) first.** So the failure is the *expected*
>   behaviour of a gated OPT write with the gate still shut — **not** evidence that the gate is fuse-side.
> - The gist's "`EN_SW_OVERRIDE` is inert / cannot change" ([doc 08](08-vbios-mac-fuse-map-external.md)) is
>   an unverified secondhand claim with unknown methodology (did they open the fuse-block PLM? use an HS
>   write?). It is not evidence Q1 is closed.
>
> The two gates in step 4 above are **different**: the **PLM** (priv-level *access* — which the beta opened)
> and **`EN_SW_OVERRIDE`** (whether an OPT write actually *takes*). Opening the first without the second is
> why every direct `OPT_GEN23` write has failed. The corrected **`driver/patches/experimental/`-style Gen2
> `0007-pcie-gen2.patch`** (modified to write `EN_SW_OVERRIDE = 1` *before* `OPT_GEN23`, readback-gated so
> it skips the retrain unless the fuse flips) is exactly this Q1 test — built and ready to boot.

**Odds, honestly:** if `EN_SW_OVERRIDE` is a PLM-gated register, Q1 passes and the entire PCIe-gen question
reopens; if it is itself fuse-backed, Q1 fails and #1 is dead. Even a Q1 pass leaves Q3 (does the consumer
read the overridable shadow or the raw fuse directly), where the static evidence leans unfavourable. Net: a
real but lower-probability shot — worth running because the test is **one boot away** and a positive Q1
would be the single biggest result of the investigation. Run it right after the doc 05 Test 0/Test 1 reads.

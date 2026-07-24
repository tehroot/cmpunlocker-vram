# CMP 170HX PCIe-Gen1 Lock - Field Manual

**Scope:** the PCIe link-SPEED cap (Gen1) on the NVIDIA CMP 170HX (GA100): what the lock is, how it is enforced, what was tried, and what remains.
**Date:** 2026-07-24
**Status:** Software and keyless-firmware surface exhausted; remaining paths are physical.

---

This is the practical "what the lock is / how it is enforced / what was tried / what remains"
reference for the PCIe link-SPEED cap on the CMP 170HX. The SPEED cap (Gen1) and the WIDTH cap
(x4) are independent problems; this manual covers SPEED. WIDTH x4->x16 is a separate PCB
solder mod (missing AC-coupling caps C1100-C1350, 24x 0402 0.22uF) that stays Gen1 and is out
of scope here.

Two sibling locks on the same die are already DEFEATED on silicon and are unaffected by any
PCIe attempt: memory (8GB -> 64GB) and compute throttle-off (12.71 TFLOPS). PCIe-Gen is the
third and hardest, and it is a hardware wall.

---

## 1. What the lock is

The 170HX trains PCIe at Gen1 x4 (2.5 GT/s) while the GA100 die and PHY are natively Gen4
capable and the host root port (AMD Raphael/Genoa) is Gen4. The Gen1 speed is an artificial
cap held by TWO independent gates: a signed VBIOS devinit strap and an OTP silicon fuse. Either
alone would hold; both are present, so the cap is double-locked.

| Property | Value |
|----------|-------|
| Observed link | Gen1 x4, 2.5 GT/s (LnkSta) |
| Die / PHY native | Gen4 (PL_LINK_RATE 0x8C1C0 reads Gen3+ capable) |
| Host root port | Gen4 (16 GT/s), not the limiter |
| Gate A (firmware) | Signed devinit strap 0x14118F78, RMW at VBIOS offset 0xE88C |
| Gate B (silicon) | FUSE_PCIE_GEN23_DIS = 0x1 (0x0 on A100) |
| devId / VBIOS | 0x20C2 / 92.00.6D.00.0A (300W ROM) |
| Verdict | Hardware wall; software + keyless-firmware surface exhausted on silicon |

---

## 2. Mechanism chain

Gen1 is enforced end to end from reset strap to the LTSSM rate decision. Each stage bounds the
next; nothing downstream can overshoot the cap seeded upstream.

| Stage | Where | Action |
|-------|-------|--------|
| 1. Devinit strap | Falcon PRIV 0x14118F78 (>16MB, NOT host BAR0) | Signed FWSEC-devinit RMW: `mov r9 0x14118f78; ld; and 0x3ff / or 0x400; st` at VBIOS offset 0xE88C. 26 refs in every ROM; 170HX-vs-A100 delta is the VALUE written, not the code. Latched at reset. |
| 2. Supported cap | NV_XVE_LINK_CAPABILITIES_2.SUPPORTED_LINK_SPEED (cfg 0xA4 / BAR0 mirror 0x8808C) | Strap seeds this. READ-ONLY (R-EVF/R---V), no write port. Encodings: Gen1=0x1, Gen1_2=0x3, Gen1_2_3=0x7, Gen1_2_3_4=0xF. |
| 3. RM enforcement | GSP-RM fn RMPcieLinkSpeed 0x4DFF358 | Reads supported source 0x85080[23:20] (jump-table index, rodata @ vaddr 0x408A118) + current 0x88088[19:16]; writes allowed-Gen mask 0x85084[3:0], BOUNDED by the supported cap. Re-derived every retrain. Enforces, does not originate. |
| 4. LTSSM | Link training TS1/TS2 | Negotiates only up to advertised SUPPORTED. Writable TARGET_LINK_SPEED (0x880A8) cannot exceed the RO cap. |
| 5. OTP fuse | FUSE_PCIE_GEN23_DIS = 0x1 | Independent of firmware. Disables the Gen2/3 SerDes at silicon, DOWNSTREAM of every override register. Terminal gate. |

Boot order: FWSEC-devinit (programs + latches SUPPORTED) -> SEC2 booter (hosts the cmpunlocker
CSB timing-hole gadget) -> GSP-RM. SUPPORTED latches BEFORE any exploit window opens.

The strap field is MONOTONIC-RESTRICTIVE: higher value = FEWER gens (0=all, 3=170HX setting
clearing Gen2/3/4, 0xF=out-of-range all-disabled). Writing it higher (the intuitive and the
community `pcie_set_speed` direction) is backwards; raising the ceiling needs a LOWER strap, and
no write port exists.

---

## 3. Register reference

Every PCIe-relevant register on the reachability map. Addresses are SMN/BAR0 unless noted.

| Addr | Name | Access | Role |
|------|------|--------|------|
| 0x14118F78 | PCIe Gen devinit strap (Falcon PRIV bus, >16MB, not host BAR0) | RW by signed devinit only | ORIGIN of the cap |
| 0x85080 | Supported-speed source [23:20] (jump-table index) | RO / poison-walled (0xBADF1100) | Cap source, zero writers in 4.1M lines RM disasm |
| 0x85084 | Allowed-Gen mask [3:0] | Derived, re-clamped each retrain | GSP re-derives from 0x85080, bounded by cap |
| 0x88088 | LINK_CONTROL_STATUS, live speed = (v>>16)&0xF | RO status | Speed oracle |
| 0x88084 | NV_XVE_LINK_CAPABILITIES MAX_LINK_SPEED [3:0] | R-XVF (RO, no write port) | PHY reflection |
| 0x8808C | NV_XVE_LINK_CAPABILITIES_2 SUPPORTED_LINK_SPEED [7:1] | R-EVF (RO, no write port) | PHY reflection |
| 0x880A8 | NV_XVE_LINK_CONTROL_STATUS_2 TARGET_LINK_SPEED | RW, capped by SUPPORTED | Target, cannot exceed cap |
| 0x8841C | NV_XVE_PRIV_MISC_1 CYA GEN2/3 override EN/VAL (bits 11-16, 30, 31) | RW (PLM) | Writable but inert |
| 0x88610 | VSEC_HIERARCHY bit12 (gates PRIV_MISC_1 reprogram) | RW | Writable but inert; live value 0x00001001 |
| 0x8872C | LTSSM retrain (write 6) | W | Retrain trigger |
| 0x8C1C0 | PL_LINK_RATE, gen field [19:16] (RM reads as current; [23:20] is a distinct subfield) | RW, downstream of fuse | Reads Gen3-capable |
| 0x8E1B0 | XP3G_PLM | RW (HS-ROP opened it) | PHY-block PLM |
| 0x8E110 / 0x8E120 | XP3G_OVR0 / VAL0 (PHY rate force) | RW, downstream of fuse | Writable but inert |
| 0x8E11C / 0x8E12C | XP3G_OVR3 / VAL3 (PHY rate force) | RW, downstream of fuse | Writable but inert |
| 0x82057C | OPT_GEN23 fuse-option shadow | RO (hard, even HS-ROP) | Pure OTP fuse-sense reflection |
| 0x820580 / 0x820520 | OPT_GEN3 / OPT_MAGIC | RO | Fuse-option shadows |
| 0x823800 | FUSE_FEATURE_OVERRIDE (FEAT_OVR block base); PLM 0x823804 | RW, no PCIe override field | Override-enable fused OFF for PCIe |
| FUSE_PCIE_GEN23_DIS | OTP silicon fuse = 0x1 (A100 = 0x0) | Fuse, no write port | Terminal SerDes gate |

Reachability note: FEAT_OVR (0x82381C/0x823804) and FBPA (0x9A0204) are INSIDE the 16MB BAR0 and
PLM-open makes them writable, which is why memory/compute unlock lands from the KMD postbl point.
The PCIe config-cap regs (0x88084/0x8808C/0x880A8) are inside BAR0 but HARDWARE read-only (no
write port at any priv). The strap 0x14118F78 is at ~321MB, outside BAR0; the only host aperture
(NV_PBUS_BAR0_WINDOW 0x1700) maps VID/SYS memory (PRAMIN), not PRIV registers, so there is no
host aperture to it at all. The SEC2 CSB mailbox gadget 0x10B9 (0xFF01C100=addr / 0xFF01C200=val
/ 0xFF01C000=cmd 0x800000F2, full 32-bit unmasked) DOES reach the XP3G/PCIe priv block (proven on
card), but the strap write is post-devinit-latch and does not re-latch.

---

## 4. Attack surface

Every method tried, its mechanism, and why it fails.

| Method | Mechanism | Result / why it fails |
|--------|-----------|-----------------------|
| setpci LnkCap2 (cfg 0x2C) <- all-speeds | Host config-space write | Silently dropped; register hardware-RO |
| Root-port retrain (TARGET 0x880A8 + retrain) | Raise target, re-train | Re-trains Gen1; endpoint re-advertises Gen1 in TS1/TS2, bounded by RO SUPPORTED |
| MMIO BAR0 link regs (0x88070/0x8808C/0x88090) | Host BAR0 write | PROT-walled from host (read 0 / write ignored) |
| HS-ROP driver GPU_REG_WR32 (repeated PLM-open driver writes) | PLM-open then driver write | PLM-walled; cannot reach Falcon-PRIV 0x14118F78 (>16MB); config-cap regs stay RO even post-PLM-open |
| HS-ROP SEC2 mailbox gadget 0x10B9 (full 32-bit) | CSB write, reaches priv | Opened XP3G_PLM, but 0x14118F78 is a devinit-RESET-time strap; runtime write does not re-latch; bar0_master store capped <16MB |
| HS-priv XP3G PHY-rate override | Open XP3G_PLM, write OVR/PL_LINK_RATE | PLM opened, regs WRITABLE (rate read Gen3), link stayed Gen1 -> fuse gates SerDes downstream of the overrides |
| HS-priv FEAT_OVR PCIe-override write with retrain | Write 0x823800, retrain | 0x823800 read back 0xFFFFFE8E (write took); OPT_GEN23 stayed 0x1, link Gen1 -> PCIe override-enable is fused OFF (unlike SM_SPD, fused ON) |
| HS-priv OPT_GEN23 / mask direct write | PLM-bypass write 0x82057C, 0x85084 | 0x82057C write FAILED (hard RO); 0x85080/0x85084 read 0xBADF1100 poison (access-walled from injection point) |
| CYA PRIV_MISC_1 / VSEC bit12 clear + retrain | Clear bit12, set CYA, retrain | Writable and STUCK, but link stayed Gen1 (bounded by 0x85080; RM 0x4DFC854 did not complete its Gen2 sequence) |
| Derived-mask write 0x85084 at postbl | Write (old&~0xF)|0xF at postbl | Reads POISON 0xBADF1100 (PLM-walled); write dropped; "GSP writes 0x85084" is HS-priv the injection point never reaches |
| csigenc ACL-0x13 spill | Reuse the SEC2 postbl HS-ROP to invoke the csigenc crypto instruction (sets ACL=0x13 Insecure-Readable on its output on Turing/TSEC-class Falcons) to leak an HS secret past the 1-bit boot oracle | DEAD offline: envydis shows the SEC2 booter secure body is ciphertext 0x101-0x86FB (csecret(6) AES) with zero SCP/crypto opcodes in the plaintext stub; the csigenc gadget, if present, is in the encrypted body with no pinnable ROP address; near-certainly hardened post-TSEC on Ampere; not offline-validatable |
| Master-key signature bypass | Find an Ampere Falcon bootrom / HS signature-verify flaw for arbitrary HS code | None exists: cmpunlocker test_bootrom_bug.py encodes the KNOWN load-before-verify timing hole (hardcoded hmac_bypass), not a new flaw; the ROP primitive is data-only register pokes (mpopaddret + write gadget), not arbitrary Falcon code (body AES-encrypted, unsignable); no plaintext iowr/CSB gadget with a controllable target (plaintext ends at 0x101); no HS-reachable Ampere CVE (Requiem/TSEC is older Falcon, GSP-RM RPC CVEs are lower priv than HS SEC2) |
| RAM-patch TOCTOU | Patch the signed firmware in system RAM between load and verify | Closed on Ampere: signature validation happens DURING the DMA into IMEM (no load-vs-verify window); modified bytes rejected before execution. The SEC2 postbl hole is a separate data-only-poke path, not code patching |
| Durable VBIOS strap edit / MAC forge | Flip Gen-cap bytes 0x40B4B / 0x40F05-3D / 0x40FC5-CB | 100% inside the Davies-Meyer csecret(2) MAC (range 0x2200-0x43C00); keyless forge = 2^128 second preimage; zero gen-strap refs in the unsigned tail |
| Flash modified VBIOS (nvflash / CH341A) | Reflash edited ROM | Ampere RSA signature check rejects; non-booting |

---

## 5. The four-layer wall

The closure. Each layer is independently sufficient and each is empirically closed on silicon.

| Layer | Mechanism | Closed by |
|-------|-----------|-----------|
| 1. Runtime register writes | Driver/host, HS-priv BAR0, HS-priv CSB-mailbox | All fail or re-clamp: config-cap RO, strap unreachable/post-latch, mask poison-walled (host + HS-priv driver writes, HS-priv CSB-mailbox, HS-priv direct mask/OPT_GEN23 write) |
| 2. Register semantics | XVE speed regs are RO no-write-port PHY reflections; 0x85084 re-derived each retrain; OPT_GEN23 hard RO | NVIDIA manual (dev_nv_xve3g_fn0): R-XVF / R-EVF / R---V have no write port at any priv; opening a PLM cannot write a portless register |
| 3. Durable firmware | Gen-cap 100% inside the DM-MAC; keyless forge = 2^128; no outside-MAC Gen byte | Empirical MAC-forge audit vs 92.00.6D.00.0A + A100 SXM4 v45 |
| 4. Silicon fuse | FUSE_PCIE_GEN23_DIS, independent of firmware; gates SerDes downstream of every override | On-card HS-priv XP3G overrides written to Gen3, link stayed Gen1 |

The only path that bypasses all four is a wire-level PCIe retimer forging TS1/TS2 Rate-ID on the
physical lanes (hardware interposer, no firmware dependency).

---

## 6. On-silicon receipts

Compact dmesg from the direct-write probes on a live 170HX. Card healthy at 64GB throughout;
memory + compute unlock unaffected by every PCIe attempt.

| Probe | Evidence | Reading |
|-------|----------|---------|
| HS-priv XP3G PHY-rate override | `PLM[4] XP3G_PLM(0x8e1b0) reg=0xffffffff` ; `XP3G rate=0x00340036 ovr0=0x4` ; `lnksta=0x10410040 speed=1` ; LnkSta Speed 2.5GT/s | PLM opened, override regs writable (rate shows Gen3), link stayed Gen1 -> fuse gates SerDes downstream. Also resolves "bus-route unproven" POSITIVE: the 0x10B9 CSB mailbox reaches the XP3G/PCIe priv block |
| HS-priv FEAT_OVR write + retrain | `BEFORE OPT_GEN23(0x82057c)=0x1 LnkSta=0x10410040` ; `AFTER OPT_GEN23=0x1 (unchanged) FEAT_BASE(0x823800)=0xfffffe8e LnkSta Gen1 AER=0` | FEAT_OVR write took, retrain fired, OPT_GEN23 unmoved -> PCIe FEAT_OVR override-enable is fused OFF |
| HS-priv OPT_GEN23 / mask direct write | `PLM[4] OPT_GEN23(0x82057c) status=0xffff reg=0x1 (write FAILED)` ; `85080/85084=0xbadf1100 poison` ; LnkSta Gen1 | OPT_GEN23 hard RO even to HS-ROP; source + mask access-walled from injection point |

The full community Gen2 sequence (cmpunlocker 0007-pcie-gen2.patch, reconstructed: PRIV_MISC_1
set11/13 clr12/14 + full XP3G OVR/VAL + OPT_GEN23/GEN3 + LINK_CTRL_2 0x001F0002 + PL_LINK_RATE +
VSEC + retrain) as a single combined write was not run: every component is individually proven
inert, so it is a low-odds combination.

---

## 7. Remaining paths + verdict

All remaining paths are physical, equipment-gated, and low-odds.

| Path | Mechanism | Assessment |
|------|-----------|------------|
| PCIe retimer | Astera Aries / TI DS160PR810-class interposer forging TS1/TS2 Rate-ID on the physical lanes | The only path to true >Gen1; no firmware dependency. A plain redriver cannot (the endpoint sources its own fuse-capped TX rate). Hardware interposer + custom board work |
| Leaked prod HULK cert | In-ROM 0xFE504, csecret(40), STRICT_ID_MATCH=NO; a SIGNED override of FUSE_FEATURE_OVERRIDE 0x823800, sidestepping the 2^128 forge | Gated by RmActivateHulk fmodel-flag (false on prod -> likely rejected); needs the cert files; and on-card FEAT_OVR writes do not move OPT_GEN23 anyway. Largely mooted |
| csecret(6)/(2) fault-injection | Fault-injection glitcher (EM / voltage) to decrypt imem_sec / forge the MAC | ~$400-2k, weeks, no guarantee; and STILL fuse-bound for PCIe afterward. Fault-injection tooling validated offline if equipment is acquired |

**Bottom line:** PCIe-Gen1 on the 170HX is a double-locked (signed devinit strap + OTP fuse)
hardware wall. Every reachable software and keyless-firmware lever is closed, verified on two
independent surfaces (a 4032-run offline firmware fuzz sweep and on-silicon direct-write probing).
Real >Gen1 requires a wire-level PCIe retimer or an Ampere Falcon signing break, and even a
signing break is still fuse-bound. Memory (64GB) and compute (12.71 TFLOPS) are separate,
already-won, durable locks and are unaffected. The card is stable and pristine at 64GB Gen1.

---

## 8. Verification

The conclusion is cross-confirmed on multiple independent surfaces, not a single tool's output:

- An offline per-bit firmware fuzz sweep (66 functions x 126 (function,register) pairs x 32
  single-bit values = 4032 runs) showing no firmware path re-derives a lock register.
- A GSP-RM emulator validated bit-exact against a live card's boot trace (reproduces the WPR1
  bounds 0xFF7400000 / 0xFFFF00000 from the on-card SEC2 trace; the reverse-engineering matches
  silicon behavior).
- Independent Falcon-emulator execution of the SEC2 booter path (cmpunlocker booter_emu /
  booter_secure), and independent adversarial review across the MAC-forge, bootrom/CVE, and
  fuse-override arguments.
- Independent community research (170th-street) finding the same four software paths dead.
- On-card direct-write probing on a live 170HX (HS-priv XP3G override, FEAT_OVR write with
  retrain, OPT_GEN23 / mask direct write).

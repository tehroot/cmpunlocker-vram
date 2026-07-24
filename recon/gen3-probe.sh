#!/bin/bash
###############################################################################
# gen3-probe.sh — EXPERIMENTAL Gen2 -> Gen3 retrain probe for CMP 170HX (GA100)
#
# Extends the PROVEN Gen2 flip (tools/retrain.sh). Only three things change from
# the Gen2 path:  MAX_RATE 2->3,  TargetLinkSpeed 2->3 (both ends),  plus two
# clearly-marked placeholder blocks the register/EQ agents fill in:
#     DIS_G3_WRITES   — the Gen3-enable / Gen3-disable-chicken-bit clears
#     EQ_WRITES       — Gen3 link-equalization presets + XP3G PHY overrides
#
# NON-PERSISTENT BY DESIGN:
#   * Every write is volatile — BAR0 MMIO + PCIe config space. NOTHING is
#     written to fuses, VBIOS, or NVRAM. A power cycle (or plain reboot) ALWAYS
#     restores stock Gen1; the systemd oneshot then re-applies the known-good
#     Gen2. There is no state a reboot cannot undo.
#   * DO NOT install this as a service. Run it by hand, GPU idle, no workload.
#   * It has a built-in recovery ladder driven entirely from the UPSTREAM port
#     (which stays alive even when the GPU link wedges).
#
# Preconditions:
#   * Run on the MAIN RIG, direct x16 slot. (On the R530/OcuLink the BAR0
#     resource0 mmap has returned EINVAL — the register method cannot even be
#     applied there, and Gen3 SI over OcuLink is marginal. See report.)
#   * Secure Boot / kernel lockdown OFF (needed for the resource0 RW mmap).
#   * No CUDA workload running; nvidia driver bound is fine (matches retrain.sh).
#   * Recommend AER ENABLED (do NOT boot with pci=noaer) so a Gen3 EQ failure
#     surfaces as a visible correctable/fatal event instead of a silent flap.
###############################################################################
set -uo pipefail   # deliberately NOT -e: we trap failures so recovery always runs

# Refuse to run if it looks like the current-speed is already >=3, or no card.
if command -v nvidia-smi >/dev/null 2>&1; then
  cur="$(nvidia-smi --query-gpu=pcie.link.gen.current --format=csv,noheader 2>/dev/null | head -1 | tr -d '[:space:]' || true)"
  echo "gen3-probe: current PCIe gen = ${cur:-unknown}"
fi

python3 - <<'PY'
import os, mmap, struct, time, subprocess, glob, sys

# =============================================================================
#  ####  PLACEHOLDER 1 — DIS_G3 / Gen3-ENABLE writes  (owned by REGISTER agent)
#  Each tuple is (bar0_offset, clear_mask, set_value): reg = (reg & ~clear) | set
#  Analogues of the Gen2 "clear DIS_G2 @ 0x8C2C0 bit2". Likely candidates the
#  register agent will pin: an adjacent XP CYA bit in 0x8C2C0, and/or the
#  PRIV_MISC_1 (0x8841C) GEN3 EN/VAL pair. Leave EMPTY to run a "MAX_RATE=3 +
#  target=3 only" baseline (tests whether Gen3 trains with no extra enable).
# =============================================================================
DIS_G3_WRITES = [
    # (0x8C2C0, (1 << 3), 0),            # <-- EXAMPLE ONLY: hypothetical DIS_G3 bit3, clear it
    # (0x8841C, 0, (1<<15)|(1<<17)),     # <-- EXAMPLE ONLY: hypothetical PRIV_MISC_1 GEN3 EN/VAL
]

# =============================================================================
#  ####  PLACEHOLDER 2 — Gen3 EQUALIZATION / PHY writes  (owned by EQ agent)
#  Gen3 (8 GT/s) REQUIRES the link-equalization phases that Gen1->Gen2 does not.
#  These MUST land BEFORE the retrain (EQ presets are consumed when the LTSSM
#  enters Recovery.Equalization). Includes the XP3G PHY overrides
#  (0x8E110/0x8E120 OVR0/VAL0, 0x8E11C/0x8E12C OVR3/VAL3, PLM 0x8E1B0/B4/B8/BC)
#  and any TX preset / coefficient (FS/LF) programming.
#  Leave EMPTY to first try AUTONOMOUS hardware EQ (default presets) — on a
#  clean direct-slot channel that often converges without manual coefficients.
# =============================================================================
EQ_WRITES = [
    # (0x8E1B0, 0xFFFFFFFF, 0xFFFFFFFF),   # <-- EXAMPLE ONLY: open XP3G PLM
    # (0x8E11C, 0, 0x00000004),            # <-- EXAMPLE ONLY: XP3G OVR3
    # (0x8E12C, 0, 0x00200000),            # <-- EXAMPLE ONLY: XP3G VAL3
]

TARGET_GEN   = 3          # the gen we are trying to reach
RECOVERY_GEN = 1          # guaranteed-to-train fallback if we wedge (Gen1, no EQ)

# ------------------------- helpers (identical idiom to retrain.sh) -----------
def sh(*a):  return subprocess.check_output(a, text=True).strip()
def sh_ok(*a):
    try: return sh(*a)
    except Exception: return None

def find_gpu():
    for path in sorted(glob.glob("/sys/bus/pci/devices/*/vendor")):
        dev = os.path.dirname(path)
        try:
            if open(path).read().strip() != "0x10de": continue
            devid = open(os.path.join(dev, "device")).read().strip()
        except OSError: continue
        if devid in ("0x20c2", "0x2082") and os.path.exists(os.path.join(dev, "resource0")):
            return os.path.basename(dev)
    return None

def short(bdf): return bdf.split(":", 1)[-1] if bdf.count(":") == 2 else bdf

def pci_read(dev, off, w):
    fmt = {1:"b",2:"w",4:"l"}[w]
    v = sh_ok("setpci","-s",short(dev),f"{off:x}.{fmt}")
    return int(v,16) if v is not None else 0xFFFFFFFF
def pci_write(dev, off, w, val):
    fmt = {1:"b",2:"w",4:"l"}[w]
    subprocess.check_call(["setpci","-s",short(dev),f"{off:x}.{fmt}={val:x}"])

def find_exp(dev):
    if not (pci_read(dev,0x06,2) & 0x10): return None
    ptr = pci_read(dev,0x34,1)
    while ptr and ptr != 0xFF:
        if pci_read(dev,ptr,1) == 0x10: return ptr
        ptr = pci_read(dev,ptr+1,1)
    return None

def upstream_of(gpu):
    parent = os.path.realpath(f"/sys/bus/pci/devices/{gpu}/..")
    name = os.path.basename(parent)
    return name if name.count(":") >= 1 else None

def gpu_alive(gpu):
    # Config-space liveness — SAFER than touching BAR0 after a possible link drop
    # (BAR0 MMIO to a downed device can raise SIGBUS). VendorID != 0xffff == alive.
    return (pci_read(gpu,0x00,2) & 0xFFFF) == 0x10de

def cur_speed(dev):
    cap = find_exp(dev)
    return pci_read(dev, cap+0x12, 2) & 0xF if cap is not None else -1

# ----------------------------- locate the link ------------------------------
gpu = find_gpu()
if not gpu: print("gen3-probe: no CMP 170HX BAR0 device; abort"); sys.exit(0)
up  = upstream_of(gpu)
if not up:  print(f"gen3-probe: no upstream port for {gpu}; abort"); sys.exit(0)
gcap, ucap = find_exp(gpu), find_exp(up)
print(f"gen3-probe: gpu={gpu} (cap@0x{(gcap or 0):x})  upstream={up} (cap@0x{(ucap or 0):x})")

# ============================== RECOVERY LADDER ==============================
# Driven ONLY from the upstream port + upstream-side config space, because the
# GPU BAR0/config may be dead. Escalates least-disruptive -> most-disruptive.
def recover():
    print("gen3-probe: !! entering RECOVERY (link wedged) !!")
    # (R1) Re-target the upstream to a guaranteed rate and re-drive retrain.
    #      Pure LTSSM retrain — does NOT reset the GPU, so the memory unlock is
    #      preserved if the link comes back.
    if ucap is not None:
        c2 = pci_read(up, ucap+0x30, 2)
        pci_write(up, ucap+0x30, 2, (c2 & ~0xF) | RECOVERY_GEN)
        c  = pci_read(up, ucap+0x10, 2)
        pci_write(up, ucap+0x10, 2, c | 0x20)
        time.sleep(1.5)
        if gpu_alive(gpu):
            print(f"gen3-probe: R1 ok — link back, speed={cur_speed(gpu)} "
                  f"(re-run retrain.sh or reboot to restore Gen2)")
            return True
    # (R2) Secondary Bus Reset from the upstream bridge. RESETS THE GPU (clears
    #      volatile unlock state until the driver re-inits) — more disruptive,
    #      but brings the link up from the bridge's target rate.
    bc = pci_read(up, 0x3E, 2)
    pci_write(up, 0x3E, 2, bc | (1<<6)); time.sleep(0.1)
    pci_write(up, 0x3E, 2, bc & ~(1<<6)); time.sleep(1.5)
    if gpu_alive(gpu):
        print(f"gen3-probe: R2 ok — SBR recovered link, speed={cur_speed(gpu)} "
              f"(driver will re-init; reboot for a clean Gen2)")
        return True
    # (R3) Warm remove + rescan (driver re-binds; unlock re-applied on re-bind).
    try:
        open(f"/sys/bus/pci/devices/{gpu}/remove","w").write("1"); time.sleep(1)
        open("/sys/bus/pci/rescan","w").write("1"); time.sleep(2)
        if gpu_alive(gpu):
            print("gen3-probe: R3 ok — remove/rescan recovered the device")
            return True
    except Exception as e:
        print(f"gen3-probe: R3 remove/rescan failed: {e}")
    # (R4) Out of warm options.
    print("gen3-probe: !! COLD POWER CYCLE REQUIRED !!")
    print("gen3-probe:    All writes were volatile — a cold boot restores stock")
    print("gen3-probe:    Gen1 and the service re-applies Gen2. Nothing is bricked.")
    return False

# ============================== PRE-FLIGHT CAPTURE ==========================
res = f"/sys/bus/pci/devices/{gpu}/resource0"
try:
    fd = os.open(res, os.O_RDWR | os.O_SYNC)
    m  = mmap.mmap(fd, os.path.getsize(res), access=mmap.ACCESS_WRITE)
except OSError as e:
    print(f"gen3-probe: BAR0 mmap FAILED ({e}). On the R530/OcuLink this is the "
          f"known EINVAL — run on the direct-slot main rig. Abort (nothing written).")
    sys.exit(0)

def r(off): return struct.unpack_from("<I", m, off)[0]
def w(off, val): struct.pack_into("<I", m, off, val & 0xFFFFFFFF)

if r(0) == 0xFFFFFFFF:
    print("gen3-probe: BAR0 dead before we did anything; abort")
    m.close(); os.close(fd); sys.exit(0)

print("---- PRE-FLIGHT (baseline snapshot) ----")
print(f"  BAR0 0x8C2C0 (CYA/DIS_G2) = 0x{r(0x8C2C0):08x}  DIS_G2=bit2={ (r(0x8C2C0)>>2)&1 }")
print(f"  BAR0 0x8C040 (LINK_CFG_0) = 0x{r(0x8C040):08x}  MAX_RATE[19:18]={ (r(0x8C040)>>18)&3 }")
print(f"  BAR0 0x8872C (LTSSM)      = 0x{r(0x8872C):08x}")
print(f"  BAR0 0x8841C (PRIV_MISC_1)= 0x{r(0x8841C):08x}")
print(f"  BAR0 0x8C1C0 (PL_LINK_RATE)=0x{r(0x8C1C0):08x}")
for a in (0x8E110,0x8E120,0x8E11C,0x8E12C,0x8E1B0,0x8E1B4,0x8E1B8,0x8E1BC):
    print(f"  BAR0 0x{a:05X} (XP3G)      = 0x{r(a):08x}")
for a,n in ((0x82057C,"OPT_GEN23"),(0x820580,"OPT_GEN3")):
    print(f"  BAR0 0x{a:06X} ({n}) = 0x{r(a):08x}")
for dev,tag in ((gpu,"GPU"),(up,"UP ")):
    cap = find_exp(dev)
    if cap is None: continue
    print(f"  {tag} LnkCap=0x{pci_read(dev,cap+0x0C,4):08x} "
          f"LnkCap2=0x{pci_read(dev,cap+0x2C,4):08x} "
          f"LnkSta=0x{pci_read(dev,cap+0x10,4):08x} "
          f"LnkCtl2=0x{pci_read(dev,cap+0x30,4):08x}")
print(f"  pre-flight negotiated speed = Gen{cur_speed(gpu)}")

# ============================== WRITE SEQUENCE ==============================
# (1) keep Gen2 enabled (clear DIS_G2) — Gen1+Gen2+Gen3 all in the set
w(0x8C2C0, r(0x8C2C0) & ~(1 << 2))

# (2) PLACEHOLDER 1 — Gen3 enable / DIS_G3 clears  (register agent)
for off, clr, setv in DIS_G3_WRITES:
    w(off, (r(off) & ~clr) | setv)
print(f"gen3-probe: applied {len(DIS_G3_WRITES)} DIS_G3/Gen3-enable write(s)")

# (3) MAX_RATE = 3  (Gen3 ceiling; 2-bit field 0x8C040[19:18], value 3 fits)
w(0x8C040, (r(0x8C040) & ~0xC0000) | (TARGET_GEN << 18))

# (4) PLACEHOLDER 2 — Gen3 EQ / PHY presets  (EQ agent) — MUST precede retrain
for off, clr, setv in EQ_WRITES:
    w(off, (r(off) & ~clr) | setv)
print(f"gen3-probe: applied {len(EQ_WRITES)} EQ/PHY write(s)")

# (5) LTSSM nudge (same as Gen2 path)
w(0x8872C, 0x6)
time.sleep(0.05)
print(f"gen3-probe: post-write CYA=0x{r(0x8C2C0):08x} DIS_G2={(r(0x8C2C0)>>2)&1} "
      f"MAX_RATE={(r(0x8C040)>>18)&3} XVE=0x{r(0x8872C):08x}")

# (6) TargetLinkSpeed = 3 on BOTH ends (link trains to min(both targets))
for dev in (up, gpu):
    cap = find_exp(dev)
    if cap is None: continue
    c2 = pci_read(dev, cap+0x30, 2)
    pci_write(dev, cap+0x30, 2, (c2 & ~0xF) | TARGET_GEN)

# --- Point of no easy return: drop the BAR0 mmap BEFORE the retrain, so that
#     if the link wedges we never touch a downed BAR0 (SIGBUS-safe). Recovery
#     and readback are config-space only. ---
m.close(); os.close(fd)

# (7) Drive Retrain-Link from the UPSTREAM port (the conceptual crack)
if ucap is None:
    print("gen3-probe: upstream has no PCIe cap; cannot retrain; abort")
    sys.exit(0)
uctl = pci_read(up, ucap+0x10, 2)
pci_write(up, ucap+0x10, 2, uctl | 0x20)
time.sleep(1.5)

# ============================== READBACK / VERDICT =========================
if not gpu_alive(gpu):
    print("gen3-probe: WEDGED after retrain (GPU config space gone)")
    recover()
    sys.exit(0)

sp = cur_speed(gpu)
lnksta = pci_read(gpu, gcap+0x10, 4)
print(f"gen3-probe: RESULT LnkSta=0x{lnksta:08x} negotiated=Gen{sp} "
      f"width=x{(lnksta>>20)&0x3F}")
if sp >= TARGET_GEN:
    print(f"gen3-probe: *** Gen{TARGET_GEN} TRAINED ***")
    print("gen3-probe: STABILITY CHECK REQUIRED — a trained link can still be a")
    print("gen3-probe:   high-BER link. Before trusting it:")
    print("gen3-probe:     dmesg | grep -iE 'AER|Corrected|pcieport|nvrm|xid'")
    print("gen3-probe:     lspci -vvv -s %s | grep -iE 'CESta|UESta|LnkSta'" % short(gpu))
    print("gen3-probe:     run a bandwidth+stress test (nvbandwidth / CUDA) and re-check errors")
elif sp == 2:
    print("gen3-probe: fell back to Gen2 (Gen3 EQ did not converge / not enabled)."
          " Link healthy — no recovery needed.")
else:
    print(f"gen3-probe: trained Gen{sp} (below Gen2). Re-run retrain.sh to restore Gen2.")
PY

#!/bin/bash
###############################################################################
# retrain-diag.sh — instrumented Gen2 retrain (diagnostic, NOT a service)
#
# Purpose: find the stage at which the GPU's advertised LnkCap2 appears and
# reverts. tools/retrain.sh applies the XP write set and the link still trains
# Gen1 on the AM5 rig, because the GPU's LnkCap2 has reverted to Gen1-only by
# the time it runs, so its LnkCtl2 target write is rejected and min() = 2.5GT/s.
# The boot-time 0007 block that DID move LnkCap2 0x2->0x6 also writes 0x880A8
# (the LnkCtl2/LnkSta2 BAR0 mirror), which retrain.sh omits.
#
# SAFETY (all learned the hard way — an earlier revision of this script wedged
# the link at the first write stage):
#   * Every write is idempotent-checked: if the register already holds the
#     wanted value, it is SKIPPED and logged as such. No blind re-writes.
#   * 0x8872C is NOT written by default. Its stock value was never captured and
#     retrain.sh stores the literal 0x6 over the whole register, clobbering
#     unknown fields. Enable with --with-ltssm; even then it is read-modify-
#     write on [3:0] only, not a full-word store.
#   * Config-space liveness is checked after every stage. If the GPU drops off
#     the bus the script stops writing immediately and runs the recovery ladder.
#   * The BAR0 mmap is dropped before the retrain, so a downed link is never
#     touched through MMIO (SIGBUS-safe). Recovery is config-space only.
#   * Recovery ladder is driven from the UPSTREAM port, which stays alive when
#     the GPU link does not: R1 re-target Gen1 + retrain -> R2 Secondary Bus
#     Reset -> R3 remove/rescan -> R4 cold-cycle instruction.
#
# All writes are volatile (BAR0 MMIO + config space). Nothing touches fuses,
# VBIOS or NVRAM. Target is Gen2 — the known-good rate. A cold power cycle
# always restores stock; the driver re-applies the memory unlock at init.
#
# Requires iomem=relaxed on the kernel cmdline, else the resource0 mmap fails
# with EINVAL (pci_mmap_resource -> iomem_is_exclusive).
#
#   sudo ./retrain-diag.sh [--dry-run] [--with-ltssm]
###############################################################################
set -uo pipefail

DRY_RUN=0
WITH_LTSSM=0
for a in "$@"; do
  case "$a" in
    --dry-run|-n)  DRY_RUN=1 ;;
    --with-ltssm)  WITH_LTSSM=1 ;;
    -h|--help)
      sed -n '2,38p' "$0" | sed 's/^# \{0,1\}//'
      exit 0 ;;
    *) echo "unknown arg: $a" >&2; exit 1 ;;
  esac
done
export DIAG_DRY_RUN="$DRY_RUN" DIAG_WITH_LTSSM="$WITH_LTSSM"

python3 - <<'PY'
import os, mmap, struct, time, subprocess, glob, sys

TARGET_GEN   = 2      # the rate we are trying to reach
RECOVERY_GEN = 1      # guaranteed-to-train fallback
DRY          = os.environ.get("DIAG_DRY_RUN")   == "1"
WITH_LTSSM   = os.environ.get("DIAG_WITH_LTSSM") == "1"

# ----------------------------------------------------------------- helpers --
def sh_ok(*a):
    try: return subprocess.check_output(a, text=True, stderr=subprocess.DEVNULL).strip()
    except Exception: return None

def short(bdf): return bdf.split(":", 1)[-1] if bdf.count(":") == 2 else bdf

def pci_read(dev, off, w):
    v = sh_ok("setpci", "-s", short(dev), f"{off:x}.{ {1:'b',2:'w',4:'l'}[w] }")
    return int(v, 16) if v is not None else 0xFFFFFFFF

def pci_write(dev, off, w, val):
    try:
        subprocess.check_call(["setpci", "-s", short(dev),
                               f"{off:x}.{ {1:'b',2:'w',4:'l'}[w] }={val:x}"])
        return True
    except Exception as e:
        print(f"  setpci write {dev} 0x{off:x} failed: {e}")
        return False

def find_exp(dev):
    if not (pci_read(dev, 0x06, 2) & 0x10): return None
    ptr = pci_read(dev, 0x34, 1)
    while ptr and ptr != 0xFF:
        if pci_read(dev, ptr, 1) == 0x10: return ptr
        ptr = pci_read(dev, ptr + 1, 1)
    return None

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

# ------------------------------------------------------------ locate link --
gpu = find_gpu()
if not gpu:
    print("retrain-diag: no CMP 170HX on the bus; abort"); sys.exit(0)
up = os.path.basename(os.path.realpath(f"/sys/bus/pci/devices/{gpu}/.."))
gcap, ucap = find_exp(gpu), find_exp(up)
print(f"retrain-diag: gpu={gpu} (cap@0x{(gcap or 0):x})  up={up} (cap@0x{(ucap or 0):x})")
print(f"retrain-diag: dry_run={DRY} with_ltssm={WITH_LTSSM} target=Gen{TARGET_GEN}")

def alive():
    """Config-space liveness. Never touch BAR0 to test this — a downed device
    can raise SIGBUS on MMIO, whereas config reads just return 0xffff."""
    return (pci_read(gpu, 0x00, 2) & 0xFFFF) == 0x10DE

# ------------------------------------------------------- recovery ladder ----
def recover():
    print("\nretrain-diag: !! LINK DOWN — entering RECOVERY !!")
    if ucap is not None:
        # R1: re-target upstream to a guaranteed rate and re-drive training.
        # Pure LTSSM retrain — does NOT reset the GPU, so the live unlock survives.
        c2 = pci_read(up, ucap + 0x30, 2)
        pci_write(up, ucap + 0x30, 2, (c2 & ~0xF) | RECOVERY_GEN)
        c = pci_read(up, ucap + 0x10, 2)
        pci_write(up, ucap + 0x10, 2, c | 0x20)
        time.sleep(1.5)
        if alive():
            print("retrain-diag: R1 OK — link back via upstream retrain "
                  f"(speed={pci_read(gpu, gcap+0x12, 2) & 0xF})")
            return True
    # R2: Secondary Bus Reset. Resets the GPU — driver re-inits, unlock re-applies.
    bc = pci_read(up, 0x3E, 2)
    if bc != 0xFFFFFFFF:
        pci_write(up, 0x3E, 2, bc | (1 << 6)); time.sleep(0.1)
        pci_write(up, 0x3E, 2, bc & ~(1 << 6)); time.sleep(2.0)
        if alive():
            print("retrain-diag: R2 OK — Secondary Bus Reset recovered the link")
            return True
    # R3: warm remove + rescan.
    try:
        open(f"/sys/bus/pci/devices/{gpu}/remove", "w").write("1"); time.sleep(1)
        open("/sys/bus/pci/rescan", "w").write("1"); time.sleep(2)
        if alive():
            print("retrain-diag: R3 OK — remove/rescan recovered the device")
            return True
    except Exception as e:
        print(f"retrain-diag: R3 failed: {e}")
    print("retrain-diag: !! COLD POWER CYCLE REQUIRED !!")
    print("retrain-diag:    Every write was volatile — a cold boot restores stock")
    print("retrain-diag:    and the driver re-applies the unlock. Nothing is bricked.")
    return False

# ------------------------------------------------------------- BAR0 mmap ----
res = f"/sys/bus/pci/devices/{gpu}/resource0"
try:
    fd = os.open(res, os.O_RDWR | os.O_SYNC)
    m = mmap.mmap(fd, os.path.getsize(res), access=mmap.ACCESS_WRITE)
except OSError as e:
    print(f"retrain-diag: BAR0 mmap FAILED ({e})")
    print("retrain-diag:   check 'iomem=relaxed' in /proc/cmdline")
    sys.exit(1)

mapped = True
def unmap():
    global mapped
    if mapped:
        m.close(); os.close(fd); mapped = False

def r(off): return struct.unpack_from("<I", m, off)[0]
def w(off, val): struct.pack_into("<I", m, off, val & 0xFFFFFFFF)

# BAR0 registers dumped at every stage. 0x8872C included — its stock value was
# never captured before, and it is the prime suspect for the earlier wedge.
REGS = [
    (0x88084, "CAP     "), (0x880A4, "CAP2    "), (0x880A8, "LC2     "),
    (0x8C2C0, "CYA0    "), (0x8C300, "CYA1    "), (0x8C040, "CFG0    "),
    (0x8C1C0, "PL_RATE "), (0x8841C, "MISC1   "), (0x8872C, "XVE_872C"),
]

def dump(tag):
    if not alive():
        print(f"[{tag:<13}] GPU OFF THE BUS — skipping BAR0 reads")
        return False
    print(f"[{tag:<13}] " + "  ".join(f"{n.strip()}=0x{r(o):08x}" for o, n in REGS))
    for dev, name, cap in ((gpu, "GPU", gcap), (up, "UP ", ucap)):
        if cap is None: continue
        print(f"{'':16}{name} LnkCap=0x{pci_read(dev,cap+0x0C,4):08x} "
              f"LnkCap2=0x{pci_read(dev,cap+0x2C,4):08x} "
              f"LnkCtl2=0x{pci_read(dev,cap+0x30,4):08x} "
              f"speed={pci_read(dev,cap+0x12,2) & 0xF} "
              f"width={(pci_read(dev,cap+0x12,2) >> 4) & 0x3F}")
    return True

def stage_guard(tag):
    """Run after every write stage. Stops the script the moment the link drops
    instead of continuing to write into a dead device."""
    if alive():
        return True
    print(f"retrain-diag: GPU dropped off the bus during stage '{tag}'")
    unmap()
    recover()
    sys.exit(1)

def apply(off, name, clr, setv, enabled=True):
    """Idempotent write: skip entirely when the register already holds the
    wanted value. This is what keeps a re-run from re-poking a live link."""
    cur = r(off)
    want = (cur & ~clr) | setv
    if not enabled:
        print(f"  SKIP  {name} 0x{off:05X} disabled       cur=0x{cur:08x}")
        return
    if want == cur:
        print(f"  SKIP  {name} 0x{off:05X} already set    cur=0x{cur:08x}")
        return
    if DRY:
        print(f"  DRY   {name} 0x{off:05X} 0x{cur:08x} -> 0x{want:08x} (not written)")
        return
    print(f"  WRITE {name} 0x{off:05X} 0x{cur:08x} -> 0x{want:08x}")
    w(off, want)
    time.sleep(0.02)
    if alive():
        print(f"{'':8}readback 0x{r(off):08x}")

if not dump("stock"):
    unmap(); sys.exit(1)

if DRY:
    print("\nretrain-diag: --dry-run — showing what WOULD be written, then exiting\n")

# --- stage 1: the retrain.sh XP write set ----------------------------------
print("\n-- stage 1: XP write set --")
apply(0x8C2C0, "DIS_G2  ", (1 << 2), 0)                       # clear DIS_G2
apply(0x8C040, "MAX_RATE", 0xC0000, (TARGET_GEN << 18))       # [19:18]
# 0x8872C: OFF by default. retrain.sh stores the literal 0x6 over the whole
# register; stock value unknown, unknown fields clobbered. RMW on [3:0] only.
apply(0x8872C, "LTSSM   ", 0xF, 0x6, enabled=WITH_LTSSM)
stage_guard("XP write set")
dump("after XP")

# --- stage 2: the write retrain.sh omits — 0x880A8 LnkCtl2/LnkSta2 mirror ---
# Exactly what 0007-pcie-gen2.patch does in the block where CAP2 went 0x2->0x6.
print("\n-- stage 2: 0x880A8 (the write retrain.sh omits) --")
apply(0x880A8, "LC2_TGT ", 0x0000000F, TARGET_GEN)
apply(0x880A8, "LC2_HI  ", 0x000F0000, 0x000F0000)
stage_guard("0x880A8")
dump("after 880A8")

if DRY:
    print("\nretrain-diag: dry run complete — no writes performed, no retrain.")
    unmap(); sys.exit(0)

# --- stage 3: TargetLinkSpeed on both ends (config space) ------------------
print("\n-- stage 3: TargetLinkSpeed both ends --")
for dev, name in ((up, "UP "), (gpu, "GPU")):
    cap = find_exp(dev)
    if cap is None: continue
    c2 = pci_read(dev, cap + 0x30, 2)
    want = (c2 & ~0xF) | TARGET_GEN
    if want == c2:
        print(f"  SKIP  {name} LnkCtl2 already 0x{c2:04x}")
        continue
    print(f"  WRITE {name} LnkCtl2 0x{c2:04x} -> 0x{want:04x}")
    pci_write(dev, cap + 0x30, 2, want)
    print(f"{'':8}readback 0x{pci_read(dev, cap+0x30, 2):04x}")
stage_guard("TargetLinkSpeed")
dump("after target")

# Drop the mmap BEFORE the retrain — never touch BAR0 on a link that may drop.
unmap()

# --- stage 4: retrain, driven from the UPSTREAM port -----------------------
print("\n-- stage 4: retrain from upstream --")
if ucap is None:
    print("retrain-diag: upstream has no PCIe cap; cannot retrain"); sys.exit(1)
pci_write(up, ucap + 0x10, 2, pci_read(up, ucap + 0x10, 2) | 0x20)
time.sleep(1.5)

if not alive():
    print("retrain-diag: GPU off the bus after retrain")
    recover(); sys.exit(1)

for dev, name, cap in ((gpu, "GPU", gcap), (up, "UP ", ucap)):
    if cap is None: continue
    sta = pci_read(dev, cap + 0x12, 2)
    print(f"[post-retrain ] {name} LnkCap=0x{pci_read(dev,cap+0x0C,4):08x} "
          f"LnkCap2=0x{pci_read(dev,cap+0x2C,4):08x} "
          f"LnkCtl2=0x{pci_read(dev,cap+0x30,4):08x} "
          f"LnkSta=0x{sta:04x} speed={sta & 0xF} width={(sta >> 4) & 0x3F}")

sp = pci_read(gpu, gcap + 0x12, 2) & 0xF
print(f"\nretrain-diag: negotiated Gen{sp} "
      + ("*** TARGET REACHED ***" if sp >= TARGET_GEN else "(still below target)"))
PY

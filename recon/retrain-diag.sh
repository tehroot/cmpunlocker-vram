#!/bin/bash
###############################################################################
# retrain-diag.sh — instrumented Gen2 retrain (diagnostic, NOT a service)
#
# Same write set as tools/retrain.sh PLUS the 0x880A8 (LnkCtl2/LnkSta2 BAR0
# mirror) write that the boot-time 0007 patch does and retrain.sh omits — the
# only write present where LnkCap2 moved to 0x6 and absent where it did not.
#
# Dumps CAP/CAP2/LC2 (BAR0 mirrors) + both ends' config-space LnkCap2/LnkCtl2
# at every stage, so the stage at which the advertised cap appears/reverts is
# visible instead of inferred.
#
# All writes are volatile (BAR0 MMIO + config space). Nothing touches fuses,
# VBIOS or NVRAM. Target is Gen2 — the known-good rate — so the worst case is
# the link staying at Gen1, which is where it already is.
#
# Requires: iomem=relaxed on the kernel cmdline (else the resource0 mmap
# returns EINVAL via pci_mmap_resource -> iomem_is_exclusive).
###############################################################################
set -uo pipefail

python3 - <<'PY'
import os, mmap, struct, time, subprocess, glob, sys

TARGET_GEN = 2

def sh_ok(*a):
    try: return subprocess.check_output(a, text=True).strip()
    except Exception: return None

def short(bdf): return bdf.split(":", 1)[-1] if bdf.count(":") == 2 else bdf

def pci_read(dev, off, w):
    v = sh_ok("setpci", "-s", short(dev), f"{off:x}.{ {1:'b',2:'w',4:'l'}[w] }")
    return int(v, 16) if v is not None else 0xFFFFFFFF

def pci_write(dev, off, w, val):
    subprocess.check_call(["setpci", "-s", short(dev),
                           f"{off:x}.{ {1:'b',2:'w',4:'l'}[w] }={val:x}"])

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

gpu = find_gpu()
if not gpu:
    print("retrain-diag: no CMP 170HX; abort"); sys.exit(0)
up = os.path.basename(os.path.realpath(f"/sys/bus/pci/devices/{gpu}/.."))
gcap, ucap = find_exp(gpu), find_exp(up)
print(f"retrain-diag: gpu={gpu} (cap@0x{(gcap or 0):x})  up={up} (cap@0x{(ucap or 0):x})")

res = f"/sys/bus/pci/devices/{gpu}/resource0"
try:
    fd = os.open(res, os.O_RDWR | os.O_SYNC)
    m = mmap.mmap(fd, os.path.getsize(res), access=mmap.ACCESS_WRITE)
except OSError as e:
    print(f"retrain-diag: BAR0 mmap FAILED ({e}) — check iomem=relaxed in /proc/cmdline")
    sys.exit(1)

def r(off): return struct.unpack_from("<I", m, off)[0]
def w(off, val): struct.pack_into("<I", m, off, val & 0xFFFFFFFF)

def dump(tag):
    """BAR0 config-mirrors + both ends' real config space."""
    line = (f"[{tag:<14}] BAR0 CAP=0x{r(0x88084):08x} CAP2=0x{r(0x880A4):08x} "
            f"LC2=0x{r(0x880A8):08x} | CYA0=0x{r(0x8C2C0):08x} "
            f"CFG0=0x{r(0x8C040):08x} PL=0x{r(0x8C1C0):08x} MISC1=0x{r(0x8841C):08x}")
    print(line)
    for dev, name, cap in ((gpu, "GPU", gcap), (up, "UP ", ucap)):
        if cap is None: continue
        print(f"                 {name} cfg LnkCap=0x{pci_read(dev,cap+0x0C,4):08x} "
              f"LnkCap2=0x{pci_read(dev,cap+0x2C,4):08x} "
              f"LnkCtl2=0x{pci_read(dev,cap+0x30,4):08x} "
              f"speed={pci_read(dev,cap+0x12,2) & 0xF}")

if r(0) == 0xFFFFFFFF:
    print("retrain-diag: BAR0 dead; abort"); m.close(); os.close(fd); sys.exit(1)

dump("stock")

# --- stage 1: the retrain.sh write set -------------------------------------
w(0x8C2C0, r(0x8C2C0) & ~(1 << 2))          # clear DIS_G2
w(0x8C040, (r(0x8C040) & ~0xC0000) | (TARGET_GEN << 18))
w(0x8872C, 0x6)
time.sleep(0.05)
dump("after XP")

# --- stage 2: THE MISSING WRITE — 0x880A8 LnkCtl2/LnkSta2 mirror -----------
# Exactly what 0007-pcie-gen2.patch does in the block where CAP2 went 0x2->0x6.
lc2 = r(0x880A8)
lc2 = (lc2 & ~0x0000000F) | TARGET_GEN
lc2 = (lc2 & ~0x000F0000) | 0x000F0000
w(0x880A8, lc2)
time.sleep(0.05)
dump("after 880A8")

# --- stage 3: TargetLinkSpeed on both ends (config space) ------------------
for dev in (up, gpu):
    cap = find_exp(dev)
    if cap is None: continue
    c2 = pci_read(dev, cap + 0x30, 2)
    pci_write(dev, cap + 0x30, 2, (c2 & ~0xF) | TARGET_GEN)
dump("after target")

# Drop the mmap before the retrain — never touch BAR0 on a link that may drop.
m.close(); os.close(fd)

# --- stage 4: retrain, driven from the UPSTREAM port -----------------------
if ucap is None:
    print("retrain-diag: upstream has no PCIe cap; abort"); sys.exit(1)
pci_write(up, ucap + 0x10, 2, pci_read(up, ucap + 0x10, 2) | 0x20)
time.sleep(1.5)

alive = (pci_read(gpu, 0x00, 2) & 0xFFFF) == 0x10de
print(f"retrain-diag: gpu alive after retrain: {alive}")
for dev, name, cap in ((gpu, "GPU", gcap), (up, "UP ", ucap)):
    if cap is None: continue
    print(f"[post-retrain  ] {name} cfg LnkCap=0x{pci_read(dev,cap+0x0C,4):08x} "
          f"LnkCap2=0x{pci_read(dev,cap+0x2C,4):08x} "
          f"LnkCtl2=0x{pci_read(dev,cap+0x30,4):08x} "
          f"LnkSta=0x{pci_read(dev,cap+0x12,2):04x} "
          f"speed={pci_read(dev,cap+0x12,2) & 0xF}")
PY

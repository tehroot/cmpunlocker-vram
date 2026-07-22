#!/usr/bin/env bash
###############################################################################
# probe-170hx.sh — READ-ONLY reconnaissance for an NVIDIA CMP 170HX (GA100)
#
# This script MODIFIES NOTHING. It performs only reads:
#   - PCIe config-space reads (lspci / setpci reads — no "=", never writes)
#   - sysfs / procfs reads
#   - nvidia-smi queries
#   - dmesg greps
# It does NOT write any register, does NOT change BIOS/PCIe settings, does NOT
# load/unload drivers, and does NOT run the cmpunlocker unlock. Safe on a stock,
# unpatched 170HX. Run it, then send back the output file it writes.
#
# The optional --fuse section reads a few GA100 fuse/feature MMIO registers
# (still read-only). It is OFF by default because it mmaps device BAR0; only
# enable it if Secure Boot / kernel lockdown is OFF and you're comfortable with
# read-only MMIO. Everything else is 100% config-space/sysfs and carries no more
# risk than `lspci`.
#
# Usage:
#   sudo ./probe-170hx.sh            # safe recon (recommended first)
#   sudo ./probe-170hx.sh --fuse     # also read targeted fuse/feature regs (opt-in)
###############################################################################
set -u

WANT_FUSE=0
for a in "$@"; do case "$a" in --fuse) WANT_FUSE=1;; esac; done

TS="$(date +%Y%m%d_%H%M%S 2>/dev/null || echo now)"
OUT="recon-170hx-${TS}.txt"
exec > >(tee "$OUT") 2>&1

hr(){ printf '\n============================================================\n%s\n============================================================\n' "$*"; }
have(){ command -v "$1" >/dev/null 2>&1; }

hr "0. Environment"
echo "date:        $(date 2>/dev/null)"
echo "kernel:      $(uname -a 2>/dev/null)"
echo "euid:        $(id -u)  ($([ "$(id -u)" -eq 0 ] && echo root || echo 'NON-ROOT — rerun with sudo for full detail'))"
[ -r /sys/kernel/security/lockdown ] && echo "lockdown:    $(cat /sys/kernel/security/lockdown)"
if have mokutil; then echo "secureboot:  $(mokutil --sb-state 2>/dev/null | head -1)"; fi

# ---- locate the GPU -------------------------------------------------------
BDF=""
if have lspci; then
    LINE="$(lspci -Dnn 2>/dev/null | grep -iE '10de:20c2|10de:2082|10de:20b0' | head -1)"
    BDF="$(echo "$LINE" | awk '{print $1}')"
fi
if [ -z "$BDF" ]; then
    echo "!! No CMP 170HX found (10de:20c2 / 10de:2082 / 10de:20b0). Is lspci installed / card present?"
    echo "   Dumping all NVIDIA devices for reference:"
    lspci -Dnn 2>/dev/null | grep -i 10de
    exit 1
fi
DEVID="$(echo "$LINE" | grep -oiE '10de:[0-9a-f]{4}' | head -1 | cut -d: -f2 | tr A-F a-f)"
echo "GPU BDF:     $BDF   (10de:$DEVID)"
SYS="/sys/bus/pci/devices/${BDF}"

hr "1. Identity"
echo "$LINE"
[ -r /proc/driver/nvidia/version ] && { echo "-- /proc/driver/nvidia/version --"; cat /proc/driver/nvidia/version; }
if have nvidia-smi; then
    echo "-- nvidia-smi identity/memory (STOCK values) --"
    nvidia-smi --query-gpu=name,driver_version,vbios_version,pci.bus_id,memory.total,pcie.link.gen.max,pcie.link.gen.current,pcie.link.width.max,pcie.link.width.current --format=csv 2>/dev/null
fi

hr "2. PCIe link / generation  (Test 1: settles Gen1 vs Gen2 fused ceiling)"
if have setpci; then
    lc=$(setpci -s "$BDF" CAP_EXP+0c.l 2>/dev/null)   # Link Capabilities
    l2=$(setpci -s "$BDF" CAP_EXP+2c.l 2>/dev/null)   # Link Capabilities 2  <-- the fused supported-speeds vector
    ls=$(setpci -s "$BDF" CAP_EXP+10.l 2>/dev/null)   # LinkControl(15:0)+LinkStatus(31:16)
    l2c=$(setpci -s "$BDF" CAP_EXP+30.l 2>/dev/null)  # LinkControl2 + LinkStatus2
    echo "raw:  LnkCap=0x${lc:-????}  LnkCap2=0x${l2:-????}  LnkCtl/Sta=0x${ls:-????}  LnkCtl2/Sta2=0x${l2c:-????}"
    if [ -n "${lc:-}" ]; then
        echo "  LnkCap max speed field (bits 3:0) = $(( 0x$lc & 0xF ))  (1=Gen1 2=Gen2 3=Gen3 4=Gen4)"
    fi
    if [ -n "${l2:-}" ]; then
        v=$(( (0x$l2 >> 1) & 0x7F ))   # supported-speeds vector, bits 7:1
        sup=""
        (( v & 0x1 )) && sup+="Gen1 "; (( v & 0x2 )) && sup+="Gen2 "
        (( v & 0x4 )) && sup+="Gen3 "; (( v & 0x8 )) && sup+="Gen4 "
        echo "  LnkCap2 supported-speeds vector = 0x$(printf %x $v)  => ${sup:-<none decoded>}"
        echo "  >>> This is THE fused gen ceiling. 'Gen1' only = software dead-end confirmed;"
        echo "  >>>  'Gen1 Gen2' = Gen2 x16 (~8 GB/s) is on the table with the caps mod."
    fi
    if [ -n "${ls:-}" ]; then
        cur=$(( (0x$ls >> 16) & 0xF )); wid=$(( (0x$ls >> 20) & 0x3F ))
        echo "  LnkSta current speed=$cur (1=Gen1..4=Gen4)  negotiated width=x$wid"
    fi
fi
echo "-- lspci -vvv link lines --"
lspci -vvv -s "$BDF" 2>/dev/null | grep -iE 'LnkCap:|LnkCap2:|LnkCtl:|LnkSta:|LnkCtl2:|LnkSta2:' | sed 's/^/  /'

hr "3. BAR layout / Resizable BAR / Above-4G  (the ReBAR question)"
echo "-- kernel-assigned BAR resources ($SYS/resource: start end flags) --"
[ -r "$SYS/resource" ] && awk 'NR<=6{printf "  BAR%d: %s\n", NR-1, $0}' "$SYS/resource"
echo "  (a BAR whose start >= 0x100000000 is mapped above 4GB => Above-4G Decoding is ON & working)"
echo
echo "-- lspci Region (BAR) sizes + flags --"
lspci -vvv -s "$BDF" 2>/dev/null | grep -iE 'Region [0-9]+:' | sed 's/^/  /'
echo
echo "-- Physical Resizable BAR capability (advertised BAR1 max = the ReBAR ceiling) --"
lspci -vvv -s "$BDF" 2>/dev/null | grep -iA12 'Resizable BAR' | sed 's/^/  /'
echo "  >>> 'BAR 1 ... supported:' lists the sizes the GPU advertises. The LARGEST entry is the"
echo "  >>>  hard cap on how much VRAM the CPU can map — analogous to the fused PCIe-gen ceiling."
echo "  >>>  If it tops out well below the unlocked FB (64GB/40GB), ReBAR can never expose it all."
echo
if have nvidia-smi; then
    echo "-- nvidia-smi BAR1 --"
    nvidia-smi -q 2>/dev/null | grep -iA3 'BAR1 Memory' | sed 's/^/  /'
fi
echo "-- /proc/iomem ranges for this GPU (needs root) --"
grep -iE "${BDF}|${BDF#0000:}" /proc/iomem 2>/dev/null | sed 's/^/  /' || echo "  (root required, or nothing mapped)"
echo "-- dmesg: BAR assignment / reservation / ReBAR messages --"
dmesg 2>/dev/null | grep -iE "${BDF}|${BDF#0000:}|BAR [0-9]|resizable|can't reserve|no space|failed to assign|reassign" | tail -30 | sed 's/^/  /' \
    || echo "  (root required for dmesg, or no relevant lines)"

hr "4. PCIe capabilities / AtomicOps / ACS (context for BAR + P2P behavior)"
lspci -vvv -s "$BDF" 2>/dev/null | grep -iE 'DevCap:|DevCtl:|DevCap2:|DevCtl2:|AtomicOp|ACSCap|ACSCtl|Address Space' | sed 's/^/  /'

if [ "$WANT_FUSE" -eq 1 ]; then
hr "5. Fuse / feature-override MMIO reads  (OPT-IN --fuse; READ-ONLY; needs SB/lockdown OFF)"
echo "NOTE: these read device BAR0 via mmap. Read-only, but if Secure Boot/lockdown is on the mmap"
echo "will simply fail (harmless). Values reading 0xbadfXXXX = PLM/PROT-protected on a stock driver"
echo "(expected — the patched driver would be needed to read those). Targeted safe registers only."
python3 - "$BDF" <<'PY' 2>&1 | sed 's/^/  /'
import mmap, os, struct, sys
bdf = sys.argv[1]
path = f"/sys/bus/pci/devices/{bdf}/resource0"
targets = [
    (0x00820040, "EN_SW_OVERRIDE (approach #1 gate)"),
    (0x00820c00, "fuse status word"),
    (0x00820c04, "NV_FUSE_STATUS_OPT_DISPLAY"),
    (0x00820c14, "packed feature/floorsweep status (gen-fuse candidate)"),
    (0x00820d38, "status word (gen-fuse candidate)"),
    (0x00823804, "FEAT PLM (SEC2 feature-override priv mask)"),
    (0x00823814, "NV_FUSE_FEATURE_READOUT (gen-fuse candidate)"),
    (0x0082381c, "SS0 (SM-speed override — STOCK/locked value)"),
    (0x00823820, "SS1 (STOCK/locked value)"),
    (0x009a0204, "FBPA_CFG1 (memory geometry — STOCK value)"),
    (0x00100ce0, "MMU_LMR (memory range — STOCK value)"),
    (0x140880a4, "LnkCap2 PRIV mirror (expect 0xbadf: PROT-walled)"),
]
try:
    sz = os.path.getsize(path)
    fd = os.open(path, os.O_RDONLY)
    mm = mmap.mmap(fd, sz, mmap.MAP_SHARED, mmap.PROT_READ)
except Exception as e:
    print(f"cannot map {path}: {e}")
    print("(need root + Secure Boot/lockdown OFF; skipping fuse reads)")
    raise SystemExit(0)
print(f"BAR0 mapped: {path}  size=0x{sz:x}")
for off, name in targets:
    if off + 4 > sz:
        print(f"0x{off:08x}  <out of BAR0 range>  {name}")
        continue
    val = struct.unpack_from('<I', mm, off)[0]
    flag = "  <-- PROT/PLM (0xbadf)" if (val & 0xffff0000) == 0xbadf0000 else ""
    print(f"0x{off:08x} = 0x{val:08x}  {name}{flag}")
mm.close(); os.close(fd)
PY
echo
echo "  If you also have the repo's read-only 'fusedump' tool, a fuller dump helps the diff later:"
echo "    sudo ./fusedump $BDF 0x820000 0x825000 > fuse_170hx.txt"
else
hr "5. Fuse / feature-override reads — SKIPPED"
echo "Re-run with '--fuse' to include targeted read-only fuse/feature MMIO reads"
echo "(only if Secure Boot / kernel lockdown is OFF). Everything above is pure config-space/sysfs."
fi

hr "Done"
echo "Wrote: $OUT"
echo "Please send this file back. Nothing on the card was modified."

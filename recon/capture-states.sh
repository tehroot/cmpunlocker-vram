#!/usr/bin/env bash
# capture-states.sh -- capture BAR0 in boot states, not once.
#
#   sudo recon/capture-states.sh                  # S0 + S1
#   sudo recon/capture-states.sh --s1-only        # driver already loaded
#   sudo recon/capture-states.sh --s2             # + host-driven retrain (see below)
#   sudo recon/capture-states.sh -o caps/170hx    # output prefix
#
# Every capture in this repo before 2026-07 was a single post-init snapshot,
# which is why several conclusions rest on values whose *time* was never
# controlled. The one that matters most: 0x118f78 bit30 gates app08's 376-
# instruction PHY routine at 0xcb00, and reads 0 post-init on both the 170HX
# and the A100 -- but app08 contains two functions that clear that bit. "0
# afterwards" and "never set" are different observations and we have only ever
# made the first one.
#
#   S0  nvidia + nouveau NOT loaded       GFW boot output alone
#   S1  modprobe nvidia; nvidia-smi       + FWSEC-on-GSP, GSP-RM, Booter
#   S2  after a host-driven link retrain  does the per-rate set track the rate
#
# What the S0/S1 diff decides:
#
#   0x118f78 bit30   1 at S0, 0 at S1  -> 0xcb00 DOES run; the gate closes
#                                         behind it, and docs/19's closure of
#                                         that route was measuring the wrong
#                                         moment
#                    0 at both         -> closure stands
#
#   XP3G per-rate    populated at S0   -> GFW/devinit populates it; the
#                                         divergence is inside app08's 0x7eea
#                                         path and the offline diff is the game
#                    empty S0, full S1 -> GSP-RM populates it at runtime, in
#                                         software, on firmware that also runs
#                                         on our card
#
# S0 requires the driver to have never been loaded this boot. An rmmod does not
# restore pre-init state -- GSP has already run. Boot with:
#
#   iomem=relaxed modprobe.blacklist=nouveau,nvidia
#
# then run this. Read-only except for --s2 (see the warning at that step).
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DUMP="$HERE/ga100-bar0-dump"
OUT="cap"
DO_S0=1
DO_S2=0
BDF=""

while [ $# -gt 0 ]; do
    case "$1" in
        --s1-only) DO_S0=0; shift ;;
        --s2)      DO_S2=1; shift ;;
        -o)        OUT="$2"; shift 2 ;;
        -b)        BDF="$2"; shift 2 ;;
        -h|--help) sed -n '2,50p' "${BASH_SOURCE[0]}"; exit 0 ;;
        *)         echo "unknown arg: $1" >&2; exit 1 ;;
    esac
done

[ "$(id -u)" -eq 0 ] || { echo "needs root" >&2; exit 1; }

grep -q 'iomem=relaxed' /proc/cmdline || {
    echo "iomem=relaxed missing from /proc/cmdline -- resource0 mmap will fail." >&2
    echo "Add it (and modprobe.blacklist=nouveau,nvidia for S0) and reboot," >&2
    echo "or use the kmod fallback in recon/kmod/." >&2
    exit 1
}

[ -x "$DUMP" ] || cc -O2 -o "$DUMP" "$HERE/ga100-bar0-dump.c"

mkdir -p "$(dirname "$OUT")" 2>/dev/null || true

find_bdf() {
    local d id
    for d in /sys/bus/pci/devices/*; do
        [ "$(cat "$d/vendor" 2>/dev/null)" = "0x10de" ] || continue
        id="$(cat "$d/device" 2>/dev/null)"
        case "$id" in
            0x20b0|0x20b1|0x20b2|0x20b3|0x20b5|0x20b6|0x20b7|0x20b8|\
            0x20bb|0x20bd|0x20be|0x20bf|0x20c2|0x20f0|0x20f1|0x20f2)
                basename "$d"; return 0 ;;
        esac
    done
    return 1
}

[ -n "$BDF" ] || BDF="$(find_bdf)" || { echo "no GA100-class device found" >&2; exit 1; }
echo "== device $BDF  ($(cat /sys/bus/pci/devices/$BDF/device))"

cap() {  # cap <label> <file>
    echo "== capturing $1 -> $2"
    "$DUMP" "$BDF" --wide --label "$1" > "$2"
    sed -n '/^=== decode ===/,/^$/p' "$2" | sed 's/^/     /'
}

# ---------------------------------------------------------------- S0
if [ "$DO_S0" -eq 1 ]; then
    if [ -d /sys/module/nvidia ] || [ -d /sys/module/nouveau ]; then
        echo "S0 impossible: a GPU driver is already loaded this boot." >&2
        echo "  rmmod does NOT restore pre-init state -- GSP has already run." >&2
        echo "  Boot with modprobe.blacklist=nouveau,nvidia and rerun," >&2
        echo "  or pass --s1-only to skip S0." >&2
        exit 1
    fi
    cap S0-pre-driver "${OUT}-s0.txt"
fi

# ---------------------------------------------------------------- S1
if [ ! -d /sys/module/nvidia ]; then
    echo "== modprobe nvidia"
    modprobe nvidia
    modprobe nvidia_uvm 2>/dev/null || true
fi
nvidia-smi >/dev/null 2>&1 || echo "   (nvidia-smi failed; capture is still valid, note it)"
cap S1-post-init "${OUT}-s1.txt"

# ---------------------------------------------------------------- S2
if [ "$DO_S2" -eq 1 ]; then
    RP="$(basename "$(readlink -f "/sys/bus/pci/devices/$BDF/..")")"
    case "$RP" in
        [0-9a-f]*:*) ;;
        *) echo "could not resolve upstream port for $BDF" >&2; exit 1 ;;
    esac

    # Everything above this line is a read. This is not.
    #
    # The write targets the UPSTREAM PORT ($RP), not the GPU: LNKCTL2 target
    # speed, then the retrain bit in LNKCTL. That is the same upstream-driven
    # mechanism doc 10 uses for Gen2, and it is how a speed change is supposed
    # to be initiated. It is reversible -- the original LNKCTL2 is saved and
    # restored below.
    #
    # The honest risk: if the link does not come back, the GPU is gone until
    # the host reboots. On rented hardware that may not be your reboot to make.
    # Run S0/S1 first, decide whether you still need S2, and do it last.
    echo "== S2: retrain $RP down to Gen1 and back  [THIS WRITES to the root port]"
    OLD="$(setpci -s "$RP" CAP_EXP+30.W)"
    echo "   saved LNKCTL2 = 0x$OLD"

    setpci -s "$RP" CAP_EXP+30.W="0001:000f"   # target link speed = 2.5 GT/s
    setpci -s "$RP" CAP_EXP+10.W="0020:0020"   # retrain
    sleep 1
    lspci -s "$BDF" -vv 2>/dev/null | grep -m1 'LnkSta:' | sed 's/^/   /'
    cap S2-gen1 "${OUT}-s2-gen1.txt"

    echo "== restoring LNKCTL2 = 0x$OLD"
    setpci -s "$RP" CAP_EXP+30.W="$OLD:000f"
    setpci -s "$RP" CAP_EXP+10.W="0020:0020"
    sleep 1
    lspci -s "$BDF" -vv 2>/dev/null | grep -m1 'LnkSta:' | sed 's/^/   /'
    cap S2-restored "${OUT}-s2-restored.txt"
fi

# ---------------------------------------------------------------- summary
echo
echo "== decisive lines across states"
for f in "${OUT}"-s*.txt; do
    [ -f "$f" ] || continue
    printf '%-28s %s\n' "$(basename "$f")" \
        "$(grep -h 'bit30 ' "$f" | head -1 | tr -s ' ')"
    printf '%-28s %s\n' "" "$(grep -h 'GFW_BOOT progress' "$f" | tr -s ' ')"
    printf '%-28s %s\n' "" "$(grep -h 'XP3G per-rate' "$f" | tr -s ' ')"
    printf '%-28s %s\n' "" "$(grep -h 'MSGBOX 0x200e0' "$f" | tr -s ' ')"
    printf '%-28s %s\n' "" "$(grep -h 'LnkSta  cur_speed' "$f" | tr -s ' ')"
    echo
done

echo "Diff any two with:  recon/dump-diff.sh ${OUT}-s0.txt ${OUT}-s1.txt \\"
echo "                        --mask recon/volatile-offsets-wide2.txt"

#!/usr/bin/env bash
###############################################################################
# power-cap-170hx.sh — cap the CMP 170HX to a minimal power/clock state for
# PASSIVE (no-airflow) probing, with a temperature watchdog.
#
# Everything it changes is a REVERSIBLE nvidia-smi DRIVER setting — NOT firmware,
# NOT a fuse, NOT the VBIOS. Undo it all with:  ./power-cap-170hx.sh --reset
#   - persistence mode ON        (limits survive with no client attached)
#   - power limit -> minimum      (or --watts N)
#   - graphics-clock ceiling -> low
#   - optional temperature watchdog loop (--watch)
#
# ############################  ⚠ READ THIS  ################################
# A 170HX has NO onboard fan and is designed for forced server airflow. Even
# power-capped and idle, running it with ZERO airflow WILL heat-soak and can
# reach damaging temps. This script caps the ceiling and warns you — it CANNOT
# cool the card. Put at least a desk fan on it and keep --watch running.
# ##########################################################################
#
# Usage:
#   sudo ./power-cap-170hx.sh                 # cap to min power + low clocks
#   sudo ./power-cap-170hx.sh --watts 120     # cap to a specific wattage
#   sudo ./power-cap-170hx.sh --watch         # cap, then live temp watchdog
#   sudo ./power-cap-170hx.sh --watch 3 --crit 80 --halt-on-crit
#   sudo ./power-cap-170hx.sh --reset         # restore defaults
###############################################################################
set -u

WATTS=""          # target power limit (blank = card minimum)
GR_CAP=510        # graphics-clock ceiling (MHz) when capping
WARN=70           # temperature warn threshold (C)
CRIT=83           # temperature critical threshold (C)  [GA100 slows ~85, conservative here]
WATCH=0
INTERVAL=5
RESET=0
HALT=0

while [ $# -gt 0 ]; do
    case "$1" in
        --watts) WATTS="${2:-}"; shift 2 ;;
        --grcap) GR_CAP="${2:-}"; shift 2 ;;
        --warn)  WARN="${2:-}";  shift 2 ;;
        --crit)  CRIT="${2:-}";  shift 2 ;;
        --watch) WATCH=1; if [[ "${2:-}" =~ ^[0-9]+$ ]]; then INTERVAL="$2"; shift; fi; shift ;;
        --halt-on-crit) HALT=1; shift ;;
        --reset) RESET=1; shift ;;
        -h|--help) sed -n '2,32p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "Unknown arg: $1 (try --help)" >&2; exit 1 ;;
    esac
done

command -v nvidia-smi >/dev/null 2>&1 || { echo "nvidia-smi not found (is the driver loaded?)" >&2; exit 1; }
[ "$(id -u)" -eq 0 ] || { echo "Run as root: sudo $0 ..." >&2; exit 1; }

# --- locate the 170HX's nvidia-smi index ---------------------------------
ROWS="$(nvidia-smi --query-gpu=index,name,pci.bus_id --format=csv,noheader 2>/dev/null)"
IDX="$(echo "$ROWS" | awk -F', *' 'tolower($2) ~ /170hx|cmp/ {print $1; exit}')"
if [ -z "${IDX}" ]; then
    # fall back: single GPU -> 0, else ask
    n="$(echo "$ROWS" | grep -c .)"
    if [ "$n" -eq 1 ]; then IDX=0; else
        echo "Could not identify the 170HX among multiple GPUs:"; echo "$ROWS"
        echo "Re-run after setting IDX, or ensure the 170HX is present." >&2; exit 1
    fi
fi
NAME="$(echo "$ROWS" | awk -F', *' -v i="$IDX" '$1==i{print $2; exit}')"
echo "Target GPU: index ${IDX} — ${NAME}"

nvsmi(){ nvidia-smi -i "$IDX" "$@"; }

# --- reset path ----------------------------------------------------------
if [ "$RESET" -eq 1 ]; then
    echo "Restoring defaults..."
    def="$(nvsmi --query-gpu=power.default_limit --format=csv,noheader,nounits 2>/dev/null | tr -d ' ')"
    nvsmi -rgc >/dev/null 2>&1 && echo "  graphics clocks reset" || echo "  (clock reset unsupported/failed)"
    nvsmi -rmc >/dev/null 2>&1 || true
    if [ -n "${def}" ]; then nvsmi -pl "${def%.*}" >/dev/null 2>&1 && echo "  power limit -> default ${def} W"; fi
    echo "Done. (persistence mode left as-is; disable with: nvidia-smi -i ${IDX} -pm 0)"
    exit 0
fi

# --- read limits ---------------------------------------------------------
read -r MINW DEFW MAXW <<<"$(nvsmi --query-gpu=power.min_limit,power.default_limit,power.max_limit \
    --format=csv,noheader,nounits 2>/dev/null | tr -d ',' )"
MINW="${MINW%.*}"; DEFW="${DEFW%.*}"; MAXW="${MAXW%.*}"
[ -n "${MINW:-}" ] || { echo "Could not read power limits (does this GPU support -pl?)." >&2; exit 1; }
echo "Power limits (W): min=${MINW} default=${DEFW} max=${MAXW}"

TARGET="${WATTS:-$MINW}"; TARGET="${TARGET%.*}"
[[ "$TARGET" =~ ^[0-9]+$ ]] || { echo "  --watts must be an integer; using min ${MINW}W"; TARGET="$MINW"; }
# clamp to [min,max]
if [ "$TARGET" -lt "$MINW" ]; then echo "  requested ${TARGET}W < min; clamping to ${MINW}W"; TARGET="$MINW"; fi
if [ "$TARGET" -gt "$MAXW" ]; then echo "  requested ${TARGET}W > max; clamping to ${MAXW}W"; TARGET="$MAXW"; fi

# --- apply ---------------------------------------------------------------
echo "Enabling persistence mode..."
nvsmi -pm 1 >/dev/null 2>&1 && echo "  persistence ON" || echo "  (persistence via -pm failed; try nvidia-persistenced)"

echo "Setting power limit -> ${TARGET} W..."
nvsmi -pl "${TARGET}" >/dev/null 2>&1 && echo "  power limit set" || echo "  !! power-limit set FAILED"

echo "Capping graphics clock ceiling -> ${GR_CAP} MHz..."
nvsmi -lgc 0,"${GR_CAP}" >/dev/null 2>&1 && echo "  graphics clock capped" || echo "  (graphics-clock lock unsupported/failed — power cap still applies)"

echo
echo "Applied. Current state:"
nvsmi --query-gpu=power.limit,temperature.gpu,clocks.gr,clocks.mem,utilization.gpu \
      --format=csv 2>/dev/null | sed 's/^/  /'

CUR_T=""; CUR_FLAG="OK"
status_line(){
    local t p g u
    IFS=',' read -r t p g u <<<"$(nvsmi --query-gpu=temperature.gpu,power.draw,clocks.gr,utilization.gpu \
        --format=csv,noheader,nounits 2>/dev/null | tr -d ' ')"
    t="${t%.*}"
    CUR_T="$t"; CUR_FLAG="OK"
    if   [[ "$t" =~ ^[0-9]+$ ]] && [ "$t" -ge "$CRIT" ]; then CUR_FLAG="CRIT"
    elif [[ "$t" =~ ^[0-9]+$ ]] && [ "$t" -ge "$WARN" ]; then CUR_FLAG="WARN"; fi
    printf '%s  temp=%s°C  draw=%sW  gr=%sMHz  util=%s%%  [%s]\n' \
        "$(date +%H:%M:%S 2>/dev/null)" "${t:-?}" "${p:-?}" "${g:-?}" "${u:-?}" "$CUR_FLAG"
}

if [ "$WATCH" -eq 1 ]; then
    echo
    echo "Watchdog: warn>=${WARN}°C  crit>=${CRIT}°C  every ${INTERVAL}s  (Ctrl-C to stop)"
    echo "Reminder: this only WARNS — add airflow; the script can't cool the card."
    crit_streak=0
    while true; do
        status_line
        if [ "$CUR_FLAG" = "CRIT" ]; then
            crit_streak=$((crit_streak+1))
            printf '\a\a'   # audible bell
            echo "  *** CRITICAL: ${CUR_T}°C >= ${CRIT}°C — ADD COOLING OR POWER OFF NOW (streak ${crit_streak}) ***"
            if [ "$HALT" -eq 1 ] && [ "$crit_streak" -ge 3 ]; then
                echo "  --halt-on-crit: powering off to protect the card."
                command -v systemctl >/dev/null 2>&1 && systemctl poweroff || shutdown -h now
                exit 0
            fi
        else
            crit_streak=0
        fi
        sleep "$INTERVAL"
    done
fi

echo
echo "Not watching. To monitor: sudo $0 --watch   (or add a desk fan and re-run with --watch)"
echo "To undo everything:        sudo $0 --reset"

#!/usr/bin/env bash
# cmpx.sh -- run one probe cycle without touching /etc/modprobe.d.
#
#   sudo recon/cmpx.sh CmpXvePermit=0x3fc CmpXveCmd=0xe
#   sudo recon/cmpx.sh --base-only            # plain Gen2, no probes
#   sudo recon/cmpx.sh -g FUSE CmpFuseOvr=1   # filter output on a pattern
#
# NVreg_RegistryDwords is consumed at module init, so probe keys must be passed
# at load time. Passing them here rather than editing cmp-pcie-gen2.conf means
# a stale experiment can never survive into the next run -- which has bitten
# this project before.
#
# The persistent conf still governs normal boots; this does not modify it.
set -euo pipefail

BASE="RmForceEnableGen2=1;RMPcieLinkSpeed=0x1"
GREP='SEC2_DEBUG'
KEYS=()

while [ $# -gt 0 ]; do
    case "$1" in
        --base-only) shift ;;
        -g) GREP="$2"; shift 2 ;;
        *)  KEYS+=("$1"); shift ;;
    esac
done

[ "$(id -u)" -eq 0 ] || { echo "needs root" >&2; exit 1; }

DWORDS="$BASE"
for k in ${KEYS+"${KEYS[@]}"}; do DWORDS="$DWORDS;$k"; done

echo "== NVreg_RegistryDwords=\"$DWORDS\""

# Verify the running module actually contains the probe keys being requested.
# A rebuild that silently did not take looks identical to a probe that ran and
# found nothing -- that has burned this project three times.
KO=$(find /lib/modules/"$(uname -r)" -name 'nvidia.ko*' 2>/dev/null | head -1)
if [ -n "$KO" ]; then
    MISSING=""
    for k in ${KEYS+"${KEYS[@]}"}; do
        name=${k%%=*}
        case "$name" in Cmp*) ;; *) continue ;; esac
        # -a is required: without it GNU strings skips .rodata in a
        # relocatable .ko and every key looks absent.
        strings -a "$KO" 2>/dev/null | grep -qx "$name" || MISSING="$MISSING $name"
    done
    if [ -n "$MISSING" ]; then
        # Warn, do not abort: a wrong check blocking real work is worse than a
        # silent rebuild. Believe the log over this heuristic.
        echo "!! WARNING: module may lack:$MISSING"
        echo "!! if the probe produces no output:  rm -rf driver/.build && sudo ./install.sh"
    fi
fi

# Clear the ring buffer so only this run's output is shown. Counting lines and
# slicing was unreliable -- the buffer wraps under the volume these probes emit
# and the offset silently goes wrong, producing "no matching lines" on a run
# that logged fine. History is already committed in the docs; live probe output
# is what matters here.
dmesg -C

modprobe -r nvidia_uvm nvidia_drm nvidia_modeset nvidia 2>/dev/null || {
    echo "!! modules busy -- in use by:" >&2
    fuser -v /dev/nvidia* 2>&1 | head >&2
    exit 1
}

modprobe nvidia NVreg_RegistryDwords="$DWORDS"

# GSP does not bootstrap until something opens the device.
nvidia-smi >/dev/null 2>&1 || echo "!! nvidia-smi failed (card may be wedged)"

echo "== link state (gen.max, gen.current, width.current)"
nvidia-smi --query-gpu=pcie.link.gen.max,pcie.link.gen.current,pcie.link.width.current \
           --format=csv,noheader 2>/dev/null || echo "  unavailable"

echo "== log"
if ! dmesg | grep -E "$GREP" | sed 's/^\[[^]]*\] //'; then
    echo "  (no matching lines -- module may lack the probe; check:"
    echo "   strings \$(find /lib/modules/\$(uname -r) -name 'nvidia.ko*' | head -1) | grep -c SEC2_DEBUG )"
fi

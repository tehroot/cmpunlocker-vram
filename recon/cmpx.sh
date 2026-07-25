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

# Record the log position so we only show this run's output.
MARK=$(dmesg | wc -l)

modprobe -r nvidia_uvm nvidia_drm nvidia_modeset nvidia 2>/dev/null || {
    echo "!! modules busy -- in use by:" >&2
    fuser -v /dev/nvidia* 2>&1 | head >&2
    exit 1
}

modprobe nvidia NVreg_RegistryDwords="$DWORDS"

# GSP does not bootstrap until something opens the device.
nvidia-smi >/dev/null 2>&1 || echo "!! nvidia-smi failed (card may be wedged)"

echo "== link state"
nvidia-smi --query-gpu=pcie.link.gen.max,pcie.link.gen.current,pcie.link.width.current \
           --format=csv,noheader 2>/dev/null || echo "  unavailable"

echo "== log"
dmesg | tail -n +$((MARK + 1)) | grep -E "$GREP" | sed 's/^\[[^]]*\] //' || echo "  (no matching lines)"

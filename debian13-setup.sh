#!/usr/bin/env bash
#
# debian13-setup.sh — one-shot bootstrap for cmpunlocker on Debian 13 (trixie).
#
# Does, in order, everything we otherwise did by hand:
#   1. Prerequisites          (build-essential, matching kernel headers, tools)
#   2. Sanity checks          (gcc-vs-kernel compiler, Secure Boot)
#   3. Split-headers fix      (merge linux-headers-*-common into the build tree)
#   4. NVIDIA open driver     (download + silent install of 610.43.0x, if not present)
#   5. cmpunlocker install    (./install.sh — builds + installs the patched modules)
#
# IDEMPOTENT and RE-RUNNABLE. If a stage needs a reboot (e.g. to unload nouveau),
# it stops with a clear banner; reboot and run the script again — completed stages
# are detected and skipped.
#
#   sudo ./debian13-setup.sh [--profile=8gb|10gb] [--skip-driver]
#
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
KVER="$(uname -r)"
BUILD_DIR="/lib/modules/${KVER}/build"
DL_DIR="/var/tmp/cmpunlocker"
SKIP_DRIVER=0
PROFILE_ARG=""

# --- Driver target: first supported version in driver/VERSION -----------------
TARGET_DRIVER="610.43.03"
if [[ -r "${SCRIPT_DIR}/driver/VERSION" ]]; then
    _v="$(grep -E '^[0-9]+\.[0-9]+\.[0-9]+$' "${SCRIPT_DIR}/driver/VERSION" | head -1 || true)"
    [[ -n "${_v}" ]] && TARGET_DRIVER="${_v}"
fi
DRIVER_RUN="NVIDIA-Linux-x86_64-${TARGET_DRIVER}.run"
DRIVER_URL="https://us.download.nvidia.com/XFree86/Linux-x86_64/${TARGET_DRIVER}/${DRIVER_RUN}"

# --- Pretty logging -----------------------------------------------------------
if [ -t 1 ] && [ -z "${NO_COLOR:-}" ]; then
    RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'; CYAN='\033[0;36m'; NC='\033[0m'
else
    RED=""; GREEN=""; YELLOW=""; CYAN=""; NC=""
fi
info() { echo -e "${CYAN}[INFO]${NC}  $*"; }
ok()   { echo -e "${GREEN}[ OK ]${NC}  $*"; }
warn() { echo -e "${YELLOW}[WARN]${NC}  $*"; }
die()  { echo -e "${RED}[FAIL]${NC}  $*" >&2; exit 1; }
step() {
    echo ""
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
    echo -e "${CYAN}$*${NC}"
    echo -e "${CYAN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
}
reboot_checkpoint() {
    echo ""
    echo -e "${YELLOW}╔══════════════════════════════════════════════════════╗${NC}"
    echo -e "${YELLOW}║  ACTION REQUIRED: reboot, then re-run this script.    ║${NC}"
    echo -e "${YELLOW}╚══════════════════════════════════════════════════════╝${NC}"
    echo -e "  Reason: $*"
    echo -e "  After reboot:  ${CYAN}sudo ${SCRIPT_DIR}/debian13-setup.sh${NC}"
    echo -e "  (it resumes — completed stages are skipped)"
    exit 0
}

# --- Args ---------------------------------------------------------------------
for arg in "$@"; do
    case "${arg}" in
        --profile=8gb|--profile=8GB)   PROFILE_ARG="--profile=8gb" ;;
        --profile=10gb|--profile=10GB) PROFILE_ARG="--profile=10gb" ;;
        --skip-driver)                 SKIP_DRIVER=1 ;;
        -y|--yes)                      : ;;   # accepted; script is non-interactive anyway
        -h|--help)
            cat <<EOF
debian13-setup.sh — bootstrap cmpunlocker on Debian 13 (trixie)

  sudo ./debian13-setup.sh [options]

  --profile=8gb|10gb   Force the card profile (default: auto-detect from nvidia-smi)
  --skip-driver        Leave the NVIDIA driver alone (assume ${TARGET_DRIVER} open is already installed)
  -y, --yes            Non-interactive (default behaviour; accepted for convenience)
  -h, --help           Show this help

Stages: prereqs → gcc/Secure-Boot checks → split-headers fix → NVIDIA ${TARGET_DRIVER} (open) → install.sh
Idempotent: safe to re-run, including after a reboot checkpoint.
EOF
            exit 0
            ;;
        *) die "Unknown argument: ${arg}  (try --help)" ;;
    esac
done

[[ "${EUID}" -eq 0 ]] || die "Run as root:  sudo ${SCRIPT_DIR}/debian13-setup.sh"

# --- Helpers ------------------------------------------------------------------
current_driver_version() {
    local v=""
    if [[ -r /proc/driver/nvidia/version ]]; then
        v="$(grep -oE '[0-9]+\.[0-9]+\.[0-9]+' /proc/driver/nvidia/version | head -1 || true)"
    fi
    if [[ -z "${v}" ]] && command -v nvidia-smi &>/dev/null; then
        v="$(nvidia-smi --query-gpu=driver_version --format=csv,noheader 2>/dev/null | head -1 | tr -d '[:space:]' || true)"
    fi
    echo "${v}"
}

card_present() {
    lspci -nn 2>/dev/null | grep -iqE '10de:20c2|10de:2082|10de:20b0'
}

# --- Stage 0: environment -----------------------------------------------------
echo ""
echo -e "${CYAN}╔════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║     cmpunlocker · Debian 13 setup      ║${NC}"
echo -e "${CYAN}╚════════════════════════════════════════╝${NC}"

step "0/5  Environment"
if [[ -r /etc/os-release ]]; then
    . /etc/os-release
    if [[ "${ID:-}" == "debian" && "${VERSION_ID:-}" == "13" ]]; then
        ok "Debian 13 (trixie) confirmed"
    else
        warn "Expected Debian 13; detected '${PRETTY_NAME:-unknown}'. Continuing — the split-headers step assumes Debian's layout."
    fi
fi
info "Kernel: ${KVER}"
if card_present; then
    ok "CMP 170HX on the bus: $(lspci -nn | grep -iE '10de:20c2|10de:2082|10de:20b0' | head -1)"
else
    warn "No CMP 170HX detected yet (10de:20c2 / 2082 / 20b0)."
    warn "That's fine if the card isn't installed — the driver/headers stages still run; the unlock build is skipped until it's present."
fi

# --- Stage 1: prerequisites ---------------------------------------------------
step "1/5  Prerequisites"
export DEBIAN_FRONTEND=noninteractive
info "apt-get update…"
apt-get update -y
info "Installing build tooling + headers for ${KVER}…"
if ! apt-get install -y --no-install-recommends \
        build-essential "linux-headers-${KVER}" pkg-config libglvnd-dev \
        wget curl kmod pciutils file mokutil; then
    die "apt install failed. If it can't find linux-headers-${KVER}, your headers package lags the running kernel — 'apt full-upgrade && reboot' onto the matching kernel, then re-run."
fi
ok "Prerequisites installed"

# --- Stage 2: sanity checks ---------------------------------------------------
step "2/5  Sanity checks"
# gcc vs. kernel compiler
KGCC="$(grep -oE 'gcc-[0-9]+' /proc/version 2>/dev/null | head -1 | grep -oE '[0-9]+' || true)"
SGCC="$(gcc -dumpversion 2>/dev/null | cut -d. -f1 || true)"
if [[ -n "${KGCC}" && -n "${SGCC}" && "${KGCC}" != "${SGCC}" ]]; then
    warn "Kernel built with gcc-${KGCC}, but default gcc is ${SGCC}."
    warn "  If the module build fails on version-magic, install the matching compiler:"
    warn "    sudo apt install gcc-${KGCC} && sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-${KGCC} 100 --slave /usr/bin/g++ g++ /usr/bin/g++-${KGCC}"
else
    ok "Compiler: gcc ${SGCC:-?} matches the kernel toolchain"
fi
# Secure Boot
if [[ -d /sys/firmware/efi ]] && command -v mokutil &>/dev/null; then
    if mokutil --sb-state 2>/dev/null | grep -qi 'SecureBoot enabled'; then
        die "Secure Boot is ENABLED — cmpunlocker's patched modules are unsigned and will not load.\n  Disable Secure Boot in UEFI setup, then re-run."
    fi
fi
ok "Secure Boot is not blocking (disabled or legacy BIOS)"

# --- Stage 3: split-headers fix ----------------------------------------------
step "3/5  Debian split-headers fix"
# Debian ships kernel headers in two trees: the arch tree (what /lib/modules/.../build
# points at) and a '-common' tree with the actual include/linux source headers. NVIDIA's
# conftest only searches the arch tree, goes blind, and falls back to ancient kernel APIs.
# Merging '-common' into the build tree (no-clobber) makes conftest see the real headers.
if [[ -f "${BUILD_DIR}/include/linux/fs.h" ]]; then
    ok "Build tree already has the common headers — nothing to merge"
else
    ARCH_HDR="$(readlink -f "${BUILD_DIR}" 2>/dev/null || echo "${BUILD_DIR}")"
    COMMON_HDR="${ARCH_HDR%-*}-common"
    if [[ ! -d "${COMMON_HDR}" ]]; then
        VER_BASE="${KVER%-*}"   # e.g. 6.12.90+deb13.1
        COMMON_HDR="$(ls -d /usr/src/linux-headers-*-common 2>/dev/null | grep -F "${VER_BASE}" | head -1 || true)"
    fi
    if [[ -z "${COMMON_HDR}" || ! -d "${COMMON_HDR}" ]]; then
        COMMON_HDR="$(ls -d /usr/src/linux-headers-*-common 2>/dev/null | sort -V | tail -1 || true)"
    fi
    [[ -n "${COMMON_HDR}" && -d "${COMMON_HDR}" ]] || \
        die "Could not locate a linux-headers-*-common tree. Ensure linux-headers-${KVER} is installed (it pulls the -common package)."
    info "Merging common headers:  ${COMMON_HDR}  →  ${BUILD_DIR}"
    # -n (no-clobber) keeps every arch/generated file (.config, Module.symvers, generated/, arch Makefile)
    # and only ADDS the missing common source headers.
    cp -rn "${COMMON_HDR}/." "${BUILD_DIR}/"
    [[ -f "${BUILD_DIR}/include/linux/fs.h" ]] || \
        die "Header merge did not populate ${BUILD_DIR}/include/linux — inspect ${COMMON_HDR}"
    ok "Common headers merged into the build tree"
fi

# --- Stage 4: NVIDIA open driver ---------------------------------------------
step "4/5  NVIDIA open driver ${TARGET_DRIVER}"
if [[ "${SKIP_DRIVER}" -eq 1 ]]; then
    warn "--skip-driver set; leaving the NVIDIA driver as-is"
else
    CUR="$(current_driver_version)"
    if [[ "${CUR}" == "${TARGET_DRIVER}" ]]; then
        ok "NVIDIA ${CUR} already installed (target ${TARGET_DRIVER})"
    else
        [[ -n "${CUR}" ]] && warn "Installed driver is ${CUR}; want ${TARGET_DRIVER} (open modules). Replacing."
        # Refuse to mix .run over apt-managed NVIDIA packages — that path is a mess.
        if dpkg -l 2>/dev/null | awk '/^ii/{print $2}' | grep -qE '^nvidia-(driver|kernel|dkms|open|smi)'; then
            die "apt-managed NVIDIA packages are installed. Remove them first, then re-run:\n    sudo apt purge '~nnvidia-.*' && sudo apt autoremove --purge\n  (or 'sudo apt purge nvidia-driver nvidia-kernel-dkms nvidia-open' for the specific ones you have)"
        fi
        # Blacklist nouveau; unload it if live (else the .run install can't bind).
        cat >/etc/modprobe.d/blacklist-nouveau.conf <<'EOF'
blacklist nouveau
options nouveau modeset=0
EOF
        if lsmod | grep -q '^nouveau'; then
            warn "nouveau is loaded — stopping display managers and unloading…"
            for dm in gdm3 gdm lightdm sddm display-manager; do systemctl stop "${dm}" 2>/dev/null || true; done
            modprobe -r nouveau 2>/dev/null || true
        fi
        if lsmod | grep -q '^nouveau'; then
            update-initramfs -u 2>/dev/null || true
            reboot_checkpoint "nouveau is in use and won't unload live; it's now blacklisted for next boot."
        fi
        # Download (resumable) and install silently.
        mkdir -p "${DL_DIR}"
        if [[ ! -f "${DL_DIR}/${DRIVER_RUN}" ]]; then
            info "Downloading ${DRIVER_RUN}…"
            wget -c -O "${DL_DIR}/${DRIVER_RUN}.partial" "${DRIVER_URL}"
            mv "${DL_DIR}/${DRIVER_RUN}.partial" "${DL_DIR}/${DRIVER_RUN}"
        else
            ok "Using cached installer ${DL_DIR}/${DRIVER_RUN}"
        fi
        info "Installing NVIDIA ${TARGET_DRIVER} (open kernel modules, silent)…"
        sh "${DL_DIR}/${DRIVER_RUN}" --silent --kernel-module-type=open --no-x-check
        CUR="$(current_driver_version)"
        if [[ "${CUR}" == "${TARGET_DRIVER}" ]]; then
            ok "NVIDIA ${TARGET_DRIVER} installed and reporting"
        elif [[ -d "/lib/firmware/nvidia/${TARGET_DRIVER}" ]]; then
            ok "NVIDIA ${TARGET_DRIVER} files installed (module not loaded yet — that's fine)"
        else
            reboot_checkpoint "driver installed but not yet active; a reboot will load it."
        fi
    fi
fi

# --- Stage 5: cmpunlocker -----------------------------------------------------
step "5/5  cmpunlocker unlock build"
if ! card_present; then
    warn "No CMP 170HX on the bus — skipping the unlock build."
    warn "Install the card in this rig, then run:  sudo ${SCRIPT_DIR}/install.sh ${PROFILE_ARG}"
else
    chmod +x "${SCRIPT_DIR}/install.sh"
    # install.sh runs driver/build.sh, which re-runs conftest against the now-complete headers.
    if [[ -n "${PROFILE_ARG}" ]]; then
        "${SCRIPT_DIR}/install.sh" "${PROFILE_ARG}"
    else
        "${SCRIPT_DIR}/install.sh"
    fi
fi

# --- Done ---------------------------------------------------------------------
echo ""
echo -e "${GREEN}╔════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║        setup finished                  ║${NC}"
echo -e "${GREEN}╚════════════════════════════════════════╝${NC}"
echo ""
echo "Final step — a real COLD power cycle so the new FB geometry latches:"
echo -e "  1. ${CYAN}sudo shutdown -h now${NC}   (full power off — not 'reboot')"
echo -e "  2. Power the machine back on"
echo -e "  3. Verify:  ${CYAN}nvidia-smi${NC}                       (expect ~65536 MiB on an 8GB card)"
echo -e "     Confirm:  ${CYAN}sudo dmesg | grep SEC2_DEBUG${NC}    (POST-WRITE SS0/SS1/CFG1/LMR)"
echo ""
echo "On a direct PCIe slot a normal power-off cuts card power, so one cold boot is all it takes."
echo ""

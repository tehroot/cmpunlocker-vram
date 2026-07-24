# cmpunlocker

Unlock tool for the NVIDIA CMP 170HX (GA100) mining card. Restores full SM compute throughput and unlocked HBM2e memory geometry that are restricted in firmware/OTP configuration.

Targets **nvidia-open driver 610.43.0x** on Linux. cmpunlocker does **not** install the full NVIDIA userspace package — it patches and installs open kernel modules only.

**[Join our Discord community](https://discord.gg/CdHSakKSFv)** for support and discussions.

---

## Background

The CMP 170HX is a physically complete GA100 die (same silicon as the A100) with compute and memory artificially limited. This tool applies an in-driver unlock path (SEC2 Booter PLM open + host SS0/SS1/CFG1/LMR writes + FB/PMA adjustments) that runs automatically every time the patched modules boot GSP for PCI ID `0x20C2`.

Card size selects the memory geometry:

| Physical card | Unlock geometry | CFG1 | LMR |
|---|---|---|---|
| **8 GB** | **64 GB** | `0x02779000` | `0x0000020B` |
| **10 GB** | **40 GB** | `0x02669000` | `0x0000028A` |

---

## Proof of Concept

Below are memory and performance results after applying the unlock:

### Memory Unlock Results

<img alt="memory unlock" src="https://github.com/user-attachments/assets/ae062bd8-e3a7-4e73-b9a4-fbcde53f3c7b" width="100%" style="max-width: 900px;" />

### Performance Benchmarks ([OpenCL-Benchmark](https://github.com/ProjectPhysX/OpenCL-Benchmark))

<img alt="performance benchmarks" src="https://github.com/user-attachments/assets/2501506d-420f-4014-9574-b1bd0290eb60" width="100%" style="max-width: 900px;" />

---

## Requirements

- Linux (x86-64)
- Root access
- NVIDIA CMP 170HX
- **nvidia-open 610.43.0x already installed** (libs + firmware)
- Kernel headers matching the running kernel (`linux-headers-$(uname -r)` / `kernel-devel`)
- Secure Boot disabled (patched modules are unsigned)
- Network access on first install (downloads matching stock `open-gpu-kernel-modules` sources)
- Python 3 (used at build time to select 8GB/10GB geometry)

---

## Install

One command. Auto-detects 8GB vs 10GB from stock `nvidia-smi` memory, then builds patched open kernel modules into `/lib/modules/$(uname -r)/updates/cmpunlocker/`.

```bash
sudo ./install.sh
```

Force a profile if detection is wrong or `nvidia-smi` is unavailable:

```bash
sudo ./install.sh --profile=8gb    # 8GB card → 64GB unlock
sudo ./install.sh --profile=10gb   # 10GB card → 40GB unlock
```

Then perform a **cold reboot** (full power off, then boot) if modules did not hot-reload cleanly, or if memory still shows the stock size.

---

## Verify

```bash
nvidia-smi
# 8GB card:  expect ~65536 MiB
# 10GB card: expect ~40960 MiB

nvidia-smi --query-gpu=memory.total,clocks.max.sm --format=csv

sudo dmesg | grep SEC2_DEBUG
# Expected: PLMs opening to 0xffffffff, CFG1/LMR/SS0/SS1 writes, late PMA

cat /lib/modules/$(uname -r)/updates/cmpunlocker/card_profile
# 8gb or 10gb
```

Booter status codes such as `0x31` / `0xffff` during the early PLM Booter passes can appear and are often harmless if the final boot succeeds.

---

## Troubleshooting

### `/proc/driver/nvidia/version` still says `dvs-builder`

`dvs-builder` is NVIDIA's internal build host, baked into their **precompiled** module. Seeing it means the
running driver is the stock one — the patched modules in `/lib/modules/$(uname -r)/updates/cmpunlocker/`
were built and installed, but are not what got loaded.

**Confirm it properly first.** The build-host string is a hint; `srcversion` is exact:

```bash
cat /sys/module/nvidia/srcversion
modinfo -F srcversion /lib/modules/$(uname -r)/updates/cmpunlocker/nvidia.ko
# differ  -> the running module is not the patched one
```

```bash
sudo dmesg | grep SEC2_DEBUG    # silence = patched module never loaded
```

Then work down this list — roughly in the order these actually happen:

**1. The old module never unloaded.** Most common. The installer says
`Could not unload nvidia modules (in use) — cold reboot required`. Anything holding the GPU blocks it:
`nvidia-persistenced`, `nvidia-fabricmanager`, an X/Wayland session, or a container.

```bash
sudo lsof /dev/nvidia* 2>/dev/null; lsmod | grep nvidia
sudo shutdown -h now      # full power off, then power on — not `reboot`
```

Use a **cold** cycle rather than a warm `reboot` — every install path in this repo prescribes a full power
off, and warm reboots have proven unreliable at picking up the patched modules.

**2. You are running a different kernel than the one you built for.** Everything is keyed to `uname -r` at
build time. Boot another kernel (after a kernel update, say) and the patched modules are simply not in that
kernel's tree, so stock loads.

```bash
ls /lib/modules/$(uname -r)/updates/cmpunlocker/
# missing or empty -> rebuild under the running kernel:
sudo ./install.sh
```

**3. Stock wins module resolution.** `updates/` normally outranks `kernel/`, but a DKMS copy or a
`/etc/depmod.d/` override can change that.

```bash
modprobe -n -v nvidia     # must print the path under updates/cmpunlocker/
sudo depmod -a
ls /lib/modules/$(uname -r)/updates/dkms/nvidia.ko 2>/dev/null   # a competing copy
grep -r . /etc/depmod.d/ 2>/dev/null                             # search-order overrides
```

The installer already warns about this: `Resolved nvidia.ko is not under updates/cmpunlocker/ — stock may
still win`.

**4. The initramfs still carries the stock module.** The installer rebuilds it, but only if
`update-initramfs`, `dracut`, or `mkinitcpio` is present — otherwise it prints
`No initramfs tool found — rebuild manually before rebooting` and continues anyway.

```bash
sudo update-initramfs -u -k "$(uname -r)"    # or: sudo dracut --force --kver "$(uname -r)"
```

**5. A package or DKMS rebuild landed after your install** and reclaimed the module. Re-run
`sudo ./install.sh`; consider holding the nvidia packages if this recurs.

**Still stuck?** Start clean, then reinstall:

```bash
sudo ./remove.sh --yes
sudo shutdown -h now
# power on, verify stock is healthy with nvidia-smi, then:
sudo ./install.sh
```

### Memory still shows stock size

If `SEC2_DEBUG` lines *are* present but `nvidia-smi` reports the stock size, the modules are correct and the
unlock ran — cold power-cycle. If the geometry is wrong rather than stock (40 GB on an 8 GB card or vice
versa), the profile was misdetected:

```bash
cat /lib/modules/$(uname -r)/updates/cmpunlocker/card_profile   # 8gb or 10gb
sudo ./install.sh --profile=8gb    # or --profile=10gb
```

### Build fails on Debian/Ubuntu with conftest errors

Split kernel headers make conftest blind. See [`docs/11-debian-build-notes.md`](docs/11-debian-build-notes.md),
or use the one-shot [`debian13-setup.sh`](debian13-setup.sh).

---

## What Gets Unlocked

| Feature | Status |
|---|---|
| Full SM compute throughput (SS0/SS1) | Working ✓ |
| Memory geometry (64GB on 8GB cards, 40GB on 10GB cards) | Working ✓ |
| Persistence across reboot (patched modules) | Working ✓ |

---

## Uninstall

Restore stock module loading:

```bash
sudo ./remove.sh --yes
```

This removes `/lib/modules/*/updates/cmpunlocker/`, runs `depmod`, and attempts to reload stock NVIDIA modules. Reboot if the GPU does not come back cleanly.

---

## Support & Community

Having issues? Need help? Join our [Discord community](https://discord.gg/CdHSakKSFv) to discuss with other users and get support.

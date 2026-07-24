# 11 — Building cmpunlocker on Debian (the split-headers fix)

> **Practical build recipe.** Getting cmpunlocker's patched **nvidia-open 610.43.03** modules to compile on
> **Debian 13 (trixie), kernel 6.12.90+deb13.1-amd64**. The one real blocker was Debian's *split* kernel-
> headers layout (arch vs common) confusing the NVIDIA conftest into old-kernel-API fallbacks. Everything
> below is the sequence that worked; the header-merge (step 4) is the load-bearing one.

> **⚡ Automated path (do this first).** All of the steps below are wrapped in
> [`debian13-setup.sh`](../debian13-setup.sh) at the repo root — prereqs, gcc/Secure-Boot checks, the
> split-headers merge, the NVIDIA open driver, then `install.sh`. It's idempotent and resumes after a
> reboot checkpoint:
> ```bash
> sudo ./debian13-setup.sh              # auto-detect card profile
> sudo ./debian13-setup.sh --profile=8gb   # or force it
> sudo ./debian13-setup.sh --skip-driver   # if 610.43.0x open is already installed
> ```
> The sections below are the same steps by hand, for reference / debugging when a stage misbehaves.
> **This is Debian-specific** — on Ubuntu / HiveOS the headers tree is already merged, so skip step 3
> (the script detects a complete build tree and skips it automatically).

## 0. Prerequisites
```bash
sudo apt install -y build-essential linux-headers-$(uname -r) pkg-config libglvnd-dev
```
- Confirm gcc matches the kernel's compiler (no fix needed if they agree):
  ```bash
  cat /proc/version          # e.g. "...gcc-14 (Debian 14.2.0-19)..."
  gcc --version              # must be the same major (here: 14.2.0)  ✔
  ```
- Secure Boot / lockdown **off** (patched modules are unsigned).

## 1. Install the right driver first (610.43.0x, open modules)
cmpunlocker is pinned to nvidia-open **610.43.03 / 610.43.02** — a different (550, 555, …) driver makes
`install.sh` refuse *and* the patches won't apply. On Debian, the version-precise route is the `.run`
installer:
```bash
# remove the current driver first (nvidia-uninstall if .run-installed, or apt purge 'nvidia-*')
wget -c https://us.download.nvidia.com/XFree86/Linux-x86_64/610.43.03/NVIDIA-Linux-x86_64-610.43.03.run
sudo sh NVIDIA-Linux-x86_64-610.43.03.run --kernel-module-type=open
# verify:
cat /proc/driver/nvidia/version    # 610.43.03, "Open Kernel Module"
```

## 2. THE Debian blocker: split headers break the NVIDIA conftest
Debian ships kernel headers in **two** packages:
- `linux-headers-<ver>-amd64` → **arch** tree (`.config`, `Module.symvers`, `generated/`, arch `Makefile`).
  `/lib/modules/$(uname -r)/build` symlinks here.
- `linux-headers-<ver>-common` → **common** tree — the actual `include/linux/`, `include/uapi/` source
  headers.

The kernel's own build system chains the two, so the *real* compile finds everything. **But NVIDIA's
`conftest` runs standalone against `-I$(SYSSRC)/include` = the arch tree only**, which lacks the common
source headers. So every feature probe fails and the driver falls back to **ancient kernel APIs**, then the
real compile explodes on the mismatch. Symptoms seen, all the same root cause:
- `nv_stdarg.h: fatal error: stdarg.h: No such file or directory` (conftest didn't detect `linux/stdarg.h`)
- `struct mm_struct has no member named 'mmap_sem'` (renamed to `mmap_lock` in kernel 5.8)
- `conflicting types for 'vm_fault_t'` (exists since 4.17; driver re-typedef'd it as `int`)

(On Ubuntu/HiveOS the headers tree is *merged*, which is why the beta built there without this step.)

## 3. Fix: merge the common headers into the build tree
Make `-I$(SYSSRC)/include` complete so the conftest can see the kernel's real headers:
```bash
sudo cp -rn /usr/src/linux-headers-$(uname -r | sed 's/-amd64/-common/;s/+deb13.1//')/* \
            /lib/modules/$(uname -r)/build/    2>/dev/null || \
sudo cp -rn /usr/src/linux-headers-*-common/*  /lib/modules/$(uname -r)/build/
```
Concretely, for `6.12.90+deb13.1`:
```bash
sudo cp -rn /usr/src/linux-headers-6.12.90+deb13.1-common/* /lib/modules/$(uname -r)/build/
```
- `-rn` = recursive, **no-clobber** — it keeps every arch/generated file (`.config`, `Module.symvers`,
  `generated/`, the arch `Makefile`) and only *adds* the common headers the tree was missing.
- This subsumes the one-off `linux/stdarg.h` copy — the merge brings that header along with everything else.
- Safe and reversible: it only adds files that belong to the `-common` package.

## 4. Build
```bash
sudo ./recon/power-cap-170hx.sh --watch   # separate terminal — passive card
sudo ./install.sh                         # detects 8gb/10gb, builds + installs the patched modules
```
`build.sh` re-runs conftest each build (`rm -rf kernel-open/conftest`), so once the headers are merged the
detections pass and the driver uses the modern APIs (`mmap_lock`, `vm_fault_t`, …).

## 5. Verify
```bash
nvidia-smi                                # ~65536 MiB (8gb→64gb profile)
sudo dmesg | grep SEC2_DEBUG              # POST-WRITE (SS0/SS1/CFG1/LMR), late PMA=0x0
```

## Symptom → fix quick reference
| Build error | Cause | Fix |
|---|---|---|
| `stdarg.h: No such file or directory` (via `nv_stdarg.h`) | conftest can't find `linux/stdarg.h` (in `-common`, not arch) | the header merge (step 3) |
| `mm_struct has no member 'mmap_sem'` | conftest blind → old-API fallback (renamed to `mmap_lock` in 5.8) | the header merge (step 3) |
| `conflicting types for 'vm_fault_t'` | same — conftest blind → old-API fallback | the header merge (step 3) |
| `Installed driver is 550.x, but cmpunlocker requires 610.43.0x` | wrong driver | install nvidia-open 610.43.03 (step 1) |
| build cuts off at `make: *** Error 1` | that's just make's summary | real error is above it; `grep -nE -B2 'error:' "$(ls -t logs/install_*.log \| head -1)" \| head -40` |

## Notes
- Every one of the header errors above is the **same** root cause (split-headers → conftest blindness); the
  merge fixes them in one shot. If a *new* API error appears after the merge, it's a genuine kernel-6.12
  driver-compat issue, not the headers — grab the first `error:` from the log and patch that specific call.
- Cross-refs: `install.sh` / `driver/build.sh` (the build path), [doc 04](04-toolchain-and-reproducibility.md)
  (the RE toolchain).

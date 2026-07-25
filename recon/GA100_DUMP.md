# Capturing reference registers from an uncrippled GA100

Goal: read-only BAR0 capture from a GA100 part whose PCIe gen is **not** fused
down, to diff against the CMP 170HX (`10de:20c2`).

Any of these is the same die and directly comparable:

| part | ID | notes |
|---|---|---|
| A100 PCIe 40 / 80GB | `20f1` / `20b5` | preferred — cleanest control |
| A100 SXM4 40 / 80GB | `20b0` / `20b2` | fine for everything that matters |
| A30 | `20b7` | fine |
| — | `20bb` | the third SKU; ROM already in `roms/` |

Confirm with `lspci -nn`, not the marketing name.

**SXM4 vs PCIe.** SXM4 is still a PCIe endpoint to the host, so the XVE/XP
block, the PGC6/AON island and the fuse shadows are on-die and read identically
— including the decisive `0x118f78` bit30. Two groups are weaker controls on
SXM4: the device-ID fuses (`0x8204d8`, `0x82056c`, which encode the SKU) and the
board-derived `0x132a00` lane map / `0x137xxx` per-lane registers, since SXM4
board data differs. Neither is on the critical path. Prefer PCIe mainly because
SXM4 is usually sold only as a full 8-GPU HGX node — expensive, often
container-only, sometimes behind NVSwitch.

## What this settles

The open contradiction: on the 170HX all three conditionals on the `0xcb00`
path pass (`0x11823c[11:10]==2`, `0x118f78[30]==1`, `0x12e0[0]==0`) yet the
routine does not execute — `0x118e90` never becomes `0x00068c00`.

**One dword decides it: `0x118f78` bit30.**

| on the reference part | conclusion |
|---|---|
| `1` | the gate is the differentiator; forcing it is the right target |
| `0` | `0xcb00` doesn't run on working parts either — thread is dead, drop it |

Everything else in the dump is bonus: `LnkCap2` speed vector as a working part
publishes it, the `0x820400`–`0x8207ff` fuse block (settles `0x820584` and the
`0x8207d4` group by comparison rather than inference), and the `0x132a00`
lane-map group as the packer leaves it at runtime.

## What you need from the instance

Not bare metal specifically — just:

- root, and
- **either** the ability to edit the kernel cmdline and reboot (for the
  userspace path), **or** the ability to `insmod` an unsigned module (for the
  kmod path).

That rules out container-only offerings (RunPod pods, Vast.ai default
containers) — no `insmod`, and `resource0` usually isn't exposed. A full VM
with the GPU passed through is enough; bare metal also works. Cheapest
sufficient thing is a single-A100 VM for one hour.

Two things to verify before trusting the numbers:

1. `lspci -nn | grep -i nvidia` shows a GA100 ID from the table above — not a
   vGPU/mediated ID. Virtualized functions do not give real BAR0.
2. MIG is off (`nvidia-smi -q | grep -i mig`). Partitioned state may alter what
   we're reading.

Run **with the nvidia driver loaded and after `nvidia-smi` has run**, so the
capture is post-GSP-init — that is the state the 170HX values were taken under.

## Path A — userspace (preferred)

```sh
# once, then reboot:
sudo sed -i 's/GRUB_CMDLINE_LINUX_DEFAULT="/&iomem=relaxed /' /etc/default/grub
sudo update-grub && sudo reboot

# after reboot:
nvidia-smi >/dev/null
cc -O2 -o ga100-bar0-dump ga100-bar0-dump.c
sudo ./ga100-bar0-dump > a100.txt
```

Pass a BDF as `argv[1]` if more than one GPU is present.

`mmap` failing with `EINVAL` means `iomem=relaxed` didn't take
(`cat /proc/cmdline` to confirm) — use path B.

## Path B — kmod fallback

No cmdline change needed; in-kernel `ioremap` isn't subject to
`iomem_is_exclusive()`. Needs kernel headers and no Secure Boot / lockdown.

```sh
sudo apt install -y linux-headers-$(uname -r) build-essential
make -C kmod
sudo dmesg -C
sudo insmod kmod/ga100dump.ko
sudo dmesg | sed -n 's/^.*ga100dump: //p' > a100.txt
sudo rmmod ga100dump
```

~1600 lines of `printk`. If the ring buffer wraps, boot with
`log_buf_len=8M` or read the regions in smaller pieces.

## Then

```sh
diff <(grep '^0x' 170hx.txt) <(grep '^0x' a100.txt)
```

Both tools emit `0xOFFSET 0xVALUE` lines in identical order so the dump diffs
directly. Values printed as `<priv-err>` (`0xBADFxxxx`) or `<no-decode>`
(`0xFFFFFFFF`) are not data — a difference in *which* offsets are priv-blocked
is itself a result worth keeping.

Both tools are strictly read-only: no register writes, no config-space writes,
nothing that changes the card's state. Safe on hardware that isn't yours.


## Second capture — what changed and why

The first capture ([doc 19](../docs/19-a100-reference-diff.md)) covered nine 4 KB
windows, ~36 KB of a 16 MB BAR0. Two gaps cost us afterwards:

- **`0x8e000` (XP3G) was never captured.** XP3G turned out to be an override file
  whose slot 3 mirrors a fuse, and it is the one mechanism found that defeats a
  fuse-derived value. Slots 0-2 remain untestable because there is no reference
  value to aim at.
- **`0x88000`-`0x8ffff` was sampled, not covered.** `0x88c88` accepted bits 17-18
  while refusing bit 2 in the same register, so the fuse holds down individual
  bits scattered across the span. Sampling misses them.

`--wide` now dumps 73728 bytes / 18432 lines: one contiguous `0x88000 +0x8000`
span, plus PMC, PTIMER, the legacy fuse base, PGC6, lane-map, per-lane, the fuse
region, FEAT, FPF, and FBPA.

## Read-only, and stay that way

The dumper performs no writes. **Do not run the fuse-macro or override probes on
a rented card.** `FUSE_MACRO` drives a state machine and wedged our own card
once, recoverable only by a cold power cycle — which on rented hardware may mean
someone else's reboot, or a machine you cannot get back. The capture is worth an
hour; a wedged rental is worth an argument.

Everything needed offline is a read.

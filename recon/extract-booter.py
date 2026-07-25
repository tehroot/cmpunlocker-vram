#!/usr/bin/env python3
"""Extract the GA100 Booter Load ucode from the driver's bindata blob.

usage: extract-booter.py [driver/.build/.../generated] [outdir]

NVIDIA ships the Booter as raw-deflate inside a generated C file. The PROD
image is what actually runs; DBG is the debug-signed variant.

Falcon IMEM address = file offset - 0x30 (FalconUCodeDescV2 is 48 bytes).
"""
import os, re, sys, zlib

GEN = sys.argv[1] if len(sys.argv) > 1 else \
    'driver/.build/open-gpu-kernel-modules-610.43.03/src/nvidia/generated'
OUT = sys.argv[2] if len(sys.argv) > 2 else 'fwsec'

SRC = os.path.join(GEN, 'g_bindata_kgspGetBinArchiveBooterLoadUcode_GA100.c')

def extract(txt, label):
    m = re.search(
        r'kgspBinArchiveBooterLoadUcode_GA100_BINDATA_LABEL_%s_data\[\]\s*=\s*\{(.*?)\};'
        % label, txt, re.S)
    if not m:
        return None
    blob = bytes(int(x, 16) for x in re.findall(r'0x([0-9a-fA-F]{2})', m.group(1)))
    return zlib.decompress(blob, -15)   # raw deflate, no header

def main():
    txt = open(SRC).read()
    os.makedirs(OUT, exist_ok=True)
    for label, name in (('IMAGE_PROD', 'booter_load_ga100_prod.bin'),
                        ('IMAGE_DBG',  'booter_load_ga100_dbg.bin')):
        data = extract(txt, label)
        if data is None:
            print(f'{label}: not found')
            continue
        path = os.path.join(OUT, name)
        open(path, 'wb').write(data)
        print(f'{label}: {len(data)} bytes -> {path}')

if __name__ == '__main__':
    main()

# 04 — Toolchain and reproducibility

Everything built during the session and how to re-run it. Host: macOS (Apple Silicon), Homebrew.

## 1. Pulled sources
- **NVIDIA open kernel modules** (the tag cmpunlocker patches): downloaded
  `open-gpu-kernel-modules-610.43.03` from GitHub into `driver/.build/` (gitignored). Used for the
  open-source PCIe/fuse analysis (BIF, XVE headers, GSP static config).
- **VBIOS ROMs** in `roms/`: `cmp170hx-bios-268495.rom` (TechPowerUp 268495) and `a100-bios.rom`
  (TechPowerUp 277449, A100 PG509 SXM4 ES). Both 1,044,480 bytes.

## 2. `fusedump` — BAR0 register / fuse-block dumper
Location: `driver/.build/fusedump/` (`fusedump.c`, `fusedump.py`, `README.md`, `DUMP_TARGETS.md`).
Read-only; maps the GPU's BAR0 (`/sys/bus/pci/devices/<bdf>/resource0`) and reads register offsets.
For dumping the 170HX fuse block and diffing vs an A100 (see doc 05). Requires Linux, root,
Secure Boot / kernel lockdown OFF. Build: `gcc -O2 -o fusedump fusedump.c`.

## 3. envytools (`nvbios`, `envydis`) — built from source on macOS
The reference Falcon disassembler. Non-trivial macOS build:
1. `git clone --depth 1 https://github.com/envytools/envytools.git`
2. macOS ships **bison 2.3** (too old) → `brew install bison flex` (bison 3.8.2).
3. envytools has case-colliding files (`nvbios/D.c` vs `d.c`) → build on a **case-sensitive HFS+
   disk image** *and* rename `d.c`→`dlow.c` (CMake collapses case-only object names even there).
4. `cmake .. -DBISON_EXECUTABLE=/usr/local/opt/bison/bin/bison -DFLEX_EXECUTABLE=/usr/local/opt/flex/bin/flex`
   then `make nvbios envydis`.
Binaries copied to **`driver/.build/tools/{nvbios,envydis}`**.

Usage notes:
- `envydis` reads **stdin** and needs **`-i`** for binary input, `-m fuc -V fuc5` for the Falcon ISA
  the 170HX uses (fuc5==fuc6 for this code; fuc4 worse):
  `envydis -m fuc -V fuc5 -i -b <base> file.bin`
- `nvbios` parses the container/BIT tables but **cannot decode GA100 devinit** ("Unknown chipset",
  BIT v2 fail) — it hangs on the `NVGI` wrapper (strip to the inner `0x55AA` image first).
- envydis "unknown" counts on a full linear sweep are mostly **misalignment noise**, not missing
  opcodes; flow-aligned disassembly is clean. A few opcodes (e.g. `e8 8d 00 00` at FwSec entry) are
  genuinely undecoded by **both** envydis and ghidra_falcon.

## 4. Ghidra 12.0.4 + `ghidra_falcon` + GhidraMCP (the interactive stack)
### ghidra_falcon (Falcon processor/language module)
Source pulled to `ghidra_falcon/` (v1.1, marysaka/hthh). Installed into
`~/Library/ghidra/ghidra_12.0.4_PUBLIC/Extensions/ghidra_falcon/`:
- Patched `extension.properties` version **11.1 → 12.0.4** (Ghidra checks this).
- Dropped `lib/Falcon.jar` (an 11.1-built optional loader) → pure-Sleigh, version-agnostic.
- Recompiled Sleigh with 12.0.4's compiler: `support/sleigh data/languages/falcon_v5.slaspec` (+v4).
- Verified headless: `analyzeHeadless … -processor falcon:LE:32:v5` → "Using Language/Compiler:
  falcon:LE:32:v5". Language ID for import: **`falcon:LE:32:v5`** (memory block imports as `imem`).
- Note: it will **not** appear in `File → Configure` (it's a language, not a plugin); it shows in the
  **Import** language picker.

### GhidraMCP (MCP HTTP bridge)
The user's installed GhidraMCP was **v1.4 (built for Ghidra 11.3.2)** — version-mismatched and with a
malformed `Module.manifest`, so it wasn't loading on 12.0.4. Fixed by installing the community fork
**`ismaelcaraballo-afk/GhidraMCP-12`** (built for 12.0.1 + JDK 21):
- Confirmed jar bytecode is **class-file v65 (JDK 21)** = matches Ghidra's OpenJDK 21.0.11.
- Patched its `version=2.0 → 12.0.4`; manifest already valid.
- Plugin category is **Miscellaneous** (`File → Configure → Miscellaneous`); it auto-starts an HTTP
  server on **port 8080** when a program is open.
- Gotchas learned: Ghidra loads extensions only at **full application restart**; the HTTP server binds
  when a CodeBrowser tool opens; the server has read-only + rename endpoints (`/methods`,
  `/list_functions`, `/decompile`, `/segments`, `/get_current_address`) and **no** disassemble/analyze
  action endpoint.

## 5. Ghidra headless decompile scripts
`scratchpad/ghscripts/` (Java GhidraScripts, compiled on the fly):
- **`SeedEntry.java`** — pre-script. Raw Falcon imports have **no entry point**, so auto-analysis
  disassembles nothing. This does a **linear sweep** over the whole program, seeding disassembly past
  opcode gaps, then creates the entry function. (Flow-following alone stalls at the first bad opcode.)
- **`ExportDecomp.java`** — post-script. Decompiles every function to C, writes to `arg[0]`.

Run pattern:
```
analyzeHeadless <proj> <name> -import <file.bin> -processor falcon:LE:32:v5 \
   -scriptPath <ghscripts> -preScript SeedEntry.java -postScript ExportDecomp.java <out.c> \
   -analysisTimeoutPerFile 300
```

## 6. Decompilation / analysis artifacts (in `fwsec/`)
| File | What |
|---|---|
| `inner_170hx.rom` / `inner_a100.rom` | NVGI-stripped VBIOS images |
| `fwsec_170hx_imem.bin` / `fwsec_a100_imem.bin` | FwSec (app 0x45) code — import as `falcon:LE:32:v5` |
| `fwsec_170hx_sec.bin` / `fwsec_a100_sec.bin` | FwSec HS secure tails (signing-analysis target) |
| `fwsec_170hx_full.bin` / `fwsec_a100_full.bin` | full app-0x45 partitions |
| `devinit_170hx_imem.bin` | devinit (app 0x01) code |
| `app08_170hx_imem.bin` | app 0x08 code (the fuse processor) |
| `fwsec_170hx.decomp.c` / `fwsec_a100.decomp.c` | FwSec decompilations (35 / 28 funcs) |
| `devinit_170hx.decomp.c` | devinit decompilation (81 funcs) |
| `app08_170hx.decomp.c` | app 0x08 decompilation (82 funcs) |
| `devinit_win_*.bin` | the `0x14118f78` RMW disassembly windows |
| `README.md` | file index for the folder |

Also `driver/.build/tools/`: `FWSEC_COMPARISON.md`, the built `nvbios`/`envydis`, and the
de-colorized `devinit_170hx_cluster.asm` / `devinit_a100_cluster.asm` envydis listings.

## Reproducing the key checks quickly
- **Fuse localization:** for each partition range, count LE-u32 values in `0x00820000–0x00825000`.
- **`LnkCap2` check:** single-pass scan the inner image for `0x14088000–0x14088fff` (config mirror in
  PRIV) and for `0x14000000–0x14200000` (all PCIe PRIV regs); confirm `0x14088084`/`0x88` present and
  `0x140880a4` absent.
- **Devinit `0x14118f78` decode:** `envydis -m fuc -V fuc5 -i -b 0x8800 fwsec/devinit_win_170hx.bin`.

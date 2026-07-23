# 09 — On-hardware PCIe-gen unlock attempt (beta branch) — measured result

> Analysis of a real boot log (`message.txt`) from a **beta cmpunlocker branch** that extends the unlock
> to PCIe gen, on **2× CMP 170HX** (`10de:20c2`) on a HiveOS / dual-EPYC rig, patched nvidia-open
> 610.43.03. This is the first **on-hardware** test of the PCIe-gen path, and it confirms the fuse model
> (docs 02 / 07 / 08) at register level. **Bottom line: software PCIe-gen unlock is dead — the fuse is
> real, immutable, and clamps the LTSSM independently of everything else.**

## What the beta attempts
Beyond a config-space cap spoof it opens extra PLMs (XVE `0x88xxx`, XP3G `0x8e1b0`, OPT `0x8200fc`, FEAT2
`0x823b00`) and hits the gen at three layers: the advertised caps, a **PHY-level override** (XP3G
`0x8e1xx`), and the **actual fuse** (`OPT_GEN23`). Good instrumentation — it prints a "PCIe pre / retrain
done / Cap oracle" register dump each pass.

## Result: advertises Gen2, trains Gen1 (register trace, GPU0)
| Register | Pre | Post | Meaning |
|---|---|---|---|
| `CAP` (LnkCap, cfg 0x84) | `0x00456101` | `0x00456102` | Max link speed **Gen1 → Gen2** |
| `CAP2` (LnkCap2, cfg 0xa4) | `0x00000002` | `0x00000006` | Supported vector **Gen1 → Gen1+Gen2** |
| `LC2` (LnkCtl2, cfg 0xa8) | `0x00000001` | `0x00000002` | Target speed **Gen1 → Gen2** |
| `speed` / `STAT` | `1` / `0x10410040` | **`1`** / `0x10410040` | **negotiated stays Gen1 x4** |

Every *advertised* layer now says Gen2; `speed=1` before and after, and `STAT=0x10410040` decodes to
CurrentLinkSpeed=Gen1, width=x4. **The link never leaves Gen1 x4.** This is the developer's "advertising
5 GT/s, training 2.5" — measured, and exactly the advertised-≠-negotiated model.

## The decisive finding: the gen fuse is immutable
`OPT_GEN23` @ **`0x82057c`** reads `0x00000001` (Gen2/3 disabled). Two writes to `0x0` → readback stays
`0x1`; the log prints **`FAILED to set OPT_GEN23`**. Under the *same* Booter status (`0xffff`) that
successfully lands SS0/SS1/CFG1/LMR, the PLM opens (`→0xffffffff`), and the XP3G override values, **this
one register rejects the write** — so it is genuinely immutable to the HS-Booter primitive, not a Booter
artifact. This is the register-level confirmation of `FUSE_PCIE_GEN23_DIS=1` (doc 08), the empirical close
of approach #1 (doc 07) for gen, and it **pins the exact address the gist left unspecified**.

## The PHY override took — and still didn't help (the double-lock, observed)
XP3G block: `PLM 0x8e1b0`, `OVR0 0x8e110`, `VAL0 0x8e120`, `OVR3 0x8e11c`, `VAL3 0x8e12c`. The beta wrote
`OVR3=0x4, VAL3=0x00200000` and they **stuck** (readback matches) — yet the retrain still returned
`speed=1`. So a *working* PHY-level override does not raise the trained rate while `OPT_GEN23` stays `1`:
the fuse clamps the LTSSM on its own. This is doc 08's "double-locked," observed directly.

## Other spoofs (all cosmetic, none changed speed)
- `VSEC_DEVICE` `0x800→0x801`: **failed** (stays `0x800`).
- `PRIV_MISC_1` `0x20340500→0x20342d00`: took.
- `OPT` reads: `OPT_GEN23=0x00000001` on the "Cap oracle" line even after opening the XP3G PLM.

## Caveats / gaps in this test
- **AER is disabled** — the kernel cmdline has `pci=noaer`, so there is **no** "did the LTSSM attempt
  Gen2 and fall back, or never try?" signal. Re-running without `pci=noaer` yields the AER/recovery screen
  (the Tier-1 PLL test in doc 06). Until then we can't distinguish *rate-advertisement fuse* from *hard
  PLL fuse*.
- The XP3G override could not be validated **with the fuse flipped** (the fuse write failed), so we can't
  isolate whether the PHY would train Gen2 if `OPT_GEN23` were `0`.

## Unaffected: compute + memory unlock works
Despite the `0x31`/`0xffff` Booter spam on the PLM passes (harmless per the README), the **final**
BooterLoad succeeded (`status=0x0`), SS0/SS1/CFG1/LMR verified in POST-BooterLoad, PMA extended
(`status=0x0`), and both GPUs initialized. The PCIe portion is a **cosmetic no-op** on top of a working
compute/memory unlock. (These are 8 GB cards at `CFG1=0x02779000` / tier 77 → 64 GB — the doc 08 aliasing
flag applies to exactly this config; unrelated to the PCIe result but the card to run the write-verify on.)

## What this settles
- **Resolves doc 02's "Gen2 reconciliation":** the card *advertises* Gen2 (spoofable config space) and
  *trains* Gen1 (fuse-clamped LTSSM). Both prior observations were correct — different layers.
- **Confirms docs 02 / 05 / 07 / 08:** PCIe gen is a genuine, immutable-fuse dead-end for software, now
  with the exact register (`OPT_GEN23 0x82057c`) and a failed on-hardware write.
- **Reframes the retimer (doc 06 #4):** the block is at the **LTSSM/fuse**, not the advertisement — so a
  retimer that rewrites Rate IDs in TS ordered sets likely cannot help either, because the GPU-facing
  segment's LTSSM still refuses to train Gen2. The retimer's odds drop accordingly.

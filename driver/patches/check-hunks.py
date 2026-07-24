#!/usr/bin/env python3
"""Validate unified-diff hunk line counts in driver/patches/*.patch.

These patches are hand-maintained, and a wrong @@ count makes `patch` either
misapply or reject silently-ish. This has bitten the tree before (see
"Fix malformed 0007 hunk line counts"). Run it after editing any patch:

    python3 driver/patches/check-hunks.py            # all patches
    python3 driver/patches/check-hunks.py 0007-*.patch

Exit 0 = all hunks consistent, 1 = at least one mismatch.
"""
import glob
import os
import re
import sys

HUNK = re.compile(r"^@@ -(\d+)(?:,(\d+))? \+(\d+)(?:,(\d+))? @@")

# Lines that terminate a hunk body rather than belonging to it.
FILE_HEADERS = ("--- ", "+++ ", "diff ", "index ", "new file mode", "deleted file mode")


def check(path):
    lines = open(path, encoding="utf-8").read().split("\n")
    # A trailing newline yields a final "" element that is not part of any hunk.
    if lines and lines[-1] == "":
        lines.pop()

    bad = []
    cur = None

    def close():
        if cur is None:
            return
        old = cur["ctx"] + cur["rm"]
        new = cur["ctx"] + cur["add"]
        if old != cur["old"] or new != cur["new"]:
            bad.append((cur["line"], cur["hdr"], old, new, cur["old"], cur["new"]))

    for i, line in enumerate(lines, 1):
        m = HUNK.match(line)
        if m:
            close()
            cur = {
                "line": i,
                "hdr": line,
                "old": int(m.group(2) or 1),
                "new": int(m.group(4) or 1),
                "ctx": 0,
                "rm": 0,
                "add": 0,
            }
        elif cur is not None:
            # A new file header ends the current hunk. These patches are
            # `diff -Naur` output, so a "diff ..." line precedes each "--- ";
            # counting it as context inflates both sides by one.
            if line.startswith(FILE_HEADERS):
                close()
                cur = None
            elif line.startswith("+"):
                cur["add"] += 1
            elif line.startswith("-"):
                cur["rm"] += 1
            elif line.startswith("\\"):  # "\ No newline at end of file"
                pass
            else:
                cur["ctx"] += 1
    close()

    name = os.path.basename(path)
    for ln, hdr, old, new, dold, dnew in bad:
        print(f"{name}:{ln}: {hdr}")
        print(f"    actual old={old} new={new}  declared old={dold} new={dnew}")
    if not bad:
        print(f"{name}: OK")
    return len(bad)


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    args = sys.argv[1:]
    if args:
        paths = [p for a in args for p in glob.glob(a)]
    else:
        paths = sorted(glob.glob(os.path.join(here, "*.patch")))
    if not paths:
        print("no patches found", file=sys.stderr)
        return 1
    return 1 if sum(check(p) for p in paths) else 0


if __name__ == "__main__":
    sys.exit(main())

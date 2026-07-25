#!/usr/bin/env bash
# dump-diff.sh REF.txt TARGET.txt [--mask FILE]
#
# Diff two ga100-bar0-dump captures. REF is the uncrippled part (A100/A30),
# TARGET is the CMP 170HX. Both must be --wide captures so the offset lists
# line up.
#
# --mask FILE drops offsets listed one-per-line in FILE. Use it with
# recon/volatile-offsets.txt, derived from two back-to-back captures of the
# SAME card: anything that moved there is a live counter or PHY status word,
# not a static difference, and would otherwise be a false positive.
#
# Output is grouped by BAR0 region so a block of differences reads as one
# finding rather than 40 unrelated lines.
set -euo pipefail

REF=${1:?usage: dump-diff.sh REF.txt TARGET.txt [--mask FILE]}
TGT=${2:?usage: dump-diff.sh REF.txt TARGET.txt [--mask FILE]}
shift 2

MASK=""
while [ $# -gt 0 ]; do
	case $1 in
		--mask) MASK=$2; shift 2 ;;
		*) echo "unknown arg: $1" >&2; exit 1 ;;
	esac
done

vals() {
	grep -E '^0x[0-9a-f]+ 0x' "$1" | awk '{print $1, $2}' | sort -u \
	| { [ -n "$MASK" ] && grep -vwFf <(grep -E '^0x' "$MASK") || cat; }
}

echo "# ref    = $REF   $(grep -m1 '^# bdf=' "$REF")"
echo "# target = $TGT   $(grep -m1 '^# bdf=' "$TGT")"
echo

join <(vals "$REF") <(vals "$TGT") \
| awk '$2 != $3 { print }' \
| while read -r off r t; do
	case $off in
		0x000[0-9a-f]*) reg="PMC" ;;
		0x0009*)        reg="PTIMER/misc" ;;
		0x0021*)        reg="fuse-ctrl" ;;
		0x0088*)        reg="XVE" ;;
		0x008c*)        reg="XP" ;;
		0x0118*)        reg="PGC6/AON" ;;
		0x0132*)        reg="lane-map" ;;
		0x0137*)        reg="per-lane" ;;
		0x0820*)        reg="FUSE" ;;
		*)              reg="?" ;;
	esac
	printf '%-14s %s  ref=%s  tgt=%s  xor=0x%08x\n' \
		"$reg" "$off" "$r" "$t" "$(( r ^ t ))"
done | sort -k1,1 -k2,2

echo
echo "=== counts by region ==="
join <(vals "$REF") <(vals "$TGT") | awk '$2 != $3 {print substr($1,1,6)}' \
	| sort | uniq -c | sort -rn

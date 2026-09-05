#!/usr/bin/env bash
# Two things this model must never get wrong. Both are invisible in a
# preview render and both are expensive:
#
#   1. part="obstruction" - body() AND the volume the panel sweeps on its
#      way to its seat. The panel is a 170 x 111 x 1.3 mm sheet of glass,
#      loaded from the open back; it cannot be trimmed, bent or persuaded.
#      Anything left in its path is a case that gets cut open with a saw.
#   2. part="clash" - body() AND lid_asm(). Shared solid is a lid that will
#      not close, or that closes by cracking whatever is under it.
#   3. part="board_clash" - plate() AND the seated PCB, and part="board_sweep"
#      - plate() AND the path the board slides along to get there. The board
#      is held by its outline, having no mounting holes, so every rail, lip,
#      stop and boss has to clear both the board and its way in.
#
# Judged by VOLUME, not by facet count: parts that merely touch (the lid
# landing on the body's rear shoulder) intersect in a zero-thickness sheet
# that CGAL still reports as hundreds of facets.
set -euo pipefail
cd "$(dirname "$0")"

scad=${1:-epaper_frame.scad}
tol=0.5          # mm^3; below this it is coincident faces, not interference
out=$(mktemp -d); trap 'rm -rf "$out"' EXIT

volume() {   # mm^3 enclosed by an ASCII STL, 0 if the file is absent
python3 - "$1" <<'PY'
import sys, os
p = sys.argv[1]
if not os.path.exists(p):
    print(0.0); raise SystemExit
v, t = [], 0.0
for line in open(p):
    line = line.split()
    if line and line[0] == "vertex":
        v.append(tuple(map(float, line[1:4])))
        if len(v) == 3:
            (ax,ay,az),(bx,by,bz),(cx,cy,cz) = v
            t += (ax*(by*cz-bz*cy) - ay*(bx*cz-bz*cx) + az*(bx*cy-by*cx)) / 6.0
            v = []
print(abs(t))
PY
}

check() {   # $1 = part name, $2 = what a hit means
    local part=$1 meaning=$2 vol
    if ! openscad -o "$out/$part.stl" -D "part=\"$part\"" "$scad" 2>"$out/$part.log" \
       && ! grep -q "Current top level object is empty" "$out/$part.log"; then
        sed -n '/ERROR\|Assert/p' "$out/$part.log" >&2
        echo "FAIL: $scad did not render part=\"$part\"" >&2
        return 1
    fi
    if grep -q "Assertion" "$out/$part.log"; then
        grep "Assertion" "$out/$part.log" >&2
        return 1                      # a failed assert renders nothing: not a pass
    fi
    vol=$(volume "$out/$part.stl")
    if [ "$(python3 -c "print(1 if $vol > $tol else 0)")" = 1 ]; then
        echo "FAIL: ${vol} mm3 - $meaning" >&2
        echo "      openscad -o bad.stl -D 'part=\"$part\"' $scad" >&2
        return 1
    fi
    printf '  %-12s %8.3f mm3\n' "$part" "$vol"
}

check obstruction "case material sits in the panel insertion path"
check clash       "the body and the lid overlap"
check board_clash "the board carrier occupies the board's own space"
check board_sweep "the carrier blocks the board sliding in"

# the asserts inside the model: clearances, boss placement, corner relief
openscad -o "$out/body.stl" -D 'part="body"' "$scad" 2>"$out/body.log"

echo "PASS: panel path clear, lid clears the body, all fit asserts hold"

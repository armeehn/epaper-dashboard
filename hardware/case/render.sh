#!/usr/bin/env bash
# Regenerate every checked-in STL and preview from epaper_frame.scad.
# Run after any parameter change so the repo never ships a mesh that
# disagrees with the model.
set -euo pipefail
cd "$(dirname "$0")"

for p in body lid plate stand; do
    openscad --export-format binstl -o "$p.stl" -D "part=\"$p\"" epaper_frame.scad
done

IMG=--imgsize=1400,1000
shot() {   # $1 = file, $2 = part, rest = extra openscad flags
    local f=$1 p=$2; shift 2
    xvfb-run -a openscad -o "previews/$f.png" $IMG --colorscheme=Tomorrow \
             -D "part=\"$p\"" "$@" epaper_frame.scad
}

# the case front is -Z, so a face-on look is a 180 deg X rotation
shot v_body_ortho_front body     --projection=o --camera=0,3.2,12,180,0,0,260
shot v_lid_iso          lid      --camera=0,3.2,12,58,0,20,470
shot v_stand            stand    --camera=0,0,10,62,0,28,420
shot p_front            assembly --camera=0,3.2,12,168,0,8,470
shot p_back             assembly --camera=0,3.2,12,20,0,-10,470
shot p_exploded         exploded --camera=0,3.2,45,66,0,22,620

echo "STLs and previews regenerated"

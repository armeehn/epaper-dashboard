#!/usr/bin/env bash
# Regenerate every STL and preview of the RUTA series from serie.scad, and
# run the fit checks first: a mesh is only shipped from a model that fits.
set -euo pipefail
cd "$(dirname "$0")"

../case/fit_check.sh ../cases/serie.scad   # it cds into ../case first

mkdir -p stl previews
for p in frame back_wall hook back_magnet stand; do
    openscad --export-format binstl -o "stl/$p.stl" -D "part=\"$p\"" serie.scad 2>/dev/null
done

IMG=--imgsize=1400,1000
shot() {   # $1 = file, $2 = part, $3 = camera
    xvfb-run -a openscad -o "previews/$1.png" $IMG --colorscheme=Tomorrow \
             --camera="$3" -D "part=\"$2\"" serie.scad 2>/dev/null
}
# the case front is -z: a face-on look is a 180 deg X rotation
shot ruta_front  ruta     0,3,10,168,0,10,470
shot ruta_back   ruta     0,3,10,30,0,-15,470
shot krok_back   krok     0,3,10,30,0,-15,470
shot fast_back   fast     0,3,10,30,0,-15,470
shot stod        stod     0,20,60,70,0,25,520
shot exploded    exploded 0,20,80,60,0,30,760
echo "STLs and previews regenerated"

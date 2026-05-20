#!/usr/bin/env bash
# Downloads Solar System Scope's 2K textures into this folder with the
# filenames our Solar System app expects. Run with:
#   cd "/Users/daryacetinkaya/Desktop/solar-system-main 3/textures"
#   bash download_textures.sh
#
# Textures licensed CC-BY 4.0 — https://www.solarsystemscope.com/textures/

set -e
cd "$(dirname "$0")"

base="https://www.solarsystemscope.com/textures/download"

declare -a items=(
  "sun.jpg|2k_sun.jpg"
  "mercury.jpg|2k_mercury.jpg"
  "venus.jpg|2k_venus_surface.jpg"
  "earth.jpg|2k_earth_daymap.jpg"
  "moon.jpg|2k_moon.jpg"
  "mars.jpg|2k_mars.jpg"
  "jupiter.jpg|2k_jupiter.jpg"
  "saturn.jpg|2k_saturn.jpg"
  "uranus.jpg|2k_uranus.jpg"
  "neptune.jpg|2k_neptune.jpg"
)

for pair in "${items[@]}"; do
  out="${pair%%|*}"
  src="${pair##*|}"
  echo "→ ${out}"
  curl -fsSL "${base}/${src}" -o "${out}"
done

echo
echo "Done. Got these files:"
ls -lh *.jpg

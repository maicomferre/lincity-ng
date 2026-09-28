#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../../.."

# Map the measured west/east/south platform corners to the native 3x3 grid.
magick data/models/petroleum/oilwell-master.png -virtual-pixel transparent \
  -define distort:viewport=384x352+0+0 \
  -distort Affine '34,725 0,256 1506,725 384,256 768,979 192,352' \
  +repage -strip PNG32:data/images/tiles/oilwell-empty.png
magick data/models/petroleum/refinery-master.png -virtual-pixel transparent \
  -define distort:viewport=384x384+0+0 \
  -distort Affine '222,623 0,288 1564,623 384,288 887,814 192,384' \
  +repage -strip PNG32:data/images/tiles/refinery-empty.png

# These registered resources are not currently selected by the modules.
for building in oilwell refinery; do
  for state in low med full; do
    cp "data/images/tiles/${building}-empty.png" \
      "data/images/tiles/${building}-${state}.png"
  done
done

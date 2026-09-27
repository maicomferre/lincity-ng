#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../../.."

# BUG-23: the ground's south corner is (256,320) in the exported canvas.
magick data/models/sea-farm/base-master.png \
  -crop 1486x786+50+122 +repage -resize '512x320!' -strip \
  PNG32:data/images/tiles/seafarm0.png
for frame in 3 7 11 15; do
  cp data/images/tiles/seafarm0.png "data/images/tiles/seafarm${frame}.png"
done
magick data/models/sea-farm/icon-master.png -trim +repage -resize 30x30 \
  -gravity center -background none -extent 32x32 -strip \
  PNG32:data/images/gui/buttonpanel/parks/seafarm.png

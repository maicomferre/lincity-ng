#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../../.."
for icon in mining/oilwell industry/refinery; do
  magick -background none "data/images/gui/buttonpanel/$icon.svg" \
    -strip "PNG32:data/images/gui/buttonpanel/$icon.png"
done

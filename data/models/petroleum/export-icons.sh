#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../../.."
for icon in mining/oilwell industry/refinery; do
  python3 data/models/petroleum/render-icon.py "data/images/gui/buttonpanel/$icon.svg" | \
    magick png:- \
    -strip "PNG32:data/images/gui/buttonpanel/$icon.png"
done

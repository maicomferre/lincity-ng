# FEAT-07b — compact fire engine

Original procedural artwork created for this fork, 2026-09-29, with
AI-assisted Blender Python modeling. No downloaded model, external texture,
image-generation service, or third-party reference image was used. New source
and exported artwork follow the project's data licensing terms in
`COPYING-data.txt` (GPL / CC-BY-SA v2).

`render.py` is the authoritative editable source. `firetruck.blend` retains
the assembled model, named parts, materials, fixed camera and lights. It is
saved before pose/flash iteration; rerun the script to regenerate the family.

## Regeneration

From the repository root, with Blender 5.0.1:

```sh
blender --background --threads 4 --python data/models/firetruck/render.py
magick montage data/images/tiles/firetruck-*.png \
  -background '#7b8769' -geometry 240x192+5+5 -tile 8x4 \
  data/models/firetruck/contact-sheet.png
```

CPU Cycles, 32 samples, no denoising, 320×256 supersampling, then Blender
image scaling to 80×64. PNG RGBA8 with genuine transparency; no background
plane, terrain, cast shadow or halo is baked in. Orthographic projection,
30° elevation and 45° azimuth give the 2:1 ground grid. Lighting is constant
from upper left with soft fill. No simulation or random state is consulted.

## Export contract

All 32 PNGs have canvas **80×64**, ground-center pivot **(40,48)** and
identical framing. Length projects to approximately 50 pixels horizontally;
the taller cabin, mirrors and ladder fit within the canvas even on ramps.
Do not auto-trim or independently resize frames.

| Indices | Meaning |
|---|---|
| 00–07 | Flat poses: ↙ ↓ ↘ → ↗ ↑ ↖ ← in screen space |
| 08–11 | Uphill, flat directions 0, 2, 4, 6; pitch +15° nose-up |
| 12–15 | Downhill, same directions; pitch −15° nose-down |
| 16–31 | Same poses as 00–15, opposite emergency-light phase |

The model faces local +X. Pose yaw begins at −90° and advances 45°;
local Y rotation is −15° uphill / +15° downhill about ground center.
Alpha is identical in each pair `i` / `i+16`. Only the red/blue light lenses
alternate camera-visible emission, without changing geometry or indirectly
lighting the rest of the truck. There are no independent generated views
whose proportions or pivot can drift.

The model includes cab glazing/dividers, mirrors, grille/headlights,
reflective cream stripes, aluminium shutter compartments and grooves,
tyres/hubs, rear fittings, a hose reel, roof ladder and emergency bar.
The small-size exports favor those silhouettes over fine text/logos.

`contact-sheet.png` shows all 32 exports enlarged on a muted green background
for review. It is a preview, not an installed game texture. Real in-engine
scene and animation validation remains the integration test's responsibility.

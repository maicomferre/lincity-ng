# Sea Farm artwork — BUG-23

Created 2026-09-27 with the built-in image generation tool, under the visual
direction of the fork's maintainer and coding assistant. The tool did not
report a model version or reproducible seed. No external reference image was
submitted for the base. The generated base was the subject reference for the
icon. Masters retain the generated alpha; neither is a Blender model.

The contributions in this directory and their exported PNGs use the project's
data licensing terms in `COPYING-data.txt` (GPL / CC-BY-SA v2). This note
records the use of AI; it does not attribute the generated drawing to an
upstream artist.

## Export contract

Run `bash data/models/sea-farm/export.sh` from the repository (ImageMagick 7).
The fixed crop removes transparent margins and fixes the ground footprint;
the export is 512×320 RGBA with pivot (256,320), matching `images.xml`.
The dedicated help icon is 32×32 RGBA.

`seafarm0/3/7/11/15.png` deliberately contain identical static artwork.
SeaFarm currently does not select visual states in `animate()`. The five
filenames satisfy the existing catalog without introducing an implied
production animation. Distinct states and a construction-menu button belong
to the later FEAT-07a work, not this missing-assets fix.

## Validation and preview

The existing `game_flow_reset_and_loading_barrier` regression checks all five
textures, canvas dimensions, pivots and the help icon in installed app data.
It also covers world reset. To export a renderer-generated comparison with
the original farm, run:

```sh
LINCITYNG_TEST_SPRITE_PREVIEW=build/logs/20260927-bug23-preview \
  bash contrib/devlog.sh sim --filter game_flow_reset_and_loading_barrier \
  --tag bug23-preview
```

Preview indices 0–4 correspond to zoom 4×, 2×, 1×, 0.5× and 0.25×.
The scene is a render fixture, not a test of construction eligibility.

SHA-256 of the retained masters and initial exports:

| File | SHA-256 |
|---|---|
| base-master.png | `425ad8517ae45e09bdc9ed266a4fa73b767762a9f5263c920d8de64035a3ea94` |
| icon-master.png | `dc0c5e922c77b07c1e97ec76a9eb9ace2e1ddfe0acf19fe9f18298e2c069e4fe` |
| seafarm0.png (all five tiles) | `7b7328fc8a1592fa75134d911ae30b1a8e595d26f7399b546d67c14349ee64d5` |
| seafarm.png (icon) | `4cd52c4133d3dc47b4e65fa9bafe3910df62ef9e06405436c175b4537ad221e1` |

## Base prompt (built-in generation)

Use case: stylized-concept. Asset type: production PNG game sprite for
LinCity-NG, Sea Farm aquaculture building. Create ONE single isolated sprite,
real transparent alpha background. Orthographic dimetric camera: ground axes
slope +/- 1/2, diamond ground footprint width twice its depth (2:1), view down
at 30 degrees, no perspective convergence. A modest working aquaculture
facility on a square 4x4 tile plot: low rectangular concrete fish-rearing
ponds with dark teal water, narrow tan wooden or concrete walkways, small
timber service shed with muted terracotta roof near rear, some net frames,
pipework and a few barrels. It must read as fish farming, not a terrestrial
crop farm. Style: restrained pre-rendered low-poly 2000s city-builder artwork,
realistic proportions, simple readable shapes and low-detail materials,
compatible with LinCity-NG legacy sprites. Soft sunlight from upper left,
subdued gray/brown/teal palette. The complete footprint is a neat 2:1 diamond,
no scenery outside it; no deep floating island slab, no ocean rectangle, no
horizon, no background, no UI, no text, no logos, no checkerboard. Object fully
visible, centered with transparent margins. Use a 512 by 320 pixel canvas if
possible; the diamond ground corners should be top (256,64), left (0,192),
bottom (256,320), right (512,192), with shed extending upward. This is the
static base sprite seafarm0; no people, no fish jumping, no state or production
indicators. Save the generated image and return its filesystem path for
integrating into the game.

## Icon prompt (built-in generation, base-master as subject reference)

Use case: stylized-concept. Input image is a SUBJECT reference, not an edit
target. Create one small game UI icon for this aquaculture farm in LinCity-NG.
True transparent background. A simplified single teal rectangular fish pond
with tan rim, a short net and one small terracotta-roof service hut, in
orthographic 2:1 dimetric view, soft top-left light. Strong simple silhouette
suitable for export at 32x32 pixels. Restrained legacy pre-rendered
city-building game style. No circle, badge, UI border, floor outside pond,
text, numbers, logo, checkerboard or background. Center object with slight
transparent margin. This is a dedicated tiny help icon, not a detailed
illustration.

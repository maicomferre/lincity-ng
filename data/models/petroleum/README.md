# Petroleum artwork — BUG-24

Created 2026-09-27 with the built-in image generation tool. No external
reference image was submitted. The tool did not report a model version or
reproducible seed. These retained masters are raster sources, not Blender
models. New contributions and exports use the project's data licensing terms
in `COPYING-data.txt` (GPL / CC-BY-SA v2); no upstream artist is credited
for these AI-generated drawings.

## Scope and export contract

Run `bash data/models/petroleum/export.sh` from the repository with
ImageMagick 7 (validated with 7.1.2-18 Q16).

| Family | Canvas RGBA | Pivot | Ground diamond |
|---|---|---|---|
| OilWell | 384×352 | (192,352) | 384×192 |
| Refinery | 384×384 | (192,384) | 384×192 |

The generated ground planes were shallower than the requested 2:1 grid.
The export maps three measured platform corners (west/east/south) through
an affine transform to the native footprint. This normalizes both scale
and projection; it is not an HD renderer mode. Pixel coordinates are in
`export.sh`, so cropping and pivot do not depend on automatic trim.

The oil master corners are (34,725), (1506,725), (768,979); the refinery
corners are (222,623), (1564,623), (887,814). Output west/east corners sit
96 pixels above the bottom pivot. Machinery stays above the platform;
no extra terrain block or outer shadow is introduced. Original alpha is
preserved and resampled with transparent virtual pixels.

Each family exports the SAME static base to empty/low/med/full. The modules
currently select only OilWell or Refinery; their animation index is reduced
modulo the single catalog frame. These aliases are not four inventory states
or an eight-frame animation. FEAT-07a remains responsible for states,
animation, editable 3D production and construction-menu integration.
No module, footprint, economy, save format or simulation RNG changed.

The eight RGBA8 textures account for 4.3125 MiB of pixel data (previously
32 KiB), excluding driver overhead and temporary loading surfaces. This is
a dimension-based budget, not a measured memory or frame-time benchmark.

The tool preview appeared to show a halo on the oil sprite, prompting an
unused transparency-edit candidate. Compositing the ORIGINAL RGBA over a
solid background confirmed the surrounding pixels were transparent; the
original was retained without a manual mask or generated background repair.

## Validation and preview

The installed-data regression `game_flow_reset_and_loading_barrier` checks
all eight resources: texture presence, one frame, native dimensions and
pivots. The optional fixture exports both Sea Farm and petroleum scenes:

```sh
LINCITYNG_TEST_SPRITE_PREVIEW=build/logs/bug24-preview \
  bash contrib/devlog.sh sim --filter game_flow_reset_and_loading_barrier \
  --tag bug24-preview
```

`petroleum-zoom-0..4.png` correspond to 4×, 2×, 1×, 0.5× and 0.25×.
The petroleum scene places both facilities next to a road and the existing
3×3 light industry. This is direct placement for visual comparison, not a
construction-eligibility test. At high zoom the camera may crop the scene;
1× is the primary footprint comparison. The harness uses the SDL renderer,
not OpenGL. Static states and menu buttons remain outside this fix.

SHA-256:

| File | SHA-256 |
|---|---|
| oilwell-master.png | `c2ca8e09f13497f6b532f44daccc6c81adcb6b76dd1e0aa33fdc74ad7da7072c` |
| refinery-master.png | `143979994018a2d2f9562ddac3be8394d7338e0ed92a8c96b0be89f5fbb4438c` |
| oilwell-empty.png (all four aliases) | `669e4349fd5406ffc8e06bdc6c40aaefce402d3d7e89f06e4ddf661c09205951` |
| refinery-empty.png (all four aliases) | `2b64443108fc5ca6d25672e9cb5c7332637527ed6d5a2f32a8c4b0065ac0e6a9` |

## Oil well prompt (built-in generation)

Use case: stylized-concept. Asset type: transparent PNG building sprite for LinCity-NG, a legacy pre-rendered dimetric city simulation. Generate ONE onshore oil well facility: a clearly readable rusty red pumpjack on steel supports, a small beige maintenance hut and one squat grey oil tank connected with thick simple pipes. Muted earthy industrial palette, restrained detail, matte low-poly pre-rendered game art, light from upper left. No cartoon outlines. Orthographic camera, symmetric dimetric 2:1 ground grid (ground edges slope exactly 1 vertical for 2 horizontal, not 30-degree isometric). Entire facility sits INSIDE one thin FLAT concrete diamond platform representing 3 by 3 tiles. Diamond width is exactly twice its depth. Left and right corners at same height, north and south corners vertically aligned with image center. Platform paper-thin, NO thick floating terrain block, NO outside shadow or objects beyond diamond. The complete platform and machinery are fully visible with small transparent margins; no clipping. Genuinely transparent RGBA background, no checkerboard, no backdrop, no horizon, no text, no watermark. Static neutral state, no smoke, no flames, no people or cars. Output as a single isolated sprite, not an icon sheet. We will reduce to 384 pixel ground width, so prioritize large legible forms.

## Refinery prompt (built-in generation)

Use case: stylized-concept. Asset type: transparent PNG building sprite for LinCity-NG, legacy pre-rendered dimetric city simulation. Generate ONE small petroleum refinery: two steel distillation columns of different heights, clustered grey cylindrical tanks, thick readable pipes and a compact brick equipment building. Muted grey metal, rusty red and beige palette. Matte restrained low-poly pre-rendered industrial game art, soft light from upper left, strong legible silhouette at 384px ground width. Orthographic camera with symmetric DIMETRIC 2:1 ground grid; ground edges slope 1 vertical for 2 horizontal, NOT standard 30-degree isometric. All equipment stays INSIDE a single thin FLAT concrete diamond platform representing 3 by 3 ground tiles. Ground diamond width EXACTLY TWICE its full north-south depth; left and right corners level, north and south tips aligned to image center. Complete machinery and platform visible with small transparent margins. Platform paper-thin, NO floating terrain cube. True transparent RGBA background, including outside the platform; NO haze, NO glow, NO cast shadow outside the platform, no backdrop, no checkerboard, no horizon. No text, labels, logos, people, cars, smoke, flames or border. A single isolated static building sprite, not a sheet. Keep details simple enough to read alongside older city game industrial buildings.

## Unused oil transparency-edit prompt

Use case: background-extraction. Edit target: the supplied oil pumpjack sprite. Change ONLY the transparency outside the machinery and concrete diamond platform: remove the entire grey/brown hazy glow and all outside cast shadows. Everything outside the clean cutout of platform plus machinery must be exactly alpha=0, genuinely transparent, not black or checkerboard. Preserve every solid object, platform geometry, colors, camera, framing, dimensions, and image positions unchanged. Keep natural antialiased edges, no fringe. No new elements, no text.

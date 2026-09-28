# FPoSu: CS2 style static crosshair (fposu_crosshair)

Date: 2026-09-28

## Goal

Let FPoSu players replace the skin cursor with a CS2 style static crosshair that
copies the settings CS2 exposes since the "Rush Hour" update (2026-09-22, hotfix 09-24):
everything is in integer screen pixels, no negative gap, outline is off / full / half.

| Setting | ConVar | Range | Default |
|---|---|---|---|
| Enable | `fposu_crosshair` | bool | off |
| Color | `fposu_crosshair_color_r/g/b` | 0..255 | 0 / 255 / 0 |
| Alpha | `fposu_crosshair_alpha` | 0..255 | 255 |
| Outline | `fposu_crosshair_outline` | 0 off, 1 full, 2 half (same numbering as `cl_crosshair_drawoutline`) | 1 |
| Thickness | `fposu_crosshair_thickness` | 0..31 (0 draws as 1, CS2 "minimum 1 pixel") | 1 |
| Center dot | `fposu_crosshair_dot` | bool | off |
| Length | `fposu_crosshair_length` | 0..255 | 8 |
| Gap | `fposu_crosshair_gap` | 0..128 | 4 |

## Geometry (`OsuFPoSuCrosshair`, engine independent)

- `t` = thickness, `half = t / 2`. The center block is the `t x t` square whose
  top-left is `(cx - half, cy - half)`; with odd `t` the center pixel is its middle.
- `gap` follows CS2: it is the distance in pixels from the center pixel to the
  innermost bar pixel (verified against a real CS2 config: CS2 gap 3 == 2 empty
  pixels per side). So the bars are `length` px long, `t` px thick, and start
  `gap - 1` px outside the center block; gap 1 leaves only the center block empty.
  Gap 0 pulls the bars one pixel into the center: the center block is filled and
  the bars keep `length - 1` px outside of it (a solid cross). For `t > 1` the
  same rule is applied from the edge of the center block (assumption, untested
  against CS2).
- Center dot = the center block itself (emitted once even when gap 0 fills it).
- Outline is 1 px black at the crosshair alpha. Full = a rect one pixel larger on
  every side. Half = only the top edge (including the top-left corner) and the left
  edge, which is what CS2 describes as "only the top-left portions of the outline".
- `build()` returns outline rects first, then fills, so fills always paint on top.
- Values are literal screen pixels (no resolution rescaling).

## Integration

- `OsuModFPoSu.cpp`: the ConVars above.
- `OsuHUD::drawFPoSuCrosshair()`: reads the ConVars, clamps, draws `fillRect`s.
- `Osu::draw()`: while the FPoSu cursor is pinned to the screen center and
  `fposu_crosshair` is on, draw the crosshair instead of `drawCursor()`. The normal
  cursor is still used in menus / 4D mode with menus open, and the FPoSu cursor
  trail (`fposu_draw_cursor_trail`) is untouched.
- Options → FPoSu → "FPoSu - Crosshair": enable checkbox, R/G/B/Alpha, Outline
  (label Off/Full/Half), Thickness, Center Dot, Length, Gap. CTRL+O in gameplay
  gives a live preview.
- `tests/OsuFPoSuCrosshairTest.cpp`: host-side test of the geometry and clamping.

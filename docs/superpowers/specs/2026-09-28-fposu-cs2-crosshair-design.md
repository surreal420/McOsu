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
| Reference height | `fposu_crosshair_screen_height` | 0 (off) .. 2160 | 0 |

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

## Share codes (import / export)

CS2 share codes look like `CSGO-xxxxx-xxxxx-xxxxx-xxxxx-xxxxx`. The 25 characters are
base-57 digits (alphabet `ABCDEFGHJKLMNOPQRSTUVWXYZabcdefhijkmnopqrstuvwxyz23456789`,
least significant digit first) of an 18 byte big endian number:

| Byte | Content |
|---|---|
| 0 | checksum = sum(bytes 1..17) & 0xff |
| 1 | version: 1 = pre Rush Hour (rejected), 3 / 4 = current |
| 2 | style (low 4 bits), recoil bit 4, outline bit 5 (v3 only), dot bit 6, T-style bit 7 |
| 3..6 | r, g, b, a |
| 7 | gap |
| 8 | length |
| 9 | dynamic spread limit |
| 10..13 | little endian packed: splitdist(7) / inner alpha(5) / outer alpha(4) / split ratio(7) / thickness(5) / outline(2, v4 only) |
| 14..15 | little endian screen height (`cl_crosshair_screen_height`) |
| 16..17 | zero |

Layout taken from the community decoder n0nz/cs2-crosshair-code and verified against
the user's own code plus seven published pro codes (all decode and re-encode
byte-exact, see `tests/OsuFPoSuCrosshairTest.cpp`). Old (v1) codes are detected by
their version byte and rejected with a dedicated message instead of being misread.

- Import: decodes into `fposu_crosshair_*`, stores the code's screen height in
  `fposu_crosshair_screen_height`, enables the crosshair and refreshes the sliders.
  Styles other than Static Cross are drawn as a static cross; T-style, recoil and the
  dynamic fields are ignored (reported in the notification).
- Export: always writes a v4 Static Cross code with CS2 defaults for the fields McOsu
  does not model, screen height = reference height (or the current screen height if
  the reference is 0), fills the textbox and copies to the clipboard.
- Preview: `OsuOptionsMenuFPoSuCrosshairPreviewElement` draws a dark panel and calls
  `OsuHUD::drawFPoSuCrosshair()` at its center, so it is pixel-identical to gameplay.

## Reference height scaling

CS2 rescales the pixel values by `screenHeight / cl_crosshair_screen_height`
(2 px at 720p becomes 4 px at 1440p). `OsuFPoSuCrosshair::scaleToScreen()` does the
same with round-to-nearest, thickness never below 1, then clamps to the maxima.
Reference height 0 keeps the previous 1:1 pixel behaviour.

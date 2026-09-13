# osu!lazer style "Random" mod for McOsu — design

Date: 2026-09-13
Repo: `McOsu` only (branch `practice-mods`). McEngine is untouched.

## Goal

A practice mod that lays a beatmap out differently every play while keeping what makes it
"the same map": every jump distance is preserved, only the jump angles are re-randomized,
sliders keep their exact shape (they are only moved, rotated and/or mirrored). This is a port
of osu!lazer's `OsuModRandom` (ppy/osu, MIT).

## Why a second Random and not a replacement

McOsu already has `osu_mod_random` ("Random" in the experimental list). It is a different
behaviour: it hashes each coordinate value into a fixed offset (circles move up/left by
0–64 px, every slider control point is moved independently by up to ~170 px, spinners are
moved), so jump distances change, slider shapes are deformed and identical coordinates get
identical offsets. It stays as is; the new mod lives next to it (user decision: coexist).

## User facing

- ConVar `osu_mod_random_lazer` (bool, default off). Checkbox **"Random (lazer)"** in the
  experimental mod list right below "Random". Reload beatmap to apply (same as the other
  load-time mods).
- ConVar `osu_mod_random_lazer_angle_sharpness` (float 1–10, default 7): lazer's
  "Angle sharpness" setting. Higher = sharper angles.
- Seed: reuses `osu_mod_random_seed` (0 = new seed every load) and the beatmap's
  `getRandomSeed()`; the checkbox tooltip shows `Seed = N` while enabled, like "Random".
- Recorded in local scores through `Osu::m_experimentalMods` (so "Use mods" on a score
  re-enables it), no score multiplier, not unranked (same as every experimental mod).
- Both Random mods may be enabled at once: lazer repositioning runs first, the legacy
  jitter is then applied on top inside the hitobject construction loops.

## Where it runs

`OsuDatabaseBeatmap::loadGameplay()` → `applyLazerRandom()` (new private static member),
called right before the `OsuCircle`/`OsuSlider`/`OsuSpinner` objects are built from the
primitive `HITCIRCLE`/`SLIDER`/`SPINNER` structs. This is the only place where slider
control points can still be changed (the `OsuSlider` constructor builds its curve from them).
Stacking (`calculateStacks()` in `onLoad()`) runs afterwards on the new layout, as in lazer.

`applyLazerRandom()`:
1. Converts circles, sliders and (unless `osu_mod_no_spinners`) spinners into
   `OsuLazerRandom::OBJECT`s: head position, combo index / new-combo flag (from the combo
   `number`), and for sliders the curve sampled relative to the head via
   `OsuSliderCurve::createCurve()` (same curve the slider will build later) plus whether it
   ends at its head (even span count).
2. Sorts by time and computes on-beat / on-downbeat flags with
   `getTimingInfoForTimeAndTimingPoints()` (uninherited timing point, lazer's
   `IsHitObjectOnBeat`; downbeats assume 4/4 because the timing points don't carry the meter).
3. Runs `OsuLazerRandom::reposition()`.
4. Writes back: circle `x/y` (rounded), slider control points through
   `applySliderTransform()` (mirror x → rotate → translate, relative to the original head),
   spinners untouched.

`OsuMainMenu` builds its logo text through `loadGameplay()` too and already force-disables
`osu_mod_random` around that call; the same is done for `osu_mod_random_lazer`.

## `OsuLazerRandom` (new `src/App/Osu/OsuLazerRandom.h/.cpp`)

Pure geometry in osu!pixel space, depends only on `Vector2` (no engine, no ConVars), which is
what makes it unit-testable on the host. Function names follow the lazer sources so the two
can be diffed side by side:

- `OsuModRandom.cs`: section/flow logic, `getRandomOffset`, `getRelativeTargetAngle`,
  `shouldStartNewSection`, `shouldApplyFlowChange`, random horizontal slider flips.
- `OsuHitObjectGenerationUtils.cs`: `rotateAwayFromEdge`, `rotateVectorTowardsVector`,
  `randomGaussian`, slider flip/rotate.
- `OsuHitObjectGenerationUtils.Reposition.cs`: `generatePositionInfos`,
  `repositionHitObjects`, `computeModifiedPosition`, `clampHitCircleToPlayfield`,
  `clampSliderToPlayfield` (falls back to 0°/180° when the rotated slider cannot fit),
  `applyDecreasingShift` (up to 10 preceding circles), `calculatePossibleMovementBounds`,
  `calculateCentreOfMass` (sampled every 50 px along the polyline).

Differences to lazer, all deliberate:
- RNG is `std::mt19937` + `uniform_real_distribution`, so a given seed does not produce the
  same layout as lazer with that seed (only determinism per seed matters here).
- Slider paths are the sampled polyline instead of `SliderPath.PositionAt()`; centre of mass
  and bounds are computed from those samples. Rotations/flips are accumulated into one rigid
  transform that is applied to the control points at the end (affine invariance of
  linear/bezier/catmull/perfect-circle curves keeps the shape exact).
- Downbeat detection assumes 4/4.

## Verification

- Host test (`scratchpad/test/test_random.cpp`, includes the .cpp for white-box access):
  helper unit checks (rotation helpers keep length, edge steering, bounds, centre of mass,
  angle formula, position infos incl. slider end / repeat handling), placement invariant
  (every object is placed exactly `distanceFromPrevious` from the previous object's end
  before clamping; clamped circles sit on the padded boundary), bounds (all circles inside
  the padded playfield, every slider that can fit is fully inside, oversize sliders keep the
  head inside), rigid slider transform, determinism per seed, seed sensitivity, sharpness
  extremes, empty/degenerate inputs.
- 32-bit Windows cross-compile of every touched file (WASAPI+ASIO configuration) and a full
  link of `McOsu.exe`. No Windows hardware here, so in-game behaviour is untested; manual
  checklist: enable "Random (lazer)" in the mod menu, play a map, check that jumps feel like
  the original spacing, sliders are intact, nothing is off-screen, seed tooltip shows, main
  menu logo sliders are unaffected, the legacy "Random" still behaves as before.

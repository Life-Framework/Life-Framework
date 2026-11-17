# Toys

### `LF_JumpPad` â€” `Feature/Toys/LF_JumpPad.c`
- âœ… A trigger sets the entering player's velocity upward once for that enter.
  A second callback before they leave applies the zero vector. Leaving clears
  the enter, so the next entry launches again. A disabled pad applies the zero
  vector and does not call `SetVelocity`. Ported from EveronLife
  `EL_JumpTrigger` (MIT, Copyright (c) 2022 EveronLife). (verified 2026-10-11)

### `LF_SpikeStrip` â€” moved to `Feature/Police/LF_SpikeStrip.c` (#646)
- The trigger version is gone; the police spike strip below replaced it.


### `LF_RangeTarget` â€” `Prefabs/Props/Range/LF_RangeTarget.et` (#1929)
- ðŸŸ¡ The practice range lane in GridWorld cell `shooting-range` (`Grid_shooting_range.layer`):
  three pop-up targets inside the yard. The target is the base-game
  `TargetInfantry_01` (`SCR_FiringRangeTarget`) with the lane's timings (folded 2 s,
  erected 3 s, distance 6 m). A hit folds it down with the base-game sound and
  animation; it pops back up by itself. No range manager is placed, so there is no
  scoring and no firing-line indicator. `LF_Test_RangeTargets` covers the prefab, its
  signal wiring and the fold and reset timing. Needs a playtest (shoot the targets,
  watch them fall and rise, a second player sees the same).

- ✅ The pad remembers a visitor only while it stands on it (#2406): `LF_JumpPadSeen` hands out a token per entity key and forgets it on exit, so the list no longer grows for the life of the server. A corpse or a downed body is not thrown (`LF_JumpPad.CanLaunch`: only `ECharacterLifeState.ALIVE`; logged `[LFDebug:JumpPad]`). Test `toys/jump-pad-seen` (LOGIC).

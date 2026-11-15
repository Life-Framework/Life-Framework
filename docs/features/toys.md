# Toys

### `LF_JumpPad` — `Feature/Toys/LF_JumpPad.c`
- ✅ A trigger sets the entering player's velocity upward once for that enter.
  A second callback before they leave applies the zero vector. Leaving clears
  the enter, so the next entry launches again. A disabled pad applies the zero
  vector and does not call `SetVelocity`. Ported from EveronLife
  `EL_JumpTrigger` (MIT, Copyright (c) 2022 EveronLife). (verified 2026-10-11)

### `LF_SpikeStrip` — moved to `Feature/Police/LF_SpikeStrip.c` (#646)
- The trigger version is gone; the police spike strip below replaced it.


# Atmosphere — Design Requirements

> Context file for music and background sound. Written against
> `docs/design.md` (in-world first). Full design, the survey of places and
> the credit cost: `docs/architecture/atmosphere.md`.

## Intent

The town is heard before it is read. Sound comes **from a thing in the world**:
a radio on a counter, a furnace, hens on a plot. No menu music, no 2D bed.

## Interaction pattern

Rung 1. A radio has one action, "Switch off" / "Switch on"
(`LF_AmbientSwitchAction`). Everything else just sounds when you are near.

## Pieces

- `LF_AmbientEmitterComponent`: project, radius, stop margin, priority, group,
  hours, condition. Local to machines that render.
- `LF_Atmosphere` + `LF_AtmosphereMix`: the 500 ms tick and the pure choice
  (range, allowed, one per group, voice cap).
- `LF_AmbientCondition`: `LF_AmbientConditionSwitch`,
  `LF_AmbientConditionChainWorking`. A feature adds its own subclass in its own
  folder; it may read only state that is replicated to clients.
- `LF_AmbientSwitchComponent`: the one replicated bool. `SetOn`, `Pulse`,
  `PulseOn(entity, seconds)` for a feature whose machine was just used.

## Rules

- The server never plays or decides a sound; it replicates state.
- A sound project comes from `tools\cli asset ambience`; its range lives in
  `tools/assets/atmosphere/ambience.json`, and the prefab radius sits inside it
  (`atmosphere-selftest`).
- Music is original and instrumental. A prompt never names an artist or a song.

## Iteration path

- **V2**: the remaining places (bank, hospital, payphones, fuel station, cash
  van), more radio tracks, persisted radio state.

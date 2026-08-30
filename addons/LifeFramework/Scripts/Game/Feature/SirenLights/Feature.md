# SirenLights — Design Requirements

> Context file for vehicle emergency lighting. Written against
> `docs/design-philosophy.md` (in-world first).

## Intent

Emergency lights are **operated in the cab**. The player reaches for the knob
on the dash and the siren/light modes change. Physical vehicle equipment, not a
menu toggle.

## Interaction pattern

Rung 1. The siren knob (`EL_SirenKnobComponent`) in the cab cycles light modes
(`EL_SirenModeAction` / `EL_LightAnimationComponent`); `EL_SirenManagerComponent`
coordinates; `Assets/Vehicles/EmergencyLightsKnob/` and
`Prefabs/Vehicles/SirenLights/` hold the assets.

## V1 (shippable)

1. Verify mode changes replicate correctly to other players (server-authoritative
   state, `BumpMe` on significant change only — AGENTS.md replication rules).
2. Ensure police-only gating (tie to `Police` on-duty / job) is server-checked.

## Iteration path

- **V2** — directional light control, sequence patterns, audio sync
  (`Sounds/Siren/`).

## Current state

- `EL_SirenManagerComponent`, `EL_SirenModeAction`, `EL_SirenKnobComponent`,
  `EL_LightAnimationComponent`, `EL_LightEntry` (`Feature/SirenLights/`).

## Dependencies

- `Police` (authorized users), `VehicleLock` (vehicle ownership), `Jobs`
  (emergency job gating).
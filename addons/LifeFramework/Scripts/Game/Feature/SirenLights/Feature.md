# SirenLights — Design Requirements

> Context file for vehicle emergency lighting. Written against
> `docs/design.md` (in-world first).

## Intent

Emergency lights are **operated in the cab**. The player reaches for the knob
on the dash and the siren/light modes change. Physical vehicle equipment, not a
menu toggle.

## Interaction pattern

Rung 1. The siren knob (`LF_SirenKnobComponent`) in the cab cycles light modes
(`LF_SirenModeAction` / `LF_LightAnimationComponent`); `LF_SirenManagerComponent`
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

- `LF_SirenManagerComponent`, `LF_SirenModeAction`, `LF_SirenKnobComponent`,
  `LF_LightAnimationComponent`, `LF_LightEntry` (`Feature/SirenLights/`).

## V1.1 - #263, service vehicles

`LF_SirenManagerComponent.ApplyRequest` is the one server rule (`LF_SirenRules.Judge`): out-of-range, repeated and
faster-than-300 ms requests are refused, so a toggle replicates once per change. Service vehicles: the police
jeep at the station and `LF_M997_Ambulance.et` (red `LightBarLED_Red.et`) at the hospital. Not done: an on-duty
gate on who may run the siren.

## Dependencies

- `Police` (authorized users), `VehicleLock` (vehicle ownership), `Jobs`
  (emergency job gating).
# Vehicles

## Intent

The car lot, the garage and the impound yard hand players cars, and a placed street car stands where it was put. A car that is not being driven stays where it was set down, even on a slope.

## Interaction pattern

In-world: the lot desk, the garage shell and the impound yard are the objects the player walks up to. Nothing here adds a key or a sheet; a driver lets go of the handbrake the vanilla way.

## Parked cars on slopes (#979)

- **Player sees:** a car left on a slope stays put until someone drives it.
- **Owner and side:** the engine. Every car prefab we spawn (the lot's S105, UAZ469, S1203, UAZ452, M923, the police UAZ469 and the M997 ambulance) comes up with the vanilla persistent handbrake on, and vanilla pulls it when a driver leaves (1.7). No LF code sets it. Checked 2026-11-14 on 1.9.0.37: a car on a 19 degree ramp, forced awake (the engine normally lets a still car sleep), crept a steady 0.18 m per 30 s with the brake on (0.35 m in 60 s, equal halves: slip, not settling); the same car with the simulation's brake input released rolled 30 m. The handbrake slows an awake car on a slope to a crawl, it does not stop it dead.
- **Gotcha:** `CarControllerComponent.SetPersistentHandBrake(false)` on an empty car does not release the simulation's brake input; only a pilot's controller feeds it. Do not read `GetPersistentHandBrake` as proof an empty car is held.
- **Data:** none.
- **Proof:** `vehicles/park-brake-world` (WORLD) guards the prefab default for every vehicle prefab we spawn; `node agent-box/session.mjs park-brake-proof` (`up --profile full`) is the slope measurement.

## Saved vehicle state (#752a)

Each owned vehicle record carries a tow state (`LF_EVehicleTowState`), towed-at and inspected-at stamps (unix seconds, 0 = never) and a hidden compartment size (0-3, `LF_VehicleOwnership.COMPARTMENT_MAX`). They are `[NonSerialized()]` on `LF_VehicleRecord` and saved in the `vehiclestate` payload (save version 33), so the `vehicles` record layout never moves. `SetState` and `StateOf` are the server seam; no player action writes them yet (#774 towing, #778/#779 inspection and compartments, #859 the cache). The restart impound is still derived and stays `NONE`. Spec: `docs/architecture/record-shape-vehicle.md`.

## Car colours (epic #1826, card cc-1)

- **Player sees:** nothing yet. The lot sheet still sells one colour per model; cc-3 adds the colour page.
- **Owner and side:** the server. `LF_VehicleColoursConfig` (`Configs/Vehicles/LF_VehicleColours.conf`, empty in core, filled by a pack overlay, rows keyed by catalog prefab) lists each model's colours: name key, swatch, locked child, price delta. A colour is a thin child of our locked car prefab, so the colour of a bought car IS its `m_sSpawnPrefab`: no new save data (version stays 34), no RplProp, and late joiners load the same prefab.
- **Buy path:** `LF_AskBuyVehicle(lot, index, colour)` carries the colour; `LF_VehicleLotComponent.BuyColour` refuses a colour the model does not sell (negative, at or past the count, any colour for a model with no row) with `LF_Debug.Error` and charges nothing. The price is the model's price plus the colour's delta, before the cash-ceiling rule. `Buy(...)` is `BuyColour(..., 0, ...)`.
- **Others:** the chop shop resolves a colour child to its base colour first (`LF_ChopShopRules.PayoutForPrefab`), so it pays as its model. Impound already falls back from the spawn prefab to the catalog prefab. A respawn whose saved colour prefab no longer loads (a pack dropped it) comes back in the model's base colour with a warning (`LF_VehicleOwnership.RespawnNameFor`).
- **Data:** `LF_VehicleColours.conf` empty in core; pack rows in cc-2 and cc-4. Colour names are neutral core strings.
- **Proof:** `vehicles/colours` (LOGIC), `vehicles/colours-overlay` (WORLD, a pack overlay add, patch and remove).

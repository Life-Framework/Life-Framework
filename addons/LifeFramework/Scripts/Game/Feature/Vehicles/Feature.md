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

## DLC vehicles (#3304, harvester spike #3282)

- **Player sees:** the Rally tractor (`Z7211`) can be sold at the lot. A player without the Rally DLC who presses Buy is told *This tractor needs the Rally DLC.* and is not charged; an owner gets the locked child like any lot car. Vanilla refuses a non-owner the driver seat, so the lot never sells a tractor nobody could drive.
- **Rule:** `LF_VehicleDlcGate`. DlcManager answers only for the machine it runs on (vanilla checks DLC on the acting client), so the buyer's client asks its own licence (`LocalOwns`, in `LF_AskBuyVehicle`) and sends the answer with the buy; the server (`LF_VehicleLot.Buy`) refuses a `/Wheeled/Z7211/` prefab before `BuyColour` charges anything when the answer is no. The answer is a hint: a lying client buys a tractor whose driver seat vanilla still refuses it. A base-game vehicle is never asked.
- **Data:** the price row (`LF_VehiclePrices.conf`, $3,000) and the locked child `EL_Z7211_red_CIV.et`. The offer row on a lot layer is a town/pack edit and is not part of the gate.
- **Proof:** `vehicles/dlc-gate` (LOGIC). A non-owner client on a dedicated server is a manual check.

## Owned cars stay out of the engine save (#3614)

- **Player sees:** after a restart, a car left on the street is at the impound only. Collecting it gives back one car with its cargo. Before the fix, an unowned copy was also left at the parking spot with whatever the trunk held.
- **Owner and side:** the server. The engine's `PersistenceSystem` tracked every vehicle entity and restored it where it stood, with a new lock identifier and no owner, while the restart impound brought the record back too. `LF_VehicleWorldSave.Hold` calls `StopTracking(vehicle, true)` on every car `LF_VehicleOwnership` spawns (the lot purchase and `SpawnOwned`, used by collect and retrieve). The car's record and its cargo snapshot are its only save. This is the same pattern as `LF_StoreShelfSave` (#3127).
- **Old saves:** a save written before the fix still holds the copies. The first boot on it restores them once as unowned, keyless cars, and they are not dropped later. Wipe the save, or delete those cars, when this ships.
- **Proof:** `vehicles/restart-one-car` (PERSISTENCE): collected and retrieved cars are not tracked. Under `tools\cli wt e2e` the reload boot finds no copy and no loose apple by the impound, then exactly one truck after the collect, holding its apple.

## Inspection stickers (#3433)

- **Player sees:** an officer at an inspection desk picks "Inspect vehicle" and the car beside the desk gets a sticker valid for 7 game days; "Check the sticker" says valid, expired or none.
- **Owner and side:** the server (`LF_Inspection`: expiry game day per lock identifier). The client only hides the actions from anyone who is not on-duty police; the server re-checks the officer.
- **Data:** none saved yet. The sticker is session only; the save card (Q10) maps it onto the `m_iInspectedAt` stamp of the vehicle record.
- **Proof:** `vehicles/inspection` (LOGIC). The desk prefab is not placed in any world; a car-side action needs a hook on the car prefabs.

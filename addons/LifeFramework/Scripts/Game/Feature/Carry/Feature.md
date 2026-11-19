# Carry

## Intent

A heavy load the player walks up to, hoists onto the shoulder, hauls slowly and sets down. One generic
component serves a log now and an ore sack or a crate later (#3265 loads logs onto a truck bed next).

## Interaction pattern

In-world: the player looks at a load on the ground and presses F (*Hoist onto shoulder*). No sheet.
The load goes through the hand-carry storage (`Feature/Character/Inventory/HandCarry/`), so the weapon is
holstered, the hands are busy and a gadget in the hands replicates natively, late joiners included.

## Shoulder carry (#3263)

- **Player sees:** the load on the shoulder (the `player_carry` hand pose of the hand-carry item), a
  walking pace and no sprint, and the load on the ground again after a drop.
- **Owner and side:** `LF_ShoulderCarry` (rules: `Decide`, `Hoist`, `Drop`, `Carried`) runs on the machine
  that owns the carrier. `LF_ShoulderCarryComponent` on the load watches its own slot change, which fires on
  every machine, so the speed cap needs no RPC.
- **Data:** the prefab sets the profile on the component (`m_fSpeedCap`: softwood log 0.7, hardwood log 0.5,
  ore sack 0.75, crate 0.6; `m_sKind`). `ShoulderCarryItem_Base.et` is the base prefab; a log, sack or crate
  inherits it. Nothing persists: a load in the hands is an ordinary inventory item.
- **Refusals** (each logs `[LFDebug:Carry]`): no profile, carrier down, a load already carried, the item
  already in a slot, out of reach, no hand-carry storage.
- **Proof:** `carry/shoulder-carry` (WORLD), `node agent-box/session.mjs shoulder-carry`.

## Truck cargo bed (#3265)

- **Player sees:** a hauler walks to the tailgate carrying a log and presses F (*Load onto the truck*): the log leaves
  the shoulder and joins a stack of logs on the bed, drawn on every machine, a late joiner included. F again with free
  hands (*Unload onto your shoulder*) takes the top log back. A full bed or a car says so in a toast.
- **Owner and side:** the server owns the stack. `LF_CargoBedComponent` on the vehicle holds one kind index per unit
  (softwood log 1, hardwood log 2; ore sack 1 and crate 2 have rows but no prefab yet) and replicates it as one short
  string through `RpcDo_SetStack` and `RplSave`/`RplLoad`. Every client draws physics-free local copies on bed slots
  (the `LF_FarmPlotComponent` pattern). Rules: `LF_CargoBed` (`DecideLoad`, `Pack`/`Unpack`). Nothing persists (#3266).
- **Data:** capacity and layout are attributes on the vehicle `.et` (1989 truck `EL_M923A1_transport` 12, vans
  `EL_UAZ452_base` and `EL_S1203_transport_beige` 6, a car none). The tailgate is a `lf_cargo` action context.
  Bed positions on the vans are estimates; the truck's come from its measured bounds.
- **Hauling gate:** both log prefabs now inherit `ShoulderCarryItem_Base`, so F hoists (no vanilla pick-up), and
  `LF_CargoHaulingGate` refuses the inventory screen's `InsertItem` of a log into anything but the hand-carry slot.
  Scripted inserts (`AddAmount`, used by the sawmill chain and its tests) are unchanged until a carried-log sawmill card.
- **Proof:** `carry/cargo-bed` (WORLD), `node agent-box/session.mjs cargo-bed`.

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

# Carry

### `LF_ShoulderCarry` — `Feature/Carry/LF_ShoulderCarry.c` (#3263, 2026-11-19)
- ✅ Hoist a load with `LF_ShoulderCarryComponent` (`ShoulderCarryItem_Base.et`) onto the shoulder through the hand-carry slot (F on the ground load); the weapon is holstered and the hands are busy. The carrier's dynamic speed is capped by the load's profile (softwood log 0.7, hardwood 0.5, ore sack 0.75, crate 0.6), so no sprint. A second hoist is refused; drop sets it down and lifts the cap; a downed carrier drops it. Tests: `carry/shoulder-carry` (WORLD); session proof `shoulder-carry`.
- ⏳ No log, sack or crate prefab uses the base yet (#3265 builds the truck-bed load on it); the two-player and late-joiner pictures need a placed load in the debug grid.

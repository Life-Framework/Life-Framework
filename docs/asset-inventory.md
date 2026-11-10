# Asset inventory

Every visible thing the mod adds, and whether its look is final or a stand-in.
Placeholder rows are the to-model list for the 3D asset pipeline (#339, Development
track).

## How to keep this up to date

- **The area polish pass** (`docs/agent-area-polish.md`) adds or updates one row per
  mod prefab it checks.
- **Any PR that adds a visible prefab** adds its row in the same change.
- **When a model replaces a placeholder,** set the row to `final` and name the new asset.

## Status

- `final`: a real base-game or mod asset that is the thing.
- `placeholder`: a stand-in mesh that sort of works (for example, a radio as a pager).
  Needs a model.
- `missing`: no mesh or an invisible fallback. Needs a model before it can ship.

| Item | Prefab | Status | Uses now | What it should be | Area / issue | Priority |
|---|---|---|---|---|---|---|
| Pager | `LF_Pager.et` | placeholder | ANPRC68 radio mesh | 1980s belt pager with a small screen | Comms #249 | medium |
| Moonshine still | `LF_ChainStill.et` | placeholder | barrel with the boiler mesh (renders plain white) | copper pot still with coil | Moonshine #420 | high: in website shots |
| Fence drill | `LF_Drill.et` | placeholder | `BenchDrill_01` model | handheld power drill or safe-cracking drill | Fence #271 | medium |
| Fence keycard | `LF_Keycard.et` | placeholder | stand-in mesh (#271) | plastic access card | Fence #271 | low |
| Fence charge | `LF_Charge.et` | placeholder | stand-in mesh (#271) | improvised breaching charge | Fence #271 | low |
| Police scanner | `LF_Scanner.et` | check | (#253) | handheld 1980s scanner | Comms #253 | medium |

The polish pass fills in the rest, area by area. The `check` rows haven't been
looked at yet.

## Area polish checks

| Item | Prefab | Status | Uses now | What it should be | Area / issue | Priority |
|---|---|---|---|---|---|---|
| Payphone | `LF_Payphone.et` | final | vanilla `PhoneBooth_E_01` | (already a phone booth) | Square, fuel, hospital, police: Comms #249 | none |
| Town signs (town map, safe zone) | `LF_Sign_Posts.et`, `LF_Sign_PolePlate.et` | final | vanilla sign bodies with a rendered face | (already signs) | Spawn square | none |
| Clerk desk and chair | `Desk_01_short.et`, `Chair_02_green.et` | final | vanilla furniture | (already a desk and chair) | Spawn square | none |

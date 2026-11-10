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
| Cash van box | `LF_CashVanBox.et` | placeholder | vanilla `Chest_01` (dark steel trunk) at 1.4 scale, on the van's rear step | a locked steel cash-in-transit box | Cash van #274 | low |
| ATM (two, on the bank front) | `ATM_Debug`, `ATM_Debug_2` (DebugWorld `ATM.layer`) | placeholder | vanilla post-office teller counter `CounterPostOffice_01_window` against the bank wall | a 1989 wall-mounted cash machine or a teller point with a glass screen (none in the base game) | Bank, ATM | low |
| Money bag | `LF_MoneyBag.et` | placeholder | vanilla `SackUniversal_01` sack mesh at 0.55 scale | a canvas cash bag with a drawstring | Bank, cash van #273 #274 | low |
| Hospital bed | `LF_HospitalBed.et` | final | vanilla `BedMedical_01` | (already a medical bed) | Hospital, area 7 | none |
| Ambulance | `LF_M997_Ambulance.et` | final | vanilla `M997_maxi_ambulance` | (already an ambulance) | Hospital, area 7 | none |
| Hospital till | `Roadmap_Hospital_Till` | final | vanilla `CounterPostOffice_01` | (a reception counter) | Hospital, area 7 | none |
| Medicine cabinet | `Roadmap_Hospital_Cabinet` | placeholder | vanilla `CupboardOld_01_medium` wooden cupboard | white medical cabinet | Hospital, area 7 | low |
| Stretcher | none | missing | no base-game stretcher found | stretcher by the ambulance | Hospital, area 7 | low |

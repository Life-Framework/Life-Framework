# Trailer shot checklist (#571)

The shots in [trailer-571.json](../../../video/trailer-571.json), grouped by location in the
order to shoot them in one session. Times and lines are in
[script.md](script.md).

## Before the shoot

The trailer is shot last (maintainer, 2026-11-10). Start only when every box
here is ticked.

**Issues that must be closed (or the shot swapped for its fallback):**

| Issue | What it fixes | Shots |
| --- | --- | --- |
| #679 | Visual QA sweep of the blind layout PRs | S04, S11, S12, S14, S21, S22 |
| #607 (PR #629) | Spawn hub table and travel globe | S05, S22, S23, R2 |
| #590 (PR #606) | Bank teller at the counter | S14 |
| #582 | Pay at the store's cash register | S07, S13, R1 |
| #592, #593, #618 | Mining rocks and nuggets, smelter boiler, quarry layout | S11, S12 |
| #614 | Farm seed-to-harvest loop (the beds may move) | S09, R4 |
| #626 | Dispatch area and test call | S03, S16 |
| #495 | Stolen-car row on the wanted board | S15 |
| #512, #625 | Revive animation, medic clock-in on the device | S17 |
| #513 | Drill working-loop animation | S01, S02, S19 |
| #601, #415 | Chop shop refusals and the strip loop | S20 |
| #616 | Convoy drop near the barn | S21 |

**#679 cross-check.** Of the areas #679 lists, the trailer uses the teller
(#606), the police light bar (#599), the smelter and rocks (#598, #596), the
spawn hub (#629), the convoy drop (#661) and the quarry (#618). It does not
use the fenced crime site in the test annex (#678), the key models (#677) or
the parcel (#619), so those do not block it.

**Tooling that must exist:**

- [ ] #392 camera rig: `static`, `drone` (keyed path), orbit with an azimuth,
  and a clean first-person frame (HUD and hints off on the player view).
- [ ] #394 env-only scenes, one per location (`trailer-*` in the storyboard),
  each 17.9 local, Clear, day frozen.
- [ ] #393 a clothed puppet outfit (only S21 needs a puppet).
- [ ] A mining flow or harness swing (S11) and a convoy flow (S21).
- [ ] #396 audio: session started with `--sound`, one slate per take.
- [ ] `quality-gate.md` (post-production, #397) read before the first take.

**Session.** This PC (the laptop waits on #482 for audio). Three clients:
client 1 the hero (a fresh character, so S06 sees form R-89), client 2 on the
police roster, client 3 holds the camera.

```powershell
node agent-box/session.mjs roster 2
node agent-box/session.mjs up --clients 3 --mission playtest --profile full --res 1920x1080 --sound --wait --priority low
```

Record at 30 fps. Shoot each shot as a separate take of its duration plus 2 s
of handle on each side.

## Look checks for every shot

- Golden hour: long warm light, shadows soft, no sky clipping to white.
- Clear sky. No goopy clouds (#602).
- Nothing floating, sunk or clipping: props sit on the ground, NPCs stand
  outside walls and counters, vehicles sit on their wheels.
- No HUD, debug text, hint toasts or harness overlays unless the shot is a UI
  on purpose (form, sign card, board, terminal, travel list).
- No other player or puppet in frame that the shot does not call for. No
  underwear characters.
- Subject in the centre third (the 9:16 crop takes it from there).
- Camera never passes through a wall or roof.

## 1. Spawn square (126, 246)

Shoot S06 first, before client 1 has done anything else.

| Shot | Take | Look check |
| --- | --- | --- |
| S06 | Client 1 `flow arrival-chain`, first person, form and ID card | Form text is sharp at 1080p; the clerk faces the camera, not into his desk |
| S08 | Client 1 reads the Town Board | Card title and rule visible; no toast over the card |
| S22 | Client 1 at the spawn table (119, 243.5), travel list, pick Farm | Globe on the table, not floating; list readable; the landing at the farm is in the open |
| S05 | Drone up from (110, 3, 252) to (118, 14, 250), 5 s | Welcome sign and payphone read; the street runs east into warm light; the table (#629) is tidy |
| S23 | Orbit, centre (125, 0, 250), azimuth 270, radius 16, 7 m up, 12 s. Clients 1 and 2 on the square | Two clothed players, both doing something; nobody stands inside a bench or cart |
| R2 | Still: `tools\cli shots area spawn-square` | Same as S05 |

## 2. General store (180, 244)

| Shot | Take | Look check |
| --- | --- | --- |
| S07 | `camera orbit 180 0 244 8 4`, no actors | Sign, window, counter and market table in the arc; camera stays on the street side |
| S13 | Client 1 store-proof, first person at the till | Hand and cash visible; the register is the one #582 uses |
| R1 | Still: `tools\cli shots area general-store` | Counter dressed, no clipping crates |

## 3. Bank and cash van (182, 255; van at 196, 252.4)

| Shot | Take | Look check |
| --- | --- | --- |
| S14 | Client 1 bank-teller-proof, first person | Edith behind the west counter, not in the wall or counter; sign reads "Teller" |
| S01 | Static beside the van rear, 0.6 m up; client 1 drills the box (bank-kit-proof, then the drill) | The bend animation plays (#513); the box sits on the rear step, not floating |
| S02 | Static at ground level behind the van; sacks drop | Three sacks land and rest on the road, none inside the van |
| S19 | Static 4 m ESE of the vault door (194, 265), client 1 drills the door | Door flush with the east wall; drill bend visible; the door swing reads |

Reset the vault and van between takes (`scene reset`, then the proof again).

## 4. Fuel station and police (266, 240; 233, 239)

| Shot | Take | Look check |
| --- | --- | --- |
| S03 | Client 2 at the fuel payphone (256.5, 237), coin, call menu | Booth door to the street, back to the kiosk; menu readable |
| S04 | `camera orbit 243.5 0 246 7 4`; client 2 on duty in the jeep, siren on | Light bar touches the roof (#599); lights flashing in the take |
| S15 | Client 1 robs the fuel till; client 2 clocks in, reads the wanted board | Board stands on the lawn facing the street; only the till robbery line shows |
| S16 | Client 1 dispatch-proof from the police payphone; client 2 reads the dispatch board | The new line is visible on the board |

## 5. Hospital (262, 255)

| Shot | Take | Look check |
| --- | --- | --- |
| S17 | Client 1 revive-anim-proof on client 2; client 3 orbits from the road, azimuth 180, radius 5, 1.8 m up | CPR animation plays the full 4 s; the patient gets up; nobody clips the duty desk or benches |
| S18 | Still: `tools\cli shots area hospital`, ambulance bay | Ambulance between its four cones, sign upright |

## 6. Farm and orchard (150, 300; tree 130, 310; press 141, 311)

| Shot | Take | Look check |
| --- | --- | --- |
| S09 | Client 1 gathers cabbage, first person | Beds after #614; plants sit in the soil |
| S10 | Client 1 `flow chain-apple`; film the gather and the press | No inventory screen (#612); juicer on its press table |
| R4 | Still: `tools\cli shots area farm` | Matches the gallery's evening light |

## 7. Quarry mining yard (176, 167; smelter 169.8, 171.5)

Scene hour 16.5: the pit walls shade the floor at 17.9. Check with a still.

| Shot | Take | Look check |
| --- | --- | --- |
| S11 | Client 1 swings a pickaxe at an ore rock | Rock shakes and shrinks a step; nuggets rest on the ground, not in it |
| S12 | Client 1 smelts two nuggets at the boiler | The base-game boiler (#598), not the old wood chipper; bar lands 1.6 m in front, on the ground |

## 8. Chop shop and hideout (322, 300; 305, 295)

| Shot | Take | Look check |
| --- | --- | --- |
| S20 | Client 1 chop-proof; client 2 orbits in over the south gate, azimuth 180, radius 9, 4 m up, hour 18.3 | Crane, stripped jeep shell, tyres in frame; camera clears the yard walls |
| R3 | Still: `tools\cli shots area hideout` | Bench under the pergola, low sun |

## 9. Convoy barn (466, 331; van 474, 324.5)

| Shot | Take | Look check |
| --- | --- | --- |
| S21 | Client 1 (org member) Start convoy, carries the crate; puppet drives the van south; `camera follow 474 0 324.5 9 3` after a 2 s wait | Driver clothed; van on its wheels down the lane; the follow does not cut into the barn |

## 10. Spawn square at dusk (last)

| Shot | Take | Look check |
| --- | --- | --- |
| S28 | Static from (132, 2.2, 250) looking east down the main street, scene hour 19.2 | Dusk, not night: the street still reads; lamps on if they have them |

## After the shoot

- Run every take through `quality-gate.md` (#397).
- The maintainer signs off the cut before it is posted (#571).

# Agent playtest round 2026-11-27 (night drive): the island by road at 22:00, headlights on

**Commit played:** `ws/night-drive-1127` on `origin/main` `df0a04a2`. Feature freeze for 0.3.0 (#4836, cut Sat 2026-11-28): fixes only.
**Setup:** one full-profile island session on lf-testbox (`up --mission island --profile full --hold 20`, nobody queued, closed straight after), one client. The new proof `node agent-box/session.mjs drive-night-tour` (`agent-box/proofs/drive-night-tour.mjs`) bought a T601 at the Saint-Philippe lot, pinned the clock to 22:00 (the island's day is 86,400 s, so it does not drift), turned the headlights on (`lights=1`, the green headlight icon is in every seat picture) and drove by road (`drive road`, 55 km/h cap). At each arrival it took one picture from the driver's seat, one standing beside the car and one standing at each counter, sign or NPC of the site, plus one from the seat every 75 s on the way. Afterwards eight eye-height world shots (`capture world`) were taken at the turn-offs and gates the drives did not reach.
**The fuel limit:** a bought car has 0.75 of a tank and burns about 0.03 a km. Legs 1 to 4 used 0.64 of it (0.75 to 0.11); leg 5 from the outpost started with 0.11 and was stopped after 747 m. Legs 5 and 6 were then driven from the town square area with a second bought car, so "outpost to the truck yard" (6,329 m planned) was **planned but not driven**.

## Result per leg

| # | Leg | Planned | Driven | Game time | Result | Fuel after |
|---|---|---|---|---|---|---|
| 1 | Saint-Philippe to the east compound (smelter gate, road end 6442, 6440) | 6,233 m (4,644 m straight) | 6,266 m, max 55.8 km/h | 548 s | **Arrived**, 9.1 m from the goal, 0 back-ups, 1 re-plan | 0.55 |
| 2 | East compound to East Rise (6670, 7909) | 2,260 m | 2,239 m | 216 s | **Arrived**, 7.7 m off, 1 back-up | 0.48 |
| 3 | East Rise to Highstone (4959, 8460) | 3,368 m | 3,454 m | 385 s | **Stuck 275 m short** at (5209, 8345), 6 back-ups, 5 re-plans: `stalled_blocked_by_t_carpinus_betulus_3f_at_0.4m_middle` | 0.32 |
| 4 | Highstone to the north outpost gate (7660.5, 8384) | 3,780 m | 3,749 m | 313 s | **Stuck 230 m short** at (7453, 8286), 6 back-ups: `stalled_clear_pitch_-8` (a slope too steep for the planner's straight tail) | 0.11 |
| 5 | North outpost to the NE truck yard (5024, 10848) | 6,329 m | 747 m, **stopped by hand** (fuel 0.08) | 62 s | Not driven; plan only | 0.08 |
| 5b | Square area to the NE truck yard gate, fresh car | 949 m (667 m straight) | 1,216 m | 182 s | **Stuck 28 m short** at (4996, 10853), nose on the yard's tin wall, 6 back-ups: `stalled_blocked_by_TinWall_01_6m_A_at_0.2m_middle` | 0.67 |
| 6 | NE truck yard back to the square (4437, 10670) | 882 m | 865 m | 88 s | **Arrived**, 9.1 m off, 1 back-up | 0.64 |

Totals: 3 legs arrived (1, 2, 6), 3 stopped short of the goal by terrain, a tree or a wall (3, 4, 5b), 1 not driven (5). The drive harness plans every route now (the 11-27 driving round could not start any of these); the stops are the last off-road metres, not the long road drives.

## Can a player find it at night?

Judged from the pictures below. "Findable" means a player who knows roughly where to go sees a light, the gate or the counter from the road or the car.

| Arrival | Road and turn-off | Gate | Counter / NPC | Sign | Verdict |
|---|---|---|---|---|---|
| East compound smelter | The headlights show the tarmac, the centre line and the reflector posts well all the way ([road](../../../website/public/docs-images/night-drive-1127/road_east_headlights.jpg)). The planner chooses the turn, so no picture shows a junction. | Road end is at the tin wall: solid dark fence; one narrow gap with a warm glow behind it, a street lamp and lit silo above ([from the seat](../../../website/public/docs-images/night-drive-1127/east_compound_arrive_seat.jpg), [from the road](../../../website/public/docs-images/night-drive-1127/east_compound_gate_from_road.jpg)). The gap itself is dark. | The furnace, flue and the stool lantern are lit when standing at the furnace ([furnace](../../../website/public/docs-images/night-drive-1127/east_compound_smelter_stand.jpg)). | The IRON SMELTER plate reads in that picture (standing at the furnace). From the road it is behind the wall and cannot be read. | **Findable**: the silo and lamps above the wall are a beacon. The gate gap is the weak spot. |
| East Rise seam | No road ends at the seam; the car stopped in the meadow. | No gate. | The seam's lamps glow in the meadow, fence and timber lit across the meadow ([from the seat](../../../website/public/docs-images/night-drive-1127/east_rise_arrive_seat.jpg)). | Not read: the standing shot at the sign faced away into grass. | **Findable** by its glow. |
| Highstone mine | **Cannot be seen.** The planner's road ends at (5209, 8345) in forest, 275 m from the sign and 64 m lower. No mark, lamp or sign at any turn-off. | None. | Nothing lit within 90 m. | The plate was not seen from the track. | **Pitch black** ([car](../../../website/public/docs-images/night-drive-1127/highstone_arrive_seat_black.jpg), [90 m out](../../../website/public/docs-images/night-drive-1127/highstone_track_90m_black.jpg)). The four stool lamps (#5013) light only the clearing. Filed as #5089. |
| North outpost | The car stopped 230 m out on a dark hill; nothing of the camp shows from there ([seat](../../../website/public/docs-images/night-drive-1127/outpost_arrive_seat_black.jpg)); a single red point glows far off in the standing shot. | The gate lamps and sandbag posts light the entrance; the camp reads as a lit clearing from 28 m through the trees and clearly from 15 m ([gate](../../../website/public/docs-images/night-drive-1127/outpost_gate_lit.jpg)). | The quartermaster is lit under the tent and offers Talk ([quartermaster](../../../website/public/docs-images/night-drive-1127/outpost_quartermaster_talk.jpg)). | Not judged. | **Findable only at the gate**; pitch black on the approach (a 230 m hill with no light). |
| NE truck yard | A lit road past the silos; the yard's floodlights and the orange lamp show above the tin wall from the road ([approach](../../../website/public/docs-images/night-drive-1127/truck_yard_approach_seat.jpg)). | A gap in the tin wall with a glow behind it ([from the road end](../../../website/public/docs-images/night-drive-1127/truck_yard_gate_from_road.jpg)); the closed gate and the yard are lit from the pavement ([gate](../../../website/public/docs-images/night-drive-1127/truck_yard_gate_stand.jpg)). | The examiner's action is offered at the examiner. | The gate plate was not in any frame. | **Findable.** |
| Saint-Philippe square | The lot and the main street are lit: lamps, window glows ([square](../../../website/public/docs-images/night-drive-1127/square_arrive_seat.jpg)). | n/a | Lit fronts and lamps around the square. | Not judged. | **Findable.** |

**Overall:** at 22:00 with headlights a player can drive the roads and find four of the six arrivals (east compound, East Rise, NE truck yard, the square) by a light, a glow or a lit gap, and the north outpost once at its gate. Not found: Highstone (pitch black from the track, its turn-off unmarked) and the last 230 m to the north outpost. No arrival was pitch black at the site itself except Highstone. No road turn-off is marked anywhere (no lamp, sign or post), so none can be called visible; the headlights show the road ahead but not a side track. Sign plates are readable only from a few metres inside a site, never from the road.

## Fixes

| What | State |
|---|---|
| #5089 Highstone mine invisible from its track at 22:00 | Filed with coordinates (the r05 / r07 bend at (5017.6, 8498), 55 m from the sign). **No lamp PR shipped in this round**: placing a beacon needs the real approach geometry, a `town fit` refit of the `iron-face` kit and a Workbench `look`; guessing it would add an unchecked prop to a town layer during the freeze. Not an RC blocker (the map waypoint leads to the mine) |

Not filed (tool limits, not the game): legs 3 and 4 stop where the AI road graph ends and the planner's straight tail meets a tree or a slope; the NE yard drive ends nose to the tin wall 28 m before the gate sign (the gate opening is not on the straight line from the road end).

## Tooling notes

1. **Fuel.** 22 km is the most one bought car does. The proof says so; run the last legs with `--buy --legs 5,6`.
2. **`stand <entity>` does not face the entity.** The standing pictures at the East Rise and Highstone signs looked into grass and leaf litter. Use `capture world` with a camera spot and a target, as `agent-box/east-night-shots.mjs` does, for a sign.
3. **Approach pictures.** The first run's "30 to 110 m before the end" window was missed (a capture takes up to 40 s); it is now 25 to 250 m. Captures are BMP files; the proof keeps that extension, and they were converted to JPG for this page.
4. A session capture folder is cleared by `session down`; every picture was copied to `tmp/night-drive/` first.

## Pictures

1. [Smelter, standing at the furnace: plate, furnace and stool lamp lit](../../../website/public/docs-images/night-drive-1127/east_compound_smelter_stand.jpg)
2. [North outpost gate from 15 m: lamps, sandbags and the camp lit](../../../website/public/docs-images/night-drive-1127/outpost_gate_lit.jpg)
3. [NE truck yard approach: floodlights and lamp above the wall](../../../website/public/docs-images/night-drive-1127/truck_yard_approach_seat.jpg)
4. [Highstone from the track, 90 m from the sign: nothing visible](../../../website/public/docs-images/night-drive-1127/highstone_track_90m_black.jpg)

## Manual testing

- At 22:00 drive a car with the headlights on from Saint-Philippe to Highstone iron mine (#5089): can you tell where to turn off the road and find the plate?
- At 22:00 walk the last 230 m up to the north outpost gate from the road at (7453, 8286): is there any light to steer by?

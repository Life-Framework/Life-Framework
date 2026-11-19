// Proves the parked-car slot planner (`town parked-cars`, #3532): a car spot clears buildings, doors, road carriageways, props and
// other markers, heads along the road, and the committed Saint-Philippe plan is deterministic and refuses nothing.
// inputs: tools/town/parked-cars.mjs, docs/everon/places/saint-philippe/mapping.json, docs/everon/places/saint-philippe/dossier.json, docs/everon/places/saint-philippe-dock/dossier.json
// red-proof: 2026-11-19, with the road test in refuse() removed the "on the carriageway" case below printed FAIL (a car 1 m off a road centre line was accepted); restored, it passes.
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { carBox, distToLine, distToPoly, nearestRoad, plan, planCar, refuse } from "../town/parked-cars.mjs";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const fails = [];
const check = (ok, msg) => { if (!ok) fails.push(msg); };

// A road along +z at x = 0, a 10 x 10 house east of it, a door on the house's west wall.
const road = { id: "r1", points: [[0, -50], [0, 50]] };
const house = { id: "b1", footprint: { corners: [[12, -5], [22, -5], [22, 5], [12, 5]] }, entrances: [{ x: 12, z: 0 }] };
const world = { buildings: [{ id: "b1", poly: house.footprint.corners }], doors: [[12, 0]], roads: [road], props: [], layers: [], taxi: [], placed: [] };

check(Math.abs(distToLine(3, 7, road.points) - 3) < 1e-9, "distToLine measures to the segment");
check(distToPoly(15, 0, house.footprint.corners) === 0 && Math.abs(distToPoly(10, 0, house.footprint.corners) - 2) < 1e-9, "distToPoly is 0 inside and the edge distance outside");
check(Math.abs(nearestRoad(5, 0, [road]).yaw) < 1e-6, "a road along +z heads 0 degrees");
check(carBox(0, 0, 0).length === 4 && Math.abs(Math.max(...carBox(0, 0, 0).map((p) => p[1])) - 2.3) < 1e-9, "the box is 4.6 m long along the heading");

check(refuse(1, 0, 0, world) === "on the carriageway of r1", `a car 1 m off the road centre line is on the carriageway, got ${refuse(1, 0, 0, world)}`);
check(refuse(6, 0, 0, world) === null, `a car 6 m off the road, a long way from house and door, is fine, got ${refuse(6, 0, 0, world)}`);
check(refuse(8, 0, 0, world) === "blocks a door", `a car within 3.5 m of the door blocks it, got ${refuse(8, 0, 0, world)}`);
check(refuse(11, 20, 0, { ...world, props: [[11, 20]] }) === "overlaps a prop", "a car on a prop is refused");
check(refuse(6, 30, 0, { ...world, placed: [[6, 25]] }) === "next to another marker", "two markers closer than 6 m are refused");
check(refuse(6, 30, 0, { ...world, taxi: [[6, 36]] }) === "near the taxi rank", "a marker within 9 m of the taxi rank is refused");

const found = planCar(house, world);
check(found.spot && refuse(found.spot.x, found.spot.z, found.spot.yaw, world) === null, "planCar returns a spot that passes every check");
check(found.spot && Math.abs(found.spot.yaw) < 1 && found.spot.roadDistance > 4, `the car heads along the road and stands off it, got ${JSON.stringify(found.spot)}`);
check(planCar({ id: "b2", footprint: { corners: [[100, 100], [110, 100], [110, 110], [100, 110]] } }, world).spot === null, "a building with no road in reach gets no spot");

// The committed plan: the real mapping and dossiers, every drop placed, deterministic.
const read = (...p) => JSON.parse(readFileSync(join(ROOT, ...p), "utf8"));
const mapping = read("docs", "everon", "places", "saint-philippe", "mapping.json");
const dossiers = { "saint-philippe": read("docs", "everon", "places", "saint-philippe", "dossier.json"), "saint-philippe-dock": read("docs", "everon", "places", "saint-philippe-dock", "dossier.json") };
const a = plan({ mapping, dossiers, others: { all: [], taxi: [] } });
const b = plan({ mapping, dossiers, others: { all: [], taxi: [] } });
check(a.problems.length === 0, `every drop gets a spot, got ${a.problems.join("; ")}`);
check(a.out.length === mapping.slots["parked-cars"].drops.length && a.out.length >= 6 && a.out.length <= 10, `six to ten markers, got ${a.out.length}`);
check(JSON.stringify(a.out.map((o) => o.spot)) === JSON.stringify(b.out.map((o) => o.spot)), "the plan is deterministic");

if (fails.length) {
  for (const f of fails) console.log(`FAIL ${f}`);
  console.log(`SUMMARY town-parked-cars-selftest fail ${fails.length}`);
  process.exit(1);
}
console.log("SUMMARY town-parked-cars-selftest ok");

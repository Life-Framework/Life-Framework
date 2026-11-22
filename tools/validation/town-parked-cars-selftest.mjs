// Proves the parked-car slot planner (`town parked-cars`, #3532): a car spot clears buildings, doors, road carriageways, props and
// other markers, heads along the road, and the committed Saint-Philippe plan is deterministic and refuses nothing.
// inputs: tools/town/parked-cars.mjs, docs/everon/places/saint-philippe/mapping.json, docs/everon/places/saint-philippe/dossier.json, docs/everon/places/saint-philippe-dock/dossier.json
// red-proof: 2026-11-19, with the road test in refuse() removed the "on the carriageway" case below printed FAIL (a car 1 m off a road centre line was accepted); restored, it passes.
// red-proof: 2026-11-21 (#3875), with the guard-rail height 0.75 removed from HEIGHTS the "CrashBarrier_01_4m_V2" case below printed FAIL (the barrier a police car clipped slipped between the two rays); restored, it passes.
// red-proof: 2026-11-21 (#3879), with the level-ray test in bodyWhy removed the "WoodenWall_01_2m" case printed FAIL (b15 stood against it); restored, it passes.
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { SPREAD, bodyRays, bodyWhy, candidates, carBox, distToLine, distToPoly, groundRays, nearestRoad, parseRays, plan, planCar, refuse, surfaceOf } from "../town/parked-cars.mjs";

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

// The island probe (#3879): candidates, the surface under a box, the level rays and the door swing are judged without a server.
const many = candidates(house, world, 14, 5);
check(many.list.length === 5 && many.list.every((c, i) => c.idx === i && many.list.slice(0, i).every((k) => Math.hypot(k.x - c.x, k.z - c.z) >= SPREAD)), "candidates keeps the best few, spread apart, indexed");
check(many.list[0].x === found.spot.x && many.list[0].z === found.spot.z, "the first candidate is planCar's spot");
const ray = (id, o) => `ray ${id} d=${o.d ?? 6} len=${o.len ?? 6} hit=${o.hit ?? "none"} root=${o.root ?? "none"} p=${o.p ?? "0,5,0"} sy=${o.sy ?? 5}`;
const down = (ys) => parseRays(ys.map((y, i) => ray(`c_g${i}`, { sy: y, p: `0,${y},0` })).join("\n"));
check(surfaceOf("c", down([5, 5, 5.1, 5.2, 5])).g === 5, "a flat street gives the centre's height");
check(/varies/.test(surfaceOf("c", down([5, 5, 5, 6.2, 5])).why), "a box over a step is refused");
check(/not the street/.test(surfaceOf("c", parseRays([ray("c_g0", { hit: "Garage", root: "Garage", d: 3, p: "0,7.5,0", sy: 5 }), ...[1, 2, 3, 4].map((i) => ray(`c_g${i}`, {}))].join("\n"))).why), "a roof or canopy over the centre is not the street");
const spot = { x: 0, z: 0, yaw: 0, idx: 0 };
check(groundRays([{ id: "c", c: spot, y0: 5 }]).length === 5 && bodyRays([{ id: "c", c: spot, g: 5 }]).length === 21, "5 down rays and 7 level rays at 3 heights per candidate");
const clear = parseRays(Array.from({ length: 21 }, (_, i) => ray(`c_b${Math.floor(i / 7)}${i % 7}`, {})).join("\n"));
check(bodyWhy("c", spot, 5, clear, []) === null, "clear rays and no door: clean");
const wall = parseRays(Array.from({ length: 21 }, (_, i) => ray(`c_b${Math.floor(i / 7)}${i % 7}`, i === 9 ? { hit: "WoodenWall_01_2m", root: "WoodenWall_01_2m", d: 1 } : {})).join("\n"));
check(/WoodenWall_01_2m at car height/.test(bodyWhy("c", spot, 5, wall, []) ?? ""), "a wall on an outline ray refuses the spot");
const rail = parseRays(Array.from({ length: 21 }, (_, i) => ray(`c_b${Math.floor(i / 7)}${i % 7}`, i === 10 ? { hit: "CrashBarrier_01_4m_V2", root: "CrashBarrier_01_4m_V2", d: 1 } : {})).join("\n"));
check(/CrashBarrier_01_4m_V2 at car height/.test(bodyWhy("c", spot, 5, rail, []) ?? ""), "a guard rail only the 0.75 m ray meets refuses the spot");
const leaf = { root: "DoorSet_MetalGate_01_Short", box: { c: [1.6, 6, 0], h: [1.2, 1.1, 0.05], a: [[1, 0, 0], [0, 1, 0], [0, 0, 1]] } };
check(/swing of a door of DoorSet_MetalGate_01_Short/.test(bodyWhy("c", spot, 5, clear, [leaf]) ?? ""), "a gate leaf whose swing reaches the car refuses the spot");
check(bodyWhy("c", spot, 5, clear, [{ ...leaf, box: { ...leaf.box, c: [30, 6, 0] } }]) === null, "a gate far away is no reason");
const shortlist = plan({ mapping: { slots: { "parked-cars": { drops: [{ place: "t", building: "b1" }] } } }, dossiers: { t: { place: { slug: "t" }, buildings: [house], roads: [road], props: [] } }, others: { all: [], taxi: [] }, tries: 4, accept: (_d, c) => (c.idx === 2 ? { ok: true, y: 7 } : { ok: false, why: "no" }) });
check(shortlist.out.length === 1 && shortlist.out[0].spot.idx === 2 && shortlist.out[0].spot.y === 7, "the probe's answer picks the first accepted candidate and its height");

// A drop with "count" puts that many markers beside its building (#3875): distinct names, a marker's clearance apart, all passing the same checks.
const row = plan({ mapping: { slots: { "police-cars": { drops: [{ place: "t", building: "b1", count: 3 }] } } }, dossiers: { t: { place: { slug: "t" }, buildings: [house], roads: [road], props: [] } }, others: { all: [], taxi: [] }, slot: "police-cars" });
check(row.out.length === 3 && new Set(row.out.map((o) => o.name)).size === 3 && row.out.every((o) => /_car_b1_[123]$/.test(o.name)), `count 3 gives three uniquely named markers, got ${row.out.map((o) => o.name)}`);
check(row.out.every((o, i) => row.out.slice(0, i).every((k) => Math.hypot(k.spot.x - o.spot.x, k.spot.z - o.spot.z) >= 6)), "the markers of one drop stand at least 6 m apart");
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

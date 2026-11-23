// tools/validation/npc-spots-selftest.mjs - validate-npc-spots and tools/lib/npc-spots.mjs (#3831).
// inputs: tools/validation/validate-npc-spots.mjs, tools/lib/npc-spots.mjs, tools/validation/npc-spots-selftest.mjs
//
// Builds a throwaway pack under the OS temp dir (a cafe with one bench anchor and a two-seat bench, three
// residents, a placed host, a scene with spawn markers) and checks that the validator:
//   - fails two residents on one anchor at the same time, naming both and the minutes they share (the cafe bench, evening);
//   - passes the same two when their lines never overlap, and when each has an anchor of its own;
//   - fails three sitters on two seats, passes two on two;
//   - fails a resident on a placed host's spot and a scene's walker on a resident's anchor;
//   - lines walkers up sideways like LF_EventSceneSpawn (two walkers on one marker stand 1.2 m apart, which passes);
//   - fails two spawn markers a few centimetres apart in one scene.
//
// red-proof: 2026-11-19, with the minute-overlap test in collisions() removed the "different hours pass" case printed FAIL;
// with the seat count ignored "three sitters on two seats fail" printed FAIL.
import { spawnSync } from "node:child_process";
import { mkdirSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { anchorsOf, collisions, lineAt, MIN_M, parseLayer, parseResidents, placesOf, sitCollisions, spotOf, walkerSpots } from "../lib/npc-spots.mjs";
import { scratchDir } from "../lib/scratch.mjs";

let failed = 0;
const check = (ok, msg) => {
  console.log(`${ok ? "PASS" : "FAIL"}  ${msg}`);
  if (!ok) failed++;
};

const here = dirname(fileURLToPath(import.meta.url));
const validator = join(here, "validate-npc-spots.mjs");

const guid = (n) => String(n).padStart(16, "0");
const place = (name, kind, id, x, z) => `GenericEntity ${name} {\n components {\n  LF_PlaceComponent "{${guid(1)}}" {\n   m_sKind "${kind}"\n   m_sId "${id}"\n  }\n }\n coords ${x} 10 ${z}\n}\n`;
const anchor = (name, id, x, z) => `GenericEntity ${name} {\n components {\n  LF_ResidentAnchorComponent "{${guid(2)}}" {\n   m_sAnchorId "${id}"\n  }\n }\n coords ${x} 10 ${z}\n angleY 0\n}\n`;
const seat = (name, x, z) => `GenericEntity ${name} : "{${guid(3)}}Prefabs/Props/Seats/LF_Seat_Bench.et" {\n coords ${x} 10 ${z}\n}\n`;
const host = (name, x, z) => `LF_NpcBaseEntity ${name} : "{${guid(4)}}Prefabs/Characters/Npcs/Outfits/LF_Npc_Clerk_1.et" {\n coords ${x} 10 ${z}\n}\n`;
const marker = (name, scene, order, x, z) => `GenericEntity ${name} : "{${guid(5)}}Prefabs/Systems/Events/LF_EventMarker_Spawn.et" {\n components {\n  LF_EventWaypointComponent "{${guid(6)}}" {\n   m_sSceneId "${scene}"\n   m_iOrder ${order}\n  }\n }\n coords ${x} 10 ${z}\n angleY 0\n}\n`;
const resident = (id, lines) =>
  `  LF_ResidentDef "{${guid(10)}}" {\n   m_sOp "add"\n   m_sId "${id}"\n   m_sHomeKind "House"\n   m_sHomePlaceId "home"\n   m_sWorkKind "CafeTable"\n   m_aTimetable {\n` +
  lines.map(([time, where, a, act]) => `    LF_ResidentEntryDef "{${guid(11)}}" {\n     m_sTime "${time}"\n     m_sPlace "${where}"\n     m_sAnchor "${a}"\n     m_sActivity "${act}"\n    }\n`).join("") +
  `   }\n  }\n`;
const conf = (...defs) => `LF_ResidentsConfig {\n m_aResidents {\n${defs.join("")} }\n}\n`;
const scene = (id, count) => `LF_EventScene {\n m_sSceneId "${id}"\n m_iNpcCount ${count}\n m_sRole "townsfolk"\n}\n`;

// A scratch repo: the layer text and resident conf the case needs; the seat prefab has two sit places.
function run(name, layer, residents, scenes = "") {
  const root = scratchDir("lf-npc-spots-");
  const dirs = ["addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers", "addons/LF_Everon1989/Configs/Residents", "addons/LF_Everon1989/Configs/Events", "addons/LifeFramework/Prefabs/Props/Seats"];
  for (const d of dirs) mkdirSync(join(root, d), { recursive: true });
  writeFileSync(join(root, dirs[0], "Place_cafe.layer"), layer);
  writeFileSync(join(root, dirs[1], "LF_Residents_Test.conf"), residents);
  writeFileSync(join(root, dirs[2], "LF_Scene_Test.conf"), scenes);
  writeFileSync(join(root, dirs[3], "LF_Seat_Bench.et"), 'GenericEntity {\n components {\n  LF_SitAction "{A}" {\n  }\n  LF_SitAction "{B}" {\n  }\n }\n}\n');
  const r = spawnSync(process.execPath, [validator], { encoding: "utf8", env: { ...process.env, LF_NPC_SPOTS_ROOT: root } });
  return { ok: r.status === 0, out: r.stdout + r.stderr, name };
}

const cafe = place("Cafe", "CafeTable", "cafe", 0, 0) + place("Home", "House", "home", 500, 500) + anchor("a_bench", "bench", 2, 2) + anchor("a_bench2", "bench2", 4, 2) + anchor("a_counter", "counter", 6, 2) + seat("bench_seat", 2, 2.4);
const evening = [["19:00", "work", "bench", "sit"], ["23:00", "home", "bench", "sit"]];

// Two residents on one anchor in the evening: the cafe counter at 19:30.
const day = (id, ...lines) => resident(id, lines.map(([time, anchor, act]) => [time, "work", anchor, act]));
let r = run("two on one anchor", cafe, conf(day("old_man", ["19:30", "counter", "serve"], ["22:30", "bench2", "idle"]), day("mireille", ["19:00", "counter", "serve"], ["22:30", "door2", "idle"])));
check(!r.ok && /old_man/.test(r.out) && /mireille/.test(r.out) && /19:30/.test(r.out), "two residents on one anchor at the same time fail, naming both and the minute");

// Swapping anchors: never on one at the same minute.
r = run("swap", cafe, conf(day("a", ["06:00", "counter", "serve"], ["12:00", "bench2", "idle"]), day("b", ["06:00", "bench2", "idle"], ["12:00", "counter", "serve"])));
check(r.ok, "two residents who swap anchors never share one at a minute and pass");
r = run("own anchors", cafe, conf(day("a", ["06:00", "counter", "serve"]), day("b", ["06:00", "bench2", "idle"])));
check(r.ok, "residents with an anchor each pass");

// Sitters against seats: the bench has two places.
const sit = (id, start, after = "bench2") => day(id, [start, "bench", "sit"], ["23:00", after, "idle"]);
r = run("three on two seats", cafe, conf(sit("a", "17:30"), sit("b", "19:00"), sit("c", "19:30")));
check(!r.ok && /sit on 2 place/.test(r.out), "three sitters on a two-seat bench fail");
r = run("two on two seats", cafe, conf(sit("a", "17:30"), sit("b", "19:00", "counter")));
check(r.ok, "two sitters on a two-seat bench pass");

// A resident on a placed host's spot.
r = run("resident on host", cafe + host("clerk", 6, 2), conf(day("a", ["08:00", "counter", "serve"], ["20:00", "bench2", "idle"])));
check(!r.ok && /host clerk/.test(r.out), "a resident on a placed host's spot fails");

// Scene walkers: lined up sideways, 1.2 m apart on one marker (passes); a walker on a resident's anchor fails; markers a hand apart fail.
r = run("walkers lined up", cafe + marker("s0", "sq", 0, 50, 50), conf(day("a", ["08:00", "counter", "serve"], ["20:00", "bench2", "idle"])), scene("sq", 3));
check(r.ok, "three walkers on one marker line up 1.2 m apart and pass");
r = run("walker on resident", cafe + marker("s0", "sq", 0, 6, 2), conf(day("a", ["08:00", "counter", "serve"], ["20:00", "bench2", "idle"])), scene("sq", 1));
check(!r.ok && /walker sq#0/.test(r.out), "a walker spawning on a resident's anchor fails");
r = run("markers a hand apart", cafe + marker("s0", "sq", 0, 50, 50) + marker("s1", "sq", 1, 50.3, 50), conf(day("a", ["08:00", "counter", "serve"], ["20:00", "bench2", "idle"])), scene("sq", 2));
check(!r.ok && /walker sq#0/.test(r.out) && /walker sq#1/.test(r.out), "two spawn markers 0.3 m apart fail");

// An anchor bound to a place by id (#4613): a depot counter 23 m from a barber, both residents on a `counter` line all day.
// A loose depot counter is claimed by the barber too (the nearest BarberChair place within 30 m): two on one spot.
const bound = (name, id, placeId, x, z) => anchor(name, id, x, z).replace(`m_sAnchorId "${id}"`, `m_sAnchorId "${id}"\n   m_sPlaceId "${placeId}"`);
const shopper = (id, kind, placeId) =>
  `  LF_ResidentDef "{${guid(10)}}" {\n   m_sOp "add"\n   m_sId "${id}"\n   m_sWorkKind "${kind}"\n   m_sWorkPlaceId "${placeId}"\n   m_aTimetable {\n    LF_ResidentEntryDef "{${guid(11)}}" {\n     m_sTime "08:00"\n     m_sPlace "work"\n     m_sAnchor "counter"\n     m_sActivity "serve"\n    }\n   }\n  }\n`;
const street = place("Depot", "ParcelDepot", "depot", 0, 0) + place("Barber", "BarberChair", "barber", 23, 0);
const pair = conf(shopper("postmaster", "ParcelDepot", "depot"), shopper("barber", "BarberChair", "barber"));
r = run("loose counter claimed by both", street + anchor("depot_counter", "counter", 1, 1), pair);
check(!r.ok && /postmaster/.test(r.out) && /barber/.test(r.out), "a loose depot counter is claimed by the barber too, and the two collide");
r = run("bound counter", street + bound("depot_counter", "counter", "depot", 1, 1), pair);
check(r.ok, "a depot counter bound to the depot id is the postmaster's alone, the barber stands at his own origin");
r = run("each bound", street + bound("depot_counter", "counter", "depot", 1, 1) + bound("barber_counter", "counter", "barber", 22, 1), pair);
check(r.ok, "each shop's counter bound to its own id passes");
const ents = parseLayer(street + bound("depot_counter", "counter", "barber", 1, 1) + anchor("loose_near", "bench", 24, 1));
const [depotP, barberP] = placesOf(ents);
const anchorsHere = anchorsOf(ents);
check(anchorsHere.find((x) => x.id === "counter")?.placeId === "barber" && anchorsHere.find((x) => x.id === "bench")?.placeId === "", "a layer anchor reads its m_sPlaceId (empty when loose)");
check(spotOf(depotP, "counter", anchorsHere, [depotP, barberP]).via.includes("place origin"), "an anchor bound to the barber is never claimed by the depot, although it stands 1 m from it");
check(spotOf(barberP, "counter", anchorsHere, [depotP, barberP]).via === "depot_counter", "the same anchor is the barber's at any distance");
check(spotOf(barberP, "bench", anchorsHere, [depotP, barberP]).via === "loose_near", "a loose anchor still goes to the nearest place of the kind");

// Pure helpers.
const layerEntities = parseLayer(host("clerk", 1.5, 2.5) + anchor("a", "door", 3, 4));
check(layerEntities.length === 2 && layerEntities[0].x === 1.5 && layerEntities[1].z === 4, "a layer parses into entities with their coordinates");
const defs = parseResidents(conf(resident("x", [["23:00", "home", "door", "idle"], ["07:00", "work", "counter", "serve"]])));
check(defs.length === 1 && defs[0].lines.length === 2 && defs[0].lines[0].time === "07:00", "a resident conf parses, lines sorted by time");
check(lineAt(defs[0], 6 * 60).place === "home" && lineAt(defs[0], 8 * 60).place === "work" && lineAt(defs[0], 23 * 60 + 30).place === "home", "the line at a minute is the last one started, wrapping past midnight");
const lined = walkerSpots({ id: "s", count: 3 }, [{ order: 0, x: 0, y: 0, z: 0, yaw: 0, name: "m" }]);
check(Math.abs(lined[1].x - lined[0].x) >= 1.19 && Math.abs(lined[2].x - lined[0].x) >= 1.19 && Math.abs(lined[1].x - lined[2].x) >= 2.39, "walkers on one marker stand 1.2 m apart sideways (left first)");
check(collisions([{ type: "host", who: "a", x: 0, y: 0, z: 0 }, { type: "host", who: "b", x: MIN_M - 0.01, y: 0, z: 0 }]).length === 1 && collisions([{ type: "host", who: "a", x: 0, y: 0, z: 0 }, { type: "host", who: "b", x: MIN_M, y: 0, z: 0 }]).length === 0, "two hosts closer than 0.8 m collide, at 0.8 m they do not");
check(sitCollisions([], []).length === 0, "no sitters, no collisions");

console.log(failed ? `npc-spots-selftest: ${failed} FAILED` : "npc-spots-selftest: OK");
process.exit(failed ? 1 : 0);

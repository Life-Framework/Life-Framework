#!/usr/bin/env node
// inputs: tools/buildings/**, agent-box/proofs/building-proof.mjs, agent-box/layers.mjs
// Offline checks of `tools\cli building proof` and `building plan` (#1143): the scenario file format, building
// space to world, the kept-value checks and log rules, the door approach and walkable verdict, the scale
// verdict, room inference and the wall a door stands in, and that every committed proof.json parses.
// red-proof: 2026-11-15, toWorld with the yaw sign flipped -> FAIL toWorld: yaw 90 turns +Z onto +X
// red-proof: 2026-11-15, walkable needing every lane clear -> FAIL walkable: one clear lane is a doorway
import { existsSync, readdirSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { checkFails, loadScenario, logFails, scenarioProblems, toWorld } from "../buildings/pipeline/scenario.mjs";
import { scaleVerdict } from "../buildings/pipeline/proof.mjs";
import { inferRooms, wallOf } from "../buildings/pipeline/plan.mjs";
import { doorPlan, walkable } from "../../agent-box/proofs/building-proof.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const fails = [];
const ok = (cond, what) => {
  if (!cond) fails.push(what);
};
const near = (a, b) => a.every((v, i) => Math.abs(v - b[i]) < 1e-3);

const place = { x: 100, y: 1, z: 50, yaw: 90 };
ok(near(toWorld(place, [0, 0, 1]), [101, 1, 50]), "toWorld: yaw 90 turns +Z onto +X");
ok(near(toWorld(place, [0, 2, 1], true), [1, 2, 0]), "toWorld: an offset turns but does not move");
ok(near(toWorld({ x: 76, y: 0.1, z: 96.45 }, [-2.4, 2.3, -7]), [73.6, 2.4, 89.45]), "toWorld: no yaw adds the origin");

const kept = { s0: { cash: "100", alarm: "" }, s1: { cash: "86", wanted: "0" }, c: { wanted: "2" } };
ok(checkFails(kept, { lt: ["s1.cash", "s0.cash"] }) === null, "checks: lt reads kept values");
ok(checkFails(kept, { lt: ["s0.cash", "s1.cash"] }) !== null, "checks: a false lt fails");
ok(checkFails(kept, { eq: ["s1.wanted", 0] }) === null, "checks: eq compares numbers");
ok(checkFails(kept, { gt: ["c.wanted", 0] }) === null, "checks: gt");
ok(checkFails(kept, { in: ["s0.alarm", ["", "none"]] }) === null, "checks: in accepts a missing alarm");
ok(checkFails(kept, { in: ["s0.cash", ["", "none"]] }) !== null, "checks: in refuses a value not listed");

const log = "x\nstore: a paid 20 for 3 item(s)\nalarm sounding at <73.7 2.4 89.5>\nalarm sounding at <1 1 1>\n";
ok(!logFails(log, [{ re: "paid \\d+ for 3 item" }], place).length, "log: a present line passes");
ok(logFails(log, [{ re: "walked out" }], place).length === 1, "log: a missing line fails");
ok(logFails(log, [{ last: "alarm sounding at <([^>]+)>", near: [-2.4, 2.3, -7] }], { x: 76, y: 0.1, z: 96.45 }).length === 1, "log: last reads the last match only");

ok(scenarioProblems({ steps: [{ shot: "a" }] }).length === 1, "scenario: a shot without look/from is refused");
ok(scenarioProblems({ steps: [{ pickup: "Apple", do: { verb: "x" } }] }).length === 1, "scenario: two kinds in one step are refused");
for (const slug of readdirSync(join(ROOT, "tools", "buildings")).filter((d) => existsSync(join(ROOT, "tools", "buildings", d, "proof.json")))) {
  const p = scenarioProblems(loadScenario(ROOT, slug));
  ok(!p.length, `tools/buildings/${slug}/proof.json: ${p.join("; ")}`);
}

const front = doorPlan({ name: "DoorSet_X_EXT", at: [0, 0.1, -6.775], yaw: 0 });
ok(front.look[2] + front.from[2] < -6.775 - 1, "door: an outside door is approached from outside");
const inner = doorPlan({ name: "DoorSet_X_INT", at: [-5.175, 0.1, 3.413], yaw: -90 });
ok(inner.look[0] + inner.from[0] > -5.175, "door: an inside door is approached from the side nearer the centre");
ok(Math.abs(front.look[1] + front.from[1] - 0.1) < 1e-6, "door: the feet stand on the door's floor");
const clear = { d: 2.4, total: 2.4 };
const hit = { d: 0.9, total: 2.4 };
ok(walkable([[hit, hit, hit], [clear, clear, clear], [hit, clear, hit]]), "walkable: one clear lane is a doorway");
ok(!walkable([[hit, clear, clear], [clear, hit, clear]]), "walkable: no lane clear at every height is not");

ok(scaleVerdict([18, 14, 6.9], [[5, 10, 3.3], [13, 20, 8.8], [20, 11, 11.3]]).ok, "scale: a shop among houses passes");
ok(!scaleVerdict([18, 14, 30], [[5, 10, 3.3], [13, 20, 8.8]]).ok, "scale: a 30 m shop among 9 m houses fails");
ok(scaleVerdict([1, 1, 1], []).ok, "scale: no neighbours is a note, not a failure");

const rooms = inferRooms([
  { name: "Toilet_01", at: [7, 0, 3] },
  { name: "ShelfShop_01", at: [0, 0, -3] },
  { name: "ShelfShop_01", at: [3, 0, -1] },
  { name: "LightSwitch_02", at: [6.5, 1.2, 3.2] },
]);
const wc = rooms.find((r) => r.kind === "wc");
ok(rooms.length === 2 && wc?.fittings.length === 1, "rooms: a fitting joins the nearest room");
ok(rooms.find((r) => r.kind === "sales hall")?.x.join() === "0,3", "rooms: a room spans its furniture");
const walls = { min: [-9, 0, -7], max: [9, 0, 7] };
ok(wallOf([0, 0, -6.8], walls) === "front" && wallOf([8.9, 0, 0], walls) === "right" && wallOf([0, 0, 2], walls) === "inside", "walls: front, side and inside");

for (const f of fails) console.log(`FAIL ${f}`);
console.log(`SUMMARY building-proof-plan-selftest ${fails.length ? "fail" : "ok"} ${fails.length} failed`);
process.exit(fails.length ? 1 : 0);

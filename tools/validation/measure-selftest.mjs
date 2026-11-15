// Proves `tools\cli measure` (tools/lib/xob.mjs, tools/lib/game-prefab.mjs): a synthetic .xob with a box,
// a turned box, a convex hull, a trimesh (its BVH tail skipped by header) and a box after it parses to
// the right surfaces; prefab names resolve; and, where the game is installed, tools/buildings/pipeline/fixtures.json
// (boards and goods footprints, #1143) is what a fresh measure of the real models gives.
// inputs: tools/lib/xob.mjs, tools/lib/game-prefab.mjs, tools/commands/measure.mjs, tools/buildings/pipeline/fixtures.mjs, tools/buildings/pipeline/fixtures.json, tools/buildings/*/spec.json, tools/validation/measure-selftest.mjs
// red-proof: 2026-11-14, the trimesh up test flipped (n[1] > 0 skipped instead of n[1] < 0) failed
// "trimesh top at 0.9", "trimesh underside not a surface" and the live CoolingBoxShop_01 board 0.66.
// A wrong box record size (72 for 76) still passed: the next record is found by its header, by design.

import { readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { colliders, measureXob } from "../lib/xob.mjs";
import { editableTwin, guidIn, splitRef } from "../lib/game-prefab.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const failures = [];
const check = (name, ok, more = "") => {
  if (!ok) failures.push(more ? `${name} (${more})` : name);
};
const near = (a, b, eps = 1e-4) => Math.abs(a - b) <= eps;

// ------------------------------------------------------------------ a synthetic model

const f32 = (...v) => {
  const b = Buffer.alloc(v.length * 4);
  v.forEach((n, i) => b.writeFloatLE(n, i * 4));
  return b;
};
const u16 = (...v) => {
  const b = Buffer.alloc(v.length * 2);
  v.forEach((n, i) => b.writeUInt16LE(n, i * 2));
  return b;
};
const u32 = (...v) => {
  const b = Buffer.alloc(v.length * 4);
  v.forEach((n, i) => b.writeUInt32LE(n, i * 4));
  return b;
};
const chunk = (id, body) => Buffer.concat([Buffer.from(id, "ascii"), Buffer.from([body.length >>> 24, (body.length >>> 16) & 255, (body.length >>> 8) & 255, body.length & 255]), body]);
const I = [1, 0, 0, 0, 1, 0, 0, 0, 1];
const header = (type, axes, pos, name, mat) => Buffer.concat([Buffer.from([type, 0xff, 6, 0]), f32(...axes, ...pos), u32(0), u16(name, mat), u32(0)]);

const strings = ["Model", "UBX_board", "wood.gamemat", "UBX_turned", "UCX_crate", "UTM_table", "UBX_last"];
const blob = Buffer.from(strings.join("\0") + "\0", "latin1");
const head = Buffer.concat([u32(0), f32(-1, 0, -0.5, 1, 1.5, 0.5), Buffer.alloc(56 - 28), u32(blob.length), blob]);

// 1. A board: centre y 0.5, half height 0.01 -> top 0.51, x -1..1, z -0.2..0.2.
const board = Buffer.concat([header(3, I, [0, 0.5, 0], 1, 2), f32(1, 0.01, 0.2)]);
// 2. A box turned 30 degrees about x: no axis is vertical, so no flat top.
const c = Math.cos(Math.PI / 6), s = Math.sin(Math.PI / 6);
const turned = Buffer.concat([header(3, [1, 0, 0, 0, c, s, 0, -s, c], [0, 1, 0], 3, 2), f32(0.2, 0.2, 0.2)]);
// 3. A convex crate: a 0.4 m cube at x 0.5, its top at 0.4 (local verts, record moved by pos).
const cube = [];
for (const y of [0, 0.4]) for (const x of [-0.2, 0.2]) for (const z of [-0.2, 0.2]) cube.push(x, y, z);
const convex = Buffer.concat([header(4, I, [0.5, 0, 0], 4, 2), u16(8, 6, 12, 24), f32(...cube), Buffer.alloc(24 * 6), Buffer.alloc(6 * 4)]);
// 4. A trimesh table top at 0.9 (two triangles wound to face up) and its underside (wound down), then a BVH.
const tv = [-0.3, 0.9, -0.3, 0.3, 0.9, -0.3, 0.3, 0.9, 0.3, -0.3, 0.9, 0.3, -0.3, 0.88, -0.3, 0.3, 0.88, -0.3, 0.3, 0.88, 0.3, -0.3, 0.88, 0.3];
const tris = [0, 2, 1, 0, 3, 2, 4, 5, 6, 4, 6, 7];
const trimesh = Buffer.concat([header(0x85, I, [-0.5, 0, 0], 5, 2), u16(8, 4), f32(...tv), u16(...tris), u32(2, 1, 1), f32(-1, -1, -1, 1, 1, 1, 100, 100, 100), Buffer.alloc(3 * 16), u32(0)]);
// 5. A box after the trimesh: found by its header past the BVH.
const last = Buffer.concat([header(3, I, [0, 0.05, 0.4], 6, 2), f32(0.1, 0.05, 0.1)]);
const coll = Buffer.concat([board, turned, convex, trimesh, last]);
const body = Buffer.concat([Buffer.from("XOB9", "ascii"), chunk("HEAD", head), chunk("COLL", coll), chunk("LODS", Buffer.alloc(8))]);
const xob = Buffer.concat([Buffer.from("FORM", "ascii"), u32(0).fill(0), body]);
xob.writeUInt32BE(body.length, 4);

const col = colliders(xob);
check("five colliders read", col.list.length === 5, `got ${col.list.length}`);
check("not truncated", !col.truncated);
check("names from HEAD", col.list[0]?.name === "UBX_board" && col.list[0]?.material === "wood.gamemat", `${col.list[0]?.name}/${col.list[0]?.material}`);
check("box after a trimesh is found", col.list[4]?.kind === "box" && col.list[4]?.name === "UBX_last");
const m = measureXob(xob, { minSize: 0 });
const at = (y) => m.surfaces.find((x) => near(x.y, y, 1e-3));
check("bounds from HEAD", near(m.bounds.min[0], -1) && near(m.bounds.max[1], 1.5));
check("board top at 0.51", at(0.51) && near(at(0.51).x[0], -1) && near(at(0.51).z[1], 0.2));
check("turned box has no flat top", !m.surfaces.some((x) => x.name === "UBX_turned"));
check("convex top at 0.4, moved to x 0.5", at(0.4) && near(at(0.4).x[0], 0.3) && near(at(0.4).x[1], 0.7));
check("trimesh top at 0.9", at(0.9) && near(at(0.9).x[0], -0.8) && near(at(0.9).area, 0.36));
check("trimesh underside not a surface", !at(0.88));
check("sorted from the top", m.surfaces.every((x, i, a) => i === 0 || a[i - 1].y >= x.y));
const scaled = measureXob(xob, { scale: 2, minSize: 0 });
check("prefab scale applies", scaled.surfaces.some((x) => near(x.y, 1.02, 1e-3)));
check("minSize drops a narrow top", !measureXob(xob, { minSize: 0.25 }).surfaces.some((x) => x.name === "UBX_last"));

// ------------------------------------------------------------------ names

check("splitRef GUID", splitRef("{B7669B7F8B7923F6}Prefabs/A.et").guid === "B7669B7F8B7923F6");
check("splitRef bare path", splitRef("Prefabs\\A.et").path === "Prefabs/A.et" && splitRef("Prefabs/A.et").guid === null);
check("guidIn finds the reference", guidIn('X : "{10C26F96EF7A8CF5}Prefabs/Props/Commercial/ShelfShop_01.et" {', "Prefabs/Props/Commercial/ShelfShop_01.et") === "10C26F96EF7A8CF5");
check("guidIn is not fooled by a longer name", guidIn('"{10C26F96EF7A8CF5}Prefabs/A_01.et"', "Prefabs/A.et") === null);
check("editable twin path", editableTwin("Prefabs/Props/Commercial/ShelfShop_01.et") === "PrefabsEditable/Auto/Props/Commercial/E_ShelfShop_01.et");

// ------------------------------------------------------------------ the real models, where the game is installed

let live = "skipped (no game install or MCP clone; CI)";
try {
  const { gameVfs } = await import("../lib/game-prefab.mjs");
  await gameVfs();
  const { measureAll, diffFixtures } = await import("../buildings/pipeline/fixtures.mjs");
  const committed = JSON.parse(readFileSync(join(ROOT, "tools", "buildings", "pipeline", "fixtures.json"), "utf8"));
  // fixtures.json is generated by `toolscli fixtures`; the traces in game that first measured the supermarket's
  // boards are LF_Test_StoreSnapToBoard's heights (store-fixtures-selftest checks them against this file).
  const diff = diffFixtures(committed, await measureAll(ROOT));
  check("fixtures.json matches a fresh measure of the models (rerun tools\\cli fixtures)", diff.length === 0, diff[0] ?? "");
  live = `${Object.keys(committed.fixtures).length} fixtures and ${Object.keys(committed.items).length} items measured, ${diff.length} differ from fixtures.json`;
} catch (e) {
  if (!/MCP clone|pak|Cannot find|ENOENT/i.test(String(e.message))) failures.push(`live measure threw: ${e.message}`);
}

if (failures.length) {
  for (const f of failures) console.error(`FAIL  measure-selftest: ${f}`);
  process.exit(1);
}
console.log(`measure-selftest: OK (synthetic model; live: ${live})`);

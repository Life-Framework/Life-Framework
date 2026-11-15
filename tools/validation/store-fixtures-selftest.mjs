// Proves the store's shelf data and the script constants that depend on it (#1143), offline: tools/buildings/pipeline/fixtures.json
// names every fixture and item a committed spec's store block uses; boards are found from collider surfaces by rule (a deck's
// strips merge, a lip and a floor bin are not boards, clearance is the gap to the surface above); and the numbers written in
// script (LF_StoreComponent's snap and stray distances, the heights in LF_Test_StoreSnapToBoard) agree with the measured boards.
// inputs: tools/buildings/pipeline/fixtures.mjs, tools/buildings/pipeline/fixtures.json, tools/buildings/*/spec.json, addons/LifeFramework/Scripts/Game/Feature/Shop/LF_StoreComponent.c, addons/LifeFramework/Scripts/Game/Tests/LF_Test_StoreSnapToBoard.c, tools/validation/store-fixtures-selftest.mjs
// red-proof: 2026-11-15, one line broken per block, each restored after its FAIL was seen:
//   the boardsOf MERGE_DY set to 0                          -> FAIL boards: a cooler deck sloped in three strips is one board
//   the boardsOf MIN_DEPTH set to 0                         -> FAIL boards: a 7 cm lip is not a board
//   SNAP_ABOVE in LF_StoreComponent.c set to 0.5            -> FAIL script: the spawn trace starts under the next board up
//   GONDOLA_BOARD_2 in LF_Test_StoreSnapToBoard.c set to 0.9 -> FAIL script: the snap test's third board is the measured 0.82
import { readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { boardsOf, itemKey, wanted } from "../buildings/pipeline/fixtures.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const failures = [];
const ok = (cond, what) => {
  if (!cond) failures.push(what);
};
const surf = (y, x0, x1, z0, z1) => ({ y, x: [x0, x1], z: [z0, z1] });

// ---- boards from surfaces
const cooler = [surf(1.212, -0.895, 0.894, -0.003, 0.304), surf(1.209, -0.869, 0.868, 0.036, 0.278), surf(0.874, -0.9, 0.9, -0.635, -0.356), surf(0.646, -0.9, 0.9, -0.381, -0.122), surf(0.623, -0.9, 0.9, -0.122, 0.138), surf(0.601, -0.9, 0.9, 0.138, 0.418), surf(0.659, -0.965, -0.9, -0.289, 0.421), surf(0.065, -0.903, 0.903, -0.574, -0.358)];
const cb = boardsOf(cooler);
ok(cb.length === 3 && cb[0].y === 0.646 && cb[0].z[0] === -0.381 && cb[0].z[1] === 0.418, `boards: a cooler deck sloped in three strips is one board (${JSON.stringify(cb)})`);
ok(cb.every((b) => b.y > 0.09) && !cb.some((b) => b.y === 0.065), "boards: the bin inside a cooler's floor is not a board");
ok(cb[0].clear !== null && Math.abs(cb[0].clear - (1.212 - 0.646)) < 0.02 && cb[2].clear === null, "boards: clearance is the gap to the surface above, null when open");
const gondola = [surf(1.426, -1.176, 1.228, 0.001, 0.072), surf(1.177, -1.167, 1.222, 0.05, 0.334), surf(0.82, -1.167, 1.222, 0.048, 0.394), surf(0.428, -1.167, 1.222, 0.048, 0.486), surf(0.102, -1.214, 1.264, -0.005, 0.522)];
const gb = boardsOf(gondola);
ok(gb.length === 4 && gb.map((b) => b.y).join() === "0.102,0.428,0.82,1.177", `boards: a 7 cm lip is not a board; the four boards of a gondola, lowest first (${gb.map((b) => b.y)})`);
ok(gb[2].clear === 0.357 && gb[3].clear === null, `boards: the gap above the third board is 0.357 and the top board is open (${gb[2].clear}, ${gb[3].clear})`);

// ---- the committed data covers every spec
const data = JSON.parse(readFileSync(join(ROOT, "tools", "buildings", "pipeline", "fixtures.json"), "utf8"));
const want = wanted(ROOT);
for (const n of want.fixtures) ok(data.fixtures[n]?.boards?.length > 0, `data: fixtures.json has boards for ${n} (run tools\cli fixtures)`);
for (const r of want.items) ok(data.items[itemKey(r)]?.size?.every((v) => v > 0), `data: fixtures.json has a footprint for ${itemKey(r)} (run tools\cli fixtures)`);
for (const [n, f] of Object.entries(data.fixtures)) ok(f.boards.every((b, i) => i === 0 || b.y > f.boards[i - 1].y), `data: ${n} boards are lowest first`);

// ---- script constants against the measured boards
const read = (p) => readFileSync(join(ROOT, p), "utf8");
const constant = (text, name) => {
  const m = new RegExp(`${name}\\s*=\\s*(-?[0-9.]+)`).exec(text);
  return m ? Number(m[1]) : NaN;
};
const comp = read("addons/LifeFramework/Scripts/Game/Feature/Shop/LF_StoreComponent.c");
const snap = read("addons/LifeFramework/Scripts/Game/Tests/LF_Test_StoreSnapToBoard.c");
const above = constant(comp, "SNAP_ABOVE");
const stray = constant(comp, "STRAY_HEIGHT");
const hint = constant(read("tools/buildings/pipeline/store-slots.mjs"), "HINT");
// The smallest gap between two boards that stand over one another, over every fixture.
let gap = Infinity;
for (const f of Object.values(data.fixtures)) {
  for (const b of f.boards) if (b.clear !== null && b.clear < gap) gap = b.clear;
}
ok(Number.isFinite(above) && hint + above < gap, `script: the spawn trace starts under the next board up (point ${hint} above its board + SNAP_ABOVE ${above} must be under the smallest gap ${gap})`);
ok(Number.isFinite(stray) && stray < gap, `script: a copy on the board above is not taken for this slot's (STRAY_HEIGHT ${stray} under the smallest gap ${gap})`);
const near = (a, b) => Math.abs(a - b) < 0.0015;
const g = data.fixtures.ShelfShop_01?.boards ?? [];
ok(near(constant(snap, "GONDOLA_BOARD_1"), g[1]?.y), `script: the snap test's second board is the measured ${g[1]?.y}`);
ok(near(constant(snap, "GONDOLA_BOARD_2"), g[2]?.y), `script: the snap test's third board is the measured ${g[2]?.y}`);
const top = (data.fixtures.CounterShop_01?.boards ?? []).slice(-1)[0];
ok(near(constant(snap, "COUNTER_TOP"), top?.y), `script: the snap test's counter top is the measured ${top?.y}`);

if (failures.length) {
  for (const f of failures) console.error(`FAIL  store-fixtures-selftest: ${f}`);
  process.exit(1);
}
console.log(`store-fixtures-selftest: OK (${Object.keys(data.fixtures).length} fixtures, ${Object.keys(data.items).length} items, smallest board gap ${gap} m)`);

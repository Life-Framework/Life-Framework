// tools/validation/grid-selftest.mjs
// inputs: tools/lib/world-zones.mjs, tools/commands/grid.mjs, tools/validation/validate-world-zones.mjs, tools/place/place.mjs
//
// Proves the debug grid and the zone guard (docs/architecture/world-zones.md) on fixtures:
// allocation is deterministic and skips claimed, reserved and occupied cells; a cell's yard has
// seven fence runs and a sign outside the gap; the rows stay inside the yard; the layer text carries
// the grid:<slug> sign id; validate-world-zones reports two claims on one cell, a grid prop outside
// its cell, a claim with no layer and a grid layer with no claim; the guard fails a slice that edits
// a town layer (core or pack) and passes a town- pass, a baselined branch and a grid layer edit.
// red-proof: with checkGuard's town- test made always true (`TOWN_PASS.test(slug) || true`) this printed
// "FAIL a slice that edits Street_Hospital.layer is refused" and exited 1 (2026-11-15); restored, it passes.

import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import * as P from "../place/place.mjs";
import { yardLayer } from "../commands/grid.mjs";
import { CELLS_DIR, DEBUG_LAYERS, allCells, allocate, blockers, cellBounds, gapSide, inGrid, layerOfSlug, lookArea, rowLine, yardPlan, zoneOfLayer, zoneOfPath } from "../lib/world-zones.mjs";
import { checkClaims, checkGuard, topLevelCoords } from "./validate-world-zones.mjs";

let failed = 0;
const ok = (cond, what) => {
  if (!cond) {
    failed++;
    console.log(`FAIL ${what}`);
  }
};

// ---- zones
ok(zoneOfLayer("Street_Hospital.layer") === "town", "a Street_ layer is town");
ok(zoneOfLayer("Grid_police_gates.layer") === "grid", "a Grid_ layer is grid");
ok(zoneOfLayer("Annex.layer") === "grid", "the legacy annex is grid");
ok(zoneOfLayer("Gamemode.layer") === "system", "Gamemode is neither zone");
ok(zoneOfPath("addons/LF_Everon1989/Worlds/Everon1989Town/Everon1989Town_Layers/Village_Dressing.layer") === "town", "a pack town layer is town");
ok(zoneOfPath(`${DEBUG_LAYERS}/Grid_x.layer`) === "grid" && zoneOfPath("tools/x.mjs") === null, "zoneOfPath");
ok(inGrid(30, 30) && !inGrid(262, 255), "inGrid: the annex yes, the hospital no");

// ---- allocation
const a1 = allocate("police-gates");
ok(a1 && allocate("police-gates").id === a1.id, "the same slug gets the same cell");
ok(allocate("police-gates", { taken: new Set([a1.id]) }).id !== a1.id, "a claimed cell is skipped");
const fixtureCell = allCells().find((c) => c.col === 4 && c.row === 4);
ok(blockers(fixtureCell, []).some((w) => /fixture/.test(w)), "the world-test fixture ground blocks its cells");
const busy = allCells().find((c) => !blockers(c, []).length);
ok(blockers(busy, [{ x: busy.x0 + 5, z: busy.z0 + 5, layer: "Annex.layer", prefab: "Desk.et" }]).length === 1, "a legacy station blocks a cell");
ok(blockers(busy, [{ x: busy.x0 + 5, z: busy.z0 + 5, layer: "Grid_x.layer", prefab: "Desk.et" }]).length === 0, "a grid layer's own props do not block it");
const every = allCells().filter((c) => !blockers(c, []).length);
ok(allocate("x", { taken: new Set(every.map((c) => c.id)) }) === null, "a full grid allocates nothing");

// ---- the yard and rows
const cell = { id: "c0r6", col: 0, row: 6, ...cellBounds(0, 6) };
const plan = yardPlan(cell);
ok(plan.runs.length === 7, `seven fence runs (got ${plan.runs.length})`);
ok(gapSide(cell).side === "east", "a west-edge cell opens east, toward the annex centre");
ok(plan.signAt[0] > plan.yard.x1, "the sign stands outside the gap side");
for (let k = 0; k < 3; k++) {
  const r = rowLine(cell, k);
  ok(r && [r.a, r.b].every(([x, z]) => x > plan.yard.x0 && x < plan.yard.x1 && z > plan.yard.z0 && z < plan.yard.z1), `row ${k} is inside the yard`);
}
ok(rowLine(cell, 3) === null, "a cell has three rows");
const text = yardLayer("police-gates", cell, P);
const coords = topLevelCoords(text);
ok(coords.length === 8, `the yard layer places 7 fences and a sign (got ${coords.length})`);
ok(/m_sSignId "grid:police-gates"/.test(text), "the sign reads grid:<slug>");
ok(coords.every((e) => e.x >= cell.x0 && e.x <= cell.x1 && e.z >= cell.z0 && e.z <= cell.z1), "the yard stays in its cell");
ok(lookArea({ slug: "police-gates", cell: "c0r6" }).front === 90, "look grid: faces the gap");

// ---- claims
function put(root, path, body) {
  const full = join(root, path);
  mkdirSync(dirname(full), { recursive: true });
  writeFileSync(full, body);
}
const claim = (slug, c) => JSON.stringify({ slug, cell: c, layer: layerOfSlug(slug), rows: 0, created: "2026-11-15" });
const root = mkdtempSync(join(tmpdir(), "lf-grid-"));
try {
  put(root, `${CELLS_DIR}/alpha.json`, claim("alpha", "c0r6"));
  put(root, `${DEBUG_LAYERS}/${layerOfSlug("alpha")}`, yardLayer("alpha", cell, P));
  let r = checkClaims(root);
  ok(r.errors.length === 0, `a clean claim passes (${r.errors.join("; ")})`);
  put(root, `${CELLS_DIR}/beta.json`, claim("beta", "c0r6"));
  put(root, `${DEBUG_LAYERS}/${layerOfSlug("beta")}`, "");
  r = checkClaims(root);
  ok(r.errors.some((e) => /claimed by/.test(e)), "two claims on one cell are reported");
  rmSync(join(root, CELLS_DIR, "beta.json"));
  r = checkClaims(root);
  ok(r.errors.some((e) => /Grid_beta\.layer: no claim/.test(e)), "a grid layer with no claim is reported");
  rmSync(join(root, DEBUG_LAYERS, layerOfSlug("beta")));
  put(root, `${DEBUG_LAYERS}/${layerOfSlug("alpha")}`, `${yardLayer("alpha", cell, P)}GenericEntity Grid_alpha_Stray : "{1111111111111111}Prefabs/Desk.et" {\n coords 262 0 255\n}\n`);
  r = checkClaims(root);
  ok(r.errors.some((e) => /Grid_alpha_Stray .* outside cell/.test(e)), "a grid prop outside its cell is reported");
  put(root, `${CELLS_DIR}/gamma.json`, claim("gamma", "c1r6"));
  r = checkClaims(root);
  ok(r.errors.some((e) => /gamma\.json: .*missing/.test(e)), "a claim with no layer is reported");
} finally {
  rmSync(root, { recursive: true, force: true });
}

// ---- the guard
const hosp = `${DEBUG_LAYERS}/Street_Hospital.layer`;
const packTown = "addons/LF_Everon1989/Worlds/Everon1989Town/Everon1989Town_Layers/Village_Dressing.layer";
ok(checkGuard({ branch: "ws/slice-1500", files: [hosp], baseline: [] }).errors.length === 1, "a slice that edits Street_Hospital.layer is refused");
ok(checkGuard({ branch: "ws/slice-1500", files: [packTown], baseline: [] }).errors.length === 1, "a slice that edits the pack town is refused");
ok(checkGuard({ branch: "ws/town-m4-ch2", files: [hosp, packTown], baseline: [] }).errors.length === 0, "a town- pass may edit town layers");
ok(checkGuard({ branch: "ws/slice-1200", files: [hosp], baseline: ["ws/slice-1200"] }).errors.length === 0, "a branch from before the guard keeps working");
ok(checkGuard({ branch: "ws/slice-1500", files: [`${DEBUG_LAYERS}/Grid_slice_1500.layer`, `${DEBUG_LAYERS}/Annex.layer`, "tools/x.mjs"], baseline: [] }).errors.length === 0, "grid and annex layers are free to edit");
ok(checkGuard({ branch: "main", files: [hosp], baseline: [] }).skipped, "main is not checked");

if (failed) {
  console.log(`grid-selftest: ${failed} FAILED`);
  process.exit(1);
}
console.log("grid-selftest: OK");

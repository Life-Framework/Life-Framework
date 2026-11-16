// tools/validation/world-zones-selftest.mjs
// inputs: tools/lib/world-zones.mjs, tools/lib/worlds.mjs, tools/commands/grid.mjs, tools/validation/validate-world-zones.mjs, tools/place/place.mjs
//
// Proves the debug grid and the zone guard (docs/architecture/world-zones.md) on fixtures:
// allocation is deterministic and skips claimed, reserved and occupied cells; a cell's yard has
// seven fence runs and a sign outside the gap; the rows stay inside the yard; the layer text carries
// the grid:<slug> sign id; validate-world-zones reports two claims on one cell, a grid prop outside
// its cell, a claim with no layer and a grid layer with no claim; the guard fails a slice that edits
// a town layer (core or pack) and passes a town- pass, a baselined branch and a grid layer edit.
// red-proof: with checkGuard's town- test made always true (`TOWN_PASS.test(slug) || true`) this printed
// "FAIL a slice that edits Street_Hospital.layer is refused" and exited 1 (2026-11-15); restored, it passes.

import { spawnSync } from "node:child_process";
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import * as P from "../place/place.mjs";
import { main as gridMain, yardLayer } from "../commands/grid.mjs";
import { CELLS_DIR, DEBUG_LAYERS, GRID_LAYERS, allCells, allocate, blockers, cellBounds, gapSide, inGrid, layerOfSlug, lookArea, mainClaims as mainClaimsOf, rowLine, yardPlan, zoneOfLayer, zoneOfPath } from "../lib/world-zones.mjs";
import { checkClaims, checkFreeze, checkGuard, topLevelCoords } from "./validate-world-zones.mjs";

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
ok(GRID_LAYERS === "addons/LifeFramework/Worlds/GridWorld/GridWorld_Layers", `the grid zone is GridWorld_Layers (got ${GRID_LAYERS})`);
ok(zoneOfPath(`${GRID_LAYERS}/Grid_x.layer`) === "grid" && zoneOfPath(`${GRID_LAYERS}/Gamemode.layer`) === "system", "zoneOfPath: GridWorld layers are grid, its Gamemode is system");
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
  put(root, `${GRID_LAYERS}/${layerOfSlug("alpha")}`, yardLayer("alpha", cell, P));
  let r = checkClaims(root);
  ok(r.errors.length === 0, `a clean claim passes (${r.errors.join("; ")})`);
  put(root, `${CELLS_DIR}/beta.json`, claim("beta", "c0r6"));
  put(root, `${GRID_LAYERS}/${layerOfSlug("beta")}`, "");
  r = checkClaims(root);
  ok(r.errors.some((e) => /claimed by/.test(e)), "two claims on one cell are reported");
  rmSync(join(root, CELLS_DIR, "beta.json"));
  r = checkClaims(root);
  ok(r.errors.some((e) => /Grid_beta\.layer: no claim/.test(e)), "a grid layer with no claim is reported");
  rmSync(join(root, GRID_LAYERS, layerOfSlug("beta")));
  put(root, `${GRID_LAYERS}/${layerOfSlug("alpha")}`, `${yardLayer("alpha", cell, P)}GenericEntity Grid_alpha_Stray : "{1111111111111111}Prefabs/Desk.et" {\n coords 262 0 255\n}\n`);
  r = checkClaims(root);
  ok(r.errors.some((e) => /Grid_alpha_Stray .* outside cell/.test(e)), "a grid prop outside its cell is reported");
  put(root, `${CELLS_DIR}/gamma.json`, claim("gamma", "c1r6"));
  r = checkClaims(root);
  ok(r.errors.some((e) => /gamma\.json: .*missing/.test(e)), "a claim with no layer is reported");
  ok(r.errors.some((e) => /gamma\.json: .*GridWorld_Layers/.test(e)), "a claim's missing layer is looked for in GridWorld_Layers");
  // a grid claim resolves to GridWorld: its layer in DebugWorld_Layers does not count, and is itself refused
  put(root, `${CELLS_DIR}/delta.json`, claim("delta", "c2r6"));
  put(root, `${DEBUG_LAYERS}/${layerOfSlug("delta")}`, yardLayer("delta", { id: "c2r6", col: 2, row: 6, ...cellBounds(2, 6) }, P));
  r = checkClaims(root);
  ok(r.errors.some((e) => /delta\.json: .*missing/.test(e)), "a claim whose layer is only in DebugWorld is missing it");
  ok(r.errors.some((e) => /DebugWorld_Layers\/Grid_delta\.layer: .*GridWorld_Layers/.test(e)), "a Grid_ layer in DebugWorld is refused, and the message names GridWorld_Layers");
  // grid place --dry names the GridWorld path (and writes nothing)
  put(root, `${GRID_LAYERS}/Annex.layer`, "");
  const lines = [];
  const realLog = console.log;
  console.log = (...a) => lines.push(a.join(" "));
  let code;
  try {
    code = await gridMain(["place", "w3-probe", "--dry"], { root });
  } finally {
    console.log = realLog;
  }
  ok(code === 0 && lines.some((l) => l.includes(`${GRID_LAYERS}/Grid_w3_probe.layer`)), `grid place --dry names ${GRID_LAYERS}/Grid_w3_probe.layer (got ${lines.join(" | ")})`);
  ok(!lines.some((l) => /DebugWorld_Layers/.test(l)), "grid place --dry does not name DebugWorld_Layers");
  ok(P.layerPath(root, "Grid_w3_probe.layer") === join(root, GRID_LAYERS, "Grid_w3_probe.layer"), "place --layer Grid_* resolves to GridWorld_Layers");
} finally {
  rmSync(root, { recursive: true, force: true });
}

// ---- claims against origin/main: two slices in separate worktrees must not take one cell
// A temp repo whose origin/main already holds a claim the working tree has not merged yet.
const natural = allocate("clash-probe");
const mainClaim = (slug, c) => JSON.stringify({ slug, cell: c, layer: layerOfSlug(slug), rows: 0, created: "2026-11-15" });
const root2 = mkdtempSync(join(tmpdir(), "lf-grid-main-"));
try {
  const git = (...a) => spawnSync("git", ["-c", "user.name=t", "-c", "user.email=t@t", "-c", "commit.gpgsign=false", ...a], { cwd: root2, encoding: "utf8" });
  git("init", "-q");
  put(root2, `${CELLS_DIR}/earlier-slice.json`, mainClaim("earlier-slice", natural.id));
  put(root2, `${GRID_LAYERS}/Annex.layer`, "");
  git("add", "-A");
  git("commit", "-q", "-m", "main has the earlier claim");
  git("update-ref", "refs/remotes/origin/main", "HEAD");
  rmSync(join(root2, CELLS_DIR, "earlier-slice.json")); // the worktree branched before it merged
  const run = async (...argv) => {
    const out = [];
    console.log = (...a) => out.push(a.join(" "));
    try {
      return { code: await gridMain(argv, { root: root2 }), out };
    } finally {
      console.log = realLogMain;
    }
  };
  const realLogMain = console.log;
  const mc = mainClaimsOf(root2, { fetch: false });
  ok(mc.claims.length === 1 && mc.claims[0].cell === natural.id, `origin/main's claims are read from git (got ${JSON.stringify(mc.claims.map((c) => c.cell))})`);
  let g = await run("place", "clash-probe", "--dry", "--no-fetch");
  ok(g.code === 0 && !g.out.some((l) => l.includes(`-> cell ${natural.id} `)), `grid place avoids a cell origin/main already claims (${natural.id}); got ${g.out[0]}`);
  g = await run("place", "clash-probe", "--dry", "--no-fetch", "--cell", natural.id);
  ok(g.code === 1 && g.out.some((l) => /origin\/main/.test(l)), `grid place --cell refuses a cell origin/main claims (${g.out.join(" | ")})`);
  g = await run("place", "earlier-slice", "--dry", "--no-fetch");
  ok(g.code === 1 && g.out.some((l) => /origin\/main already/.test(l)), `grid place refuses a slug origin/main already claims (${g.out.join(" | ")})`);
  g = await run("list", "--no-fetch");
  ok(g.out.some((l) => /origin\/main/.test(l) && l.includes(natural.id)), "grid list shows the claims only origin/main has");
} finally {
  rmSync(root2, { recursive: true, force: true });
}
{
  // validate: a branch claim on a cell origin/main gives to another slug fails; its own claim never does
  const r3 = mkdtempSync(join(tmpdir(), "lf-grid-v-"));
  try {
    put(r3, `${CELLS_DIR}/signs.json`, claim("signs", "c2r4"));
    put(r3, `${GRID_LAYERS}/${layerOfSlug("signs")}`, yardLayer("signs", { id: "c2r4", col: 2, row: 4, ...cellBounds(2, 4) }, P));
    const mainSame = [{ file: `${CELLS_DIR}/signs.json`, slug: "signs", cell: "c2r4" }];
    ok(checkClaims(r3, { mainClaims: mainSame }).errors.length === 0, "a claim origin/main holds under the same slug passes");
    ok(checkClaims(r3).errors.length === 0, "without origin/main claims the check is as before");
    const e = checkClaims(r3, { mainClaims: [{ file: `${CELLS_DIR}/orgs.json`, slug: "orgs", cell: "c2r4" }] }).errors;
    ok(e.some((x) => /signs\.json/.test(x) && /c2r4/.test(x) && /origin\/main/.test(x) && /orgs/.test(x)), `a claim on a cell origin/main gives to another slice is reported (${e.join("; ")})`);
  } finally {
    rmSync(r3, { recursive: true, force: true });
  }
}

// ---- the guard
const hosp =`${DEBUG_LAYERS}/Street_Hospital.layer`;
const packTown = "addons/LF_Everon1989/Worlds/Everon1989Town/Everon1989Town_Layers/Village_Dressing.layer";
ok(checkGuard({ branch: "ws/slice-1500", files: [hosp], baseline: [] }).errors.length === 1, "a slice that edits Street_Hospital.layer is refused");
ok(checkGuard({ branch: "ws/slice-1500", files: [packTown], baseline: [] }).errors.length === 1, "a slice that edits the pack town is refused");
ok(checkGuard({ branch: "ws/town-m4-ch2", files: [hosp, packTown], baseline: [] }).errors.length === 0, "a town- pass may edit town layers");
ok(checkGuard({ branch: "ws/slice-1200", files: [hosp], baseline: ["ws/slice-1200"] }).errors.length === 0, "a branch from before the guard keeps working");
ok(checkGuard({ branch: "ws/slice-1500", files: [`${DEBUG_LAYERS}/Grid_slice_1500.layer`, `${DEBUG_LAYERS}/Annex.layer`, "tools/x.mjs"], baseline: [] }).errors.length === 0, "grid and annex layers are free to edit");
ok(checkGuard({ branch: "main", files: [hosp], baseline: [] }).skipped, "main is not checked");
ok(checkGuard({ branch: "ws/town-m4-ch2", files: [hosp, `${GRID_LAYERS}/Grid_x.layer`], baseline: [] }).errors.length === 0, "a town change on a town- branch still passes beside a GridWorld edit");
ok(checkGuard({ branch: "ws/slice-1500", files: [`${GRID_LAYERS}/Grid_slice_1500.layer`, `${GRID_LAYERS}/Annex.layer`], baseline: [] }).errors.length === 0, "GridWorld layers are free to edit");

// ---- the freeze: no new or changed entity inside the annex rectangle in DebugWorld (until W6 deletes it)
const ent = (name, x, z) => `GenericEntity ${name} : "{1111111111111111}Prefabs/Desk.et" {
 coords ${x} 0 ${z}
}
`;
const annexEnt = ent("Annex_Desk", 30, 30);
const townEnt = ent("Street_Lamp", 262, 255);
const annexLayer = `${DEBUG_LAYERS}/Annex.layer`;
const freeze = (base, now, file = annexLayer) => checkFreeze({ files: [file], baseText: () => base, nowText: () => now });
ok(freeze(annexEnt + townEnt, annexEnt + townEnt).errors.length === 0, "an unchanged annex layer passes the freeze");
ok(freeze(annexEnt, annexEnt + ent("Annex_New", 40, 40)).errors.some((e) => /Annex_New/.test(e)), "a new annex entity in DebugWorld fails");
ok(freeze(annexEnt, ent("Annex_Desk", 31, 30)).errors.some((e) => /Annex_Desk/.test(e)), "a changed annex entity in DebugWorld fails");
ok(freeze(null, annexEnt, `${DEBUG_LAYERS}/Annex_More.layer`).errors.length === 1, "a new layer file holding an annex entity fails");
ok(freeze(annexEnt + townEnt, townEnt).errors.length === 0, "deleting an annex entity passes (W6)");
ok(freeze(annexEnt + townEnt, annexEnt + ent("Street_Lamp", 270, 255)).errors.length === 0, "a changed town entity outside the annex is not the freeze's business");
ok(freeze(annexEnt, annexEnt + ent("Edge", 114, 114)).errors.length === 1 && freeze(annexEnt, annexEnt + ent("Edge", 115, 114)).errors.length === 0, "the rectangle is x 18..114, z 18..114");
ok(checkFreeze({ files: [`${GRID_LAYERS}/Annex.layer`, "tools/x.mjs"], baseText: () => null, nowText: () => annexEnt }).errors.length === 0, "edits in GridWorld are not frozen");

if (failed) {
  console.log(`world-zones-selftest: ${failed} FAILED`);
  process.exit(1);
}
console.log("world-zones-selftest: OK");

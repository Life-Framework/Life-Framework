// The placement step of `wt gate` / `wt ship` / `ci`: run `audit placement --strict` when the branch
// moved or added world props, and `audit placement --grid --strict` when it touched GridWorld or a prefab.
// docs/placement-audit.md; the triggers are proven offline by tools/validation/placement-gate-selftest.mjs.

import { hardLayersOf } from "./town-findings.mjs";

export const ALLOW_PATH = "agent-box/place-audit-allow.json";

// A changed file that can move or add a placed prop: a layer, a prefab, or the spawn code.
export function placementRelevant(file) {
  const f = String(file).split("\\").join("/");
  if (!f.startsWith("addons/")) return false;
  if (f.endsWith(".layer")) return true;
  if (f.endsWith(".et") && /\/Prefabs\//.test(f)) return true;
  if (/\/Scripts\/.*(?:\/LF_Utils\.c|Spawn[^/]*\.c)$/.test(f)) return true;
  return false;
}

export function placementTrigger(files) {
  return files.filter(placementRelevant);
}

// A changed file that can break the grid at its bar (on the ground, reachable): a GridWorld layer or the world itself,
// a prefab a cell may place, or the spawn code. A DebugWorld layer cannot (the grid reads only GridWorld_Layers).
export function gridRelevant(file) {
  const f = String(file).split("\\").join("/");
  if (!f.startsWith("addons/")) return false;
  if (/\/Worlds\/GridWorld\/(GridWorld_Layers\/[^/]+\.layer|GridWorld\.ent)$/.test(f)) return true;
  if (/\/Worlds\/Everon1989Grid\/(Everon1989Grid_Layers\/[^/]+\.layer|Everon1989Grid\.ent)$/.test(f)) return true;
  if (/\/Worlds\/GridWorld\//.test(f)) return false;
  if (f.endsWith(".layer")) return false;
  return placementRelevant(f);
}

export function gridTrigger(files) {
  return files.filter(gridRelevant);
}

export function allowHint() {
  return `fix the placement, or if it is meant to look that way add { "match": "<regex on name or prefab>", "kind": "<problem>", "why": "<reason a reviewer can check in a shot>" } to ${ALLOW_PATH}`;
}

// `audit(args)` runs the audit and returns an exit code (place-audit.mjs main); injected so the selftest runs offline.
export async function runPlacementGate(files, audit, log = console.log) {
  const hit = placementTrigger(files);
  let code = 0;
  if (!hit.length) {
    log("=== ci: placement audit skipped (no .layer, Prefabs .et or LF_Utils/spawn code changed) ===");
  } else {
    // The town is hand-edited and its findings only warn (2026-11-15). What blocks is a flagged entity in a non-town
    // layer this branch adds or moves, which the audit only measures with --annex.
    const hard = hardLayersOf(files);
    log(`=== ci: placement audit --strict (${hit.length} file(s) touch world props, e.g. ${hit[0]}; town findings warn${hard.length ? `, ${hard.join(", ")} must be clean` : ""}) ===`);
    code = await audit(["--strict", "--top", "200", ...(hard.length ? ["--annex", "--hard-layer", hard.join(",")] : [])]);
    if (code !== 0) log(`placement audit FAILED: the FAIL entities above are flagged; ${allowHint()}. See docs/placement-audit.md`);
    else log("placement audit: no blocking finding under --strict (town findings, if any, are warnings)");
  }
  // The grid is clean (0 failures, 2026-11-17, #1814) and stays so: any grid finding blocks, it has no town to warn about.
  const grid = gridTrigger(files);
  if (grid.length) {
    log(`=== ci: placement audit --grid --strict (${grid.length} file(s) touch GridWorld or a prefab, e.g. ${grid[0]}) ===`);
    const gcode = await audit(["--grid", "--strict", "--top", "200"]);
    if (gcode !== 0) log(`grid placement audit FAILED: the FAIL entities above are flagged at the grid bar (on the ground, reachable); ${allowHint()}. See docs/placement-audit.md`);
    else log("grid placement audit: clean");
    if (gcode !== 0) code = 1;
  }
  return code === 0 ? 0 : 1;
}

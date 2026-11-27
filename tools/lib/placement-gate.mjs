// The placement step of `wt gate` / `wt ship` / `ci`: run `audit placement --strict` when the branch
// moved or added world props, `audit placement --grid --strict` when it touched GridWorld or a prefab, and the island
// audit against its baseline when it touched an island layer, kit, generator, marker or spawner (runIslandGate).
// docs/placement-audit.md; the triggers are proven offline by tools/validation/placement-gate-selftest.mjs.

import { hardLayersOf } from "./town-findings.mjs";

export const ALLOW_PATH = "agent-box/place-audit-allow.json";

// A changed file that can move or add a placed prop: a layer, a prefab, or the spawn code.
export function placementRelevant(file) {
  const f = String(file).split("\\").join("/");
  if (!f.startsWith("addons/")) return false;
  if (ISLAND_LAYER.test(f)) return false; // the island step (runIslandGate) audits it on MainWorld
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
  // The island (MainWorld): a new finding blocks, a baseline one warns (#3888).
  if ((await runIslandGate(files, audit, log)) !== 0) code = 1;
  return code === 0 ? 0 : 1;
}

// ------------------------------------------------------------------ the island gate (#3888, docs/architecture/placement-system.md "The gate")
// A finding on the island (the pack's MainWorld) is hard for a branch that changed what put it there: an island layer,
// a town kit or generator, a marker prefab or the spawn code. Today's findings live in a baseline that may only shrink,
// so the gate blocks NEW problems from day one while the cleanup cards burn the baseline down.

export const ISLAND_BASELINE = "tools/place/island-baseline.json";
const ISLAND_LAYER = /^addons\/LF_Everon1989\/Worlds\/MainWorld\/MainWorld_Layers\/([^/]+\.layer)$/;

// "layer" (audit just that layer), "whole" (audit the whole island with the runtime sweep) or null.
export function islandRelevant(file) {
  const f = String(file).split("\\").join("/");
  if (ISLAND_LAYER.test(f)) return "layer";
  if (/^tools\/town\/kits\//.test(f)) return "whole";
  if (/^tools\/town\/[^/]+\.mjs$/.test(f) && !/\.selftest\.mjs$/.test(f)) return "whole";
  if (/^addons\/[^/]+\/Prefabs\/.*Marker[^/]*\.et$/.test(f)) return "whole";
  if (/^addons\/[^/]+\/Scripts\/.*(?:\/LF_Utils\.c|Spawn[^/]*\.c)$/.test(f)) return "whole";
  return null;
}

// { hit, layers, whole }: the files that trigger, the island layer names, and whether the whole island must be audited.
export function islandScope(files) {
  const hit = [];
  const layers = new Set();
  let whole = false;
  for (const file of files) {
    const k = islandRelevant(file);
    if (!k) continue;
    hit.push(file);
    if (k === "whole") whole = true;
    else layers.add(ISLAND_LAYER.exec(String(file).split("\\").join("/"))[1]);
  }
  return { hit, layers: [...layers].sort(), whole };
}

// The `audit placement` arguments of the island step. A layer-only diff audits its layers; anything else the whole island
// (no --layer, so the audit adds the runtime sweep of what spawners put out).
export function islandAuditArgs(scope) {
  return ["--world", "MainWorld", "--baseline", "--top", "60", ...(scope.whole || !scope.layers.length ? [] : ["--layer", scope.layers.join(",")])];
}

// One key per (row, problem kind) and per world finding: stable across runs (no coordinates), so a baseline row stays
// matched while the prop it names keeps its problem.
export function findingKeys(ranked = [], world = []) {
  const out = [];
  for (const r of ranked) for (const p of r.problems ?? []) out.push({ key: `${r.layer}|${r.name}|${p.kind}`, text: `${r.name} [${r.layer}] @${r.x},${r.y},${r.z}: ${p.text}` });
  for (const w of world) out.push({ key: `world|${w.kind}|${w.text}`, text: w.text });
  return out;
}

export function readBaseline(text) {
  if (text == null) return null;
  const j = JSON.parse(text);
  return Array.isArray(j.rows) ? j.rows : [];
}

export function baselineText(rows) {
  return JSON.stringify({
    note: "Island placement findings that existed when the gate went on (#3888). May only shrink: `tools\\cli audit placement --world MainWorld --baseline-shrink` drops fixed rows; placement-gate-selftest fails a branch that adds one.",
    rows: [...new Set(rows)].sort(),
  }, null, 2) + "\n";
}

// The verdict: a finding in the baseline warns, any other fails. { fail, lines, hard, warn }.
export function judgeIsland(ranked, world, baseline) {
  const known = new Set(baseline ?? []);
  const hard = [];
  const warn = [];
  for (const f of findingKeys(ranked, world)) (known.has(f.key) ? warn : hard).push(f);
  const lines = [];
  if (warn.length) lines.push(`island audit: ${warn.length} finding(s) already in ${ISLAND_BASELINE}: warnings (the cleanup cards burn them down)`);
  for (const f of warn.slice(0, 20)) lines.push(`WARN  ${f.text}`);
  if (warn.length > 20) lines.push(`WARN  ... ${warn.length - 20} more baseline finding(s)`);
  for (const f of hard) lines.push(`FAIL  ${f.text}`);
  lines.push(hard.length ? `island audit: ${hard.length} NEW finding(s) not in the baseline; fix them (${allowHint()})` : "island audit: no new finding");
  return { fail: hard.length > 0, lines, hard, warn };
}

// The baseline may only shrink: the keys `after` holds that `before` did not.
export function baselineGrowth(before, after) {
  const old = new Set(before ?? []);
  return [...new Set(after ?? [])].filter((k) => !old.has(k)).sort();
}

// Drop the rows a full island run no longer finds; never add one.
export function shrinkBaseline(before, current) {
  const now = new Set(current);
  return [...new Set(before ?? [])].filter((k) => now.has(k)).sort();
}

// The island step of `wt ship` and `ci`: `audit(args)` runs `audit placement <args>` and returns its exit code.
export async function runIslandGate(files, audit, log = console.log) {
  const scope = islandScope(files);
  if (!scope.hit.length) {
    log("=== placement: island audit skipped (no island layer, town kit or generator, marker prefab or spawn code changed) ===");
    return 0;
  }
  const args = islandAuditArgs(scope);
  log(`=== placement: island audit (${scope.hit.length} file(s), e.g. ${scope.hit[0]}; ${args.includes("--layer") ? `layers ${scope.layers.join(", ")}` : "the whole island with the runtime sweep"}) ===`);
  const code = await audit(args);
  if (code !== 0) log("island placement audit FAILED: a new finding above, not in the baseline. See docs/placement-audit.md \"The island gate\"");
  return code === 0 ? 0 : 1;
}

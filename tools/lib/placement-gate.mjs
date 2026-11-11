// The placement step of `wt gate` / `wt ship` / `ci`: run `audit placement --strict` when the branch
// moved or added world props. docs/placement-audit.md; the trigger is proven offline by
// tools/validation/placement-gate-selftest.mjs.

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

export function allowHint() {
  return `fix the placement, or if it is meant to look that way add { "match": "<regex on name or prefab>", "kind": "<problem>", "why": "<reason a reviewer can check in a shot>" } to ${ALLOW_PATH}`;
}

// `audit(args)` runs the audit and returns an exit code (place-audit.mjs main); injected so the selftest runs offline.
export async function runPlacementGate(files, audit, log = console.log) {
  const hit = placementTrigger(files);
  if (!hit.length) {
    log("=== ci: placement audit skipped (no .layer, Prefabs .et or LF_Utils/spawn code changed) ===");
    return 0;
  }
  log(`=== ci: placement audit --strict (${hit.length} file(s) touch world props, e.g. ${hit[0]}) ===`);
  const code = await audit(["--strict", "--top", "200"]);
  if (code !== 0) log(`placement audit FAILED: the entities above are flagged; ${allowHint()}. See docs/placement-audit.md`);
  else log("placement audit: clean under --strict");
  return code === 0 ? 0 : 1;
}

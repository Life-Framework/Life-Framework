// The pre-move save step of `wt gate` / `wt ship` / `ci` (#327, decision D7 (a)): when a change can
// break loading a save written before the pack move, run `e2e --pre-move <ref> --mission playtest`:
// pass 1 writes the save on the pinned commit (core only, the playtest header the host used), pass 2
// reloads it on this tree with both addons, and `e2e/pass2-verify-load` checks the marker values.
// The trigger is proven offline by tools/validation/pre-move-gate-selftest.mjs; docs/persistence.md
// "Pre-move save gate".

// 502ebf39 (PR #1144): the last commit before the first move batch of #323. Everything the pack
// later received still lived in the core there, so a save from it has the old prefab paths. The ref
// is pinned, never "the previous release", so the guarantee does not slide forward with main.
export const PRE_MOVE_REF = "502ebf39";
export const PRE_MOVE_MISSION = "playtest";

function norm(file) {
  return String(file).split("\\").join("/");
}

// A changed file that can change what a pre-move save does on load: persistence code or its
// serializers, a mission header, a prefab-path or GUID resolver, the pack/core boundary, or the
// proof itself.
export function preMoveRelevant(file) {
  const f = norm(file);
  if (f === "tools/lib/pre-move-gate.mjs" || f === "tools/test/e2e-shutdown.mjs") return true;
  if (f === "tools/lib/addons.mjs" || f === "tools/validation/validate-addon-boundary.mjs") return true;
  if (!f.startsWith("addons/")) return false;
  if (/\/Scripts\/Game\/Persistence\//.test(f)) return true;
  if (/\/Scripts\/Game\/Tests\/LF_Test_E2E[^/]*\.c$/.test(f)) return true;
  if (/\/Scripts\/.*(?:Persist|SaveAlias|SaveIds)[^/]*\.c$/.test(f)) return true;
  if (/\/Missions\//.test(f)) return true;
  if (f.endsWith(".gproj")) return true;
  return false;
}

export function preMoveTrigger(files) {
  return files.filter(preMoveRelevant);
}

// `e2e(opts)` runs the two-boot proof and returns an exit code; injected so the selftest runs offline.
export async function runPreMoveGate(files, e2e, log = console.log) {
  const hit = preMoveTrigger(files);
  if (!hit.length) {
    log("=== ci: pre-move save skipped (no persistence code, mission header, addon boundary or .gproj changed) ===");
    return 0;
  }
  log(`=== ci: pre-move save, e2e --pre-move ${PRE_MOVE_REF} --mission ${PRE_MOVE_MISSION} (${hit.length} file(s) touch saves or the pack boundary, e.g. ${hit[0]}) ===`);
  const code = await e2e({ preMove: PRE_MOVE_REF, mission: PRE_MOVE_MISSION });
  if (code !== 0) log(`pre-move save FAILED: a save made on ${PRE_MOVE_REF} no longer loads whole; tools\\cli e2e --pre-move ${PRE_MOVE_REF} --mission ${PRE_MOVE_MISSION} reruns it (docs/persistence.md "Pre-move save gate")`);
  else log(`pre-move save: the ${PRE_MOVE_REF} save loads and its marker values survive`);
  return code === 0 ? 0 : 1;
}

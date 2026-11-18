// tools/validation/always-inputs-selftest.mjs
// inputs: tools/**, agent-box/**
//
// Proves the real headers of the checks that used to run on every validate (#3115, efficiency pass 3): each
// declares its inputs, a branch that changes only data (feature docs, PR pictures, dialogue lines) skips all of
// them against origin/main, and a branch that changes a file one of them reads runs that check. It reads the
// real tools/validation/ headers and plans them with the real planner; nothing is mocked.
//
// red-proof: 2026-11-18, with the `inputs:` header of validate-world-zones.mjs put back to `always` the run printed
// `FAIL validate-world-zones.mjs must declare inputs` and `FAIL a data-only branch must skip validate-world-zones.mjs`;
// with the header of core-place-names-selftest.mjs set to `imports` but plan() ignoring the import closure it printed
// `FAIL a change to a module core-place-names-selftest.mjs imports must run it`; restored, it passes.
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { importClosure } from "../lib/import-closure.mjs";
import { parseInputs, plan, snapshotFiles } from "../lib/validate-incremental.mjs";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const dir = join(root, "tools", "validation");
const failures = [];
const check = (ok, msg) => {
  if (!ok) failures.push(msg);
};

// The 42 checks that had `inputs: always` or no header before #3115, split by what became of them.
const GLOBAL = ["validate-addon-source-isolation.ps1", "validate-drivers-include.mjs", "validate-rdb-fresh.mjs", "validate-repo.ps1", "validate-scripts.ps1"];
const DECLARED = [
  "asset-assetserver-selftest.mjs", "blender-bridge-selftest.mjs", "building-pipeline-selftest.mjs", "building-review-shots-selftest.mjs",
  "config-harness-selftest.mjs", "core-place-names-selftest.mjs", "credits-selftest.mjs", "dialogue-patch-selftest.mjs",
  "e2e-cash-swap-selftest.mjs", "e2e-content-ids-selftest.mjs", "flagship-config-selftest.mjs", "input-listeners-selftest.mjs",
  "install-api-selftest.mjs", "keybinding-menu-selftest.mjs", "map-poi-visibility-selftest.mjs", "meta-name-path-selftest.mjs",
  "pack-manifest-selftest.mjs", "pack-race-course-selftest.mjs", "prefab-compare-selftest.mjs", "server-template-selftest.mjs",
  "terms-render-diff.mjs", "terms-selftest.mjs", "ui-back-path-selftest.mjs", "validate-addonsdir.mjs", "validate-dialogue-patch.mjs",
  "validate-docs-sync.ps1", "validate-everon-config.mjs", "validate-guid-paths.mjs", "validate-input-vanilla-clash-selftest.mjs",
  "validate-island-zones.mjs", "validate-item-catalogue.mjs", "validate-map-independence.mjs", "validate-pack-manifest.mjs",
  "validate-systems-list.mjs", "validate-world-zones.mjs", "wb-lock-selftest.mjs", "website-content-selftest.mjs",
];
const headerOf = (f) => {
  try {
    return parseInputs(readFileSync(join(dir, f), "utf8"));
  } catch {
    failures.push(f + " is gone: update the lists in always-inputs-selftest.mjs");
    return undefined;
  }
};

for (const f of DECLARED) check(headerOf(f) !== null, `${f} must declare inputs (an \`inputs:\` line in its first 30 lines)`);
for (const f of GLOBAL) {
  check(headerOf(f) === null, `${f} is meant to stay global (inputs: always)`);
  check(/^\s*(?:\/\/|#)\s*\(global: .+\)/m.test(readFileSync(join(dir, f), "utf8").split(/\r?\n/).slice(0, 30).join("\n")), `${f} must say why it stays global on the line after its header`);
}
// The real planner on the real repo, nothing green yet: what a branch that changed `changed` runs.
const files = snapshotFiles(root);
// Planning one check at a time: fingerprinting every file for all 389 checks takes a second a call.
const planFor = (changed, only = DECLARED.concat(GLOBAL)) => plan(root, dir, only, { checks: {} }, files, changed);
const skipsBase = (p, f) => !!p.get(f)?.unchanged && !!p.get(f)?.base;

// 1. A data-only branch (a feature doc, a PR picture, a dialogue line) skips every declared check.
const dataOnly = planFor(["docs/features/bank.md", "website/public/docs-images/3115/after.png", "tools/assets/dialogue/line.json"]);
// (validate-docs-sync.ps1 is the one exception: its header names docs/features/** because a feature doc is what
// answers its warning, so it still runs on a branch that edits one; it is a one-second warn-only check.)
for (const f of DECLARED) if (f !== "validate-docs-sync.ps1") check(skipsBase(dataOnly, f), `a data-only branch must skip ${f}`);
for (const f of GLOBAL) check(!dataOnly.get(f)?.unchanged, `${f} must run on every branch`);

// 2. A branch that changes something a check reads runs it. For each declared check, the path its first data glob
// names (or, for an `imports` check, the first module it imports outside tools/validation).
const sample = (glob) => glob.replace(/\*\*\//g, "x/").replace(/\*\*/g, "x").replace(/\*/g, "x");
for (const f of DECLARED) {
  const declared = headerOf(f);
  if (!declared) continue; // already reported above: it must declare inputs
  const globs = declared.filter((g) => g !== "imports");
  let path = globs.length ? sample(globs[0]) : null;
  if (!path) {
    const rel = `tools/validation/${f}`;
    path = importClosure(root, rel).find((x) => x !== rel && !x.startsWith("tools/validation/"));
    if (!path) path = importClosure(root, rel).find((x) => x !== rel); // a validator the selftest proves
  }
  check(!!path, `${f}: nothing to sample as an input`);
  if (path) check(!planFor([path], [f]).get(f)?.unchanged, `a change to ${path} must run ${f}`);
}
// An `imports` check runs when a module it imports changes (the closure is implicit, so this holds for any header).
const placeSelftest = "core-place-names-selftest.mjs";
const placeModule = importClosure(root, `tools/validation/${placeSelftest}`).find((x) => x.endsWith("validate-core-place-names.mjs"));
check(!!placeModule && !planFor([placeModule], [placeSelftest]).get(placeSelftest)?.unchanged, `a change to a module ${placeSelftest} imports must run it`);

// 3. Targeted cases by hand, so a header that matches too much or too little is caught by name.
const runs = (changed, f) => !planFor(changed, [f]).get(f)?.unchanged;
const feature = "addons/LifeFramework/Scripts/Game/Feature/Bank/LF_Bank.c";
for (const f of ["validate-map-independence.mjs", "validate-guid-paths.mjs", "validate-systems-list.mjs", "validate-pack-manifest.mjs", "validate-dialogue-patch.mjs", "validate-docs-sync.ps1"]) {
  check(runs([feature], f), `a feature script change must run ${f}`);
}
for (const f of ["flagship-config-selftest.mjs", "config-harness-selftest.mjs", "validate-world-zones.mjs", "validate-island-zones.mjs", "keybinding-menu-selftest.mjs", "terms-render-diff.mjs"]) {
  check(!runs([feature], f), `a feature script change must not run ${f}`);
}
check(runs(["addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers/Place_x_y.layer"], "validate-island-zones.mjs"), "an island layer change must run validate-island-zones.mjs");
check(!runs(["addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers/Place_x_y.layer"], "validate-item-catalogue.mjs"), "an island layer change must not run validate-item-catalogue.mjs");
check(runs(["tools/grid/cells/bank.json"], "validate-world-zones.mjs"), "a grid claim change must run validate-world-zones.mjs");
check(runs(["addons/LifeFramework/Worlds/DebugWorld/DebugWorld_Layers/Annex.layer"], "validate-world-zones.mjs"), "a DebugWorld layer change must run validate-world-zones.mjs");
check(!runs(["addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers/Place_x_y.layer"], "validate-world-zones.mjs"), "an island layer change must not run validate-world-zones.mjs (it reads the grid and DebugWorld only)");
check(runs(["addons/LifeFramework/Prefabs/Items/Pistol.et"], "validate-item-catalogue.mjs"), "a prefab change must run validate-item-catalogue.mjs");
check(runs(["tools/validation/map-independence-baseline.json"], "validate-map-independence.mjs"), "its baseline must run validate-map-independence.mjs");
check(runs(["server/flagship/config.json"], "flagship-config-selftest.mjs"), "a flagship config change must run flagship-config-selftest.mjs");
check(runs(["addons/LifeFramework/Configs/System/keyBindingMenu.conf"], "keybinding-menu-selftest.mjs"), "the key binding menu must run keybinding-menu-selftest.mjs");
check(runs(["tools/lib/scratch.mjs"], "dialogue-patch-selftest.mjs"), "tools/lib/scratch.mjs must run the selftests that import it");
check(!runs(["tools/lib/scratch.mjs"], "core-place-names-selftest.mjs"), "tools/lib/scratch.mjs must not run a selftest that does not import it");
check(!runs(["tools/pool.json"], "core-place-names-selftest.mjs"), "an unrelated tools file must not run an imports check");

if (failures.length) {
  for (const f of failures) console.log(`FAIL ${f}`);
  process.exit(1);
}
console.log(`always-inputs-selftest: OK (${DECLARED.length} checks declare inputs, ${GLOBAL.length} stay global; a data-only branch skips all ${DECLARED.length}; each runs when an input changes)`);

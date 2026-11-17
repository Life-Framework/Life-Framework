// Proves the maintainer's 2026-11-15 rule: layout findings on TOWN entities warn on every branch and never block;
// the same findings in the debug grid (or any non-town layer a PR adds or moves) still block.
// inputs: tools/lib/town-findings.mjs, tools/lib/world-checks.mjs, tools/lib/placement-gate.mjs, tools/validation/validate-prop-on-surface.mjs, agent-box/place-audit.mjs, tools/cli.mjs
// red-proof: 2026-11-15, run before town-findings.mjs gained judgeStrict/splitSurfaceFindings and world-checks gained
//   TOWN_LAYOUT_CHECKS: every case below printed FAIL (a pre-existing Street_ChopShop vice failed validate-prop-on-surface
//   and `audit placement --strict` on every branch, #1867); with the fix it passes.
import { judgeChecks, TOWN_LAYOUT_CHECKS, LAYOUT_CHECKS } from "../lib/world-checks.mjs";
import * as tf from "../lib/town-findings.mjs";
import { placementTrigger, runPlacementGate } from "../lib/placement-gate.mjs";

const fails = [];
const check = (ok, msg) => { if (!ok) fails.push(msg); };

const TOWN = "addons/LifeFramework/Worlds/DebugWorld/DebugWorld_Layers/Street_ChopShop.layer";
const PACK = "addons/LF_Everon1989/Worlds/Everon1989Town/Everon1989Town_Layers/Village_Dressing.layer";
const GRID = "addons/LifeFramework/Worlds/DebugWorld/DebugWorld_Layers/Grid_police_gates.layer";
const LEGACY = "addons/LifeFramework/Worlds/DebugWorld/DebugWorld_Layers/Tests.layer";

// ---- prop-on-surface: a pre-existing town finding warns, a grid finding blocks.
const vice = { file: "Worlds/DebugWorld/DebugWorld_Layers/Street_ChopShop.layer", prop: "Workbench_Vice_01", surface: "Roadmap_ChopShop", over: 0.2, drop: 0.41 };
const gridProp = { file: "Worlds/DebugWorld/DebugWorld_Layers/Grid_police_gates.layer", prop: "Till_01", surface: "Counter", over: 0.3, drop: 0 };
const prefabProp = { file: "Prefabs/Props/Stand.et", prop: "Cup", surface: "Table", over: 0.3, drop: 0 };
let s = tf.splitSurfaceFindings([vice, gridProp, prefabProp]);
check(s.warn.length === 1 && s.warn[0] === vice, `the town vice must warn, got ${JSON.stringify(s.warn.map((f) => f.prop))}`);
check(s.hard.length === 2 && s.hard.includes(gridProp) && s.hard.includes(prefabProp), "a grid finding and a prefab finding must still block");
s = tf.splitSurfaceFindings([vice]);
check(s.hard.length === 0, "a town-only finding must not block");
check(tf.isTownFile(PACK) && tf.isTownFile(TOWN), "town and pack town layers are town");
check(!tf.isTownFile(GRID) && !tf.isTownFile(LEGACY), "Grid_* and legacy annex layers are not town");

// ---- placement audit --strict: town rows warn, a grid row in a layer the PR changed blocks.
const row = (layer, name = "X") => ({ layer, name, score: 5, problems: [{ kind: "float", text: "floats" }] });
const ranked = [row("Street_ChopShop.layer", "Workbench_Vice_01"), row("Grid_police_gates.layer", "Crate"), row("Tests.layer", "Old")];
let j = tf.splitAuditFindings(ranked, [], { hardLayers: ["Grid_police_gates.layer"] });
check(j.hardRows.length === 1 && j.hardRows[0].name === "Crate", `only the PR's own grid layer blocks, got ${JSON.stringify(j.hardRows.map((r) => r.name))}`);
check(j.warnRows.length === 2, "the town row and an untouched grid layer warn");
j = tf.splitAuditFindings(ranked, [], { hardLayers: null });
check(j.hardRows.length === 2 && !j.hardRows.some((r) => r.layer === "Street_ChopShop.layer"), "with no layer list every non-town layer blocks, the town never");
j = tf.splitAuditFindings([row("Street_ChopShop.layer")], [{ kind: "duplicate", text: "duplicate: A [Street_ChopShop.layer] sits on B [Street_ChopShop.layer] (x)" }, { kind: "budget", text: "budget: cell 1,2 holds 99" }], { hardLayers: [] });
check(j.hardRows.length === 0 && j.hardWorld.length === 0, "town-only world findings warn");
j = tf.splitAuditFindings([], [{ kind: "duplicate", text: "duplicate: A [Grid_police_gates.layer] sits on B [Grid_police_gates.layer] (x)" }], { hardLayers: ["Grid_police_gates.layer"] });
check(j.hardWorld.length === 1, "a duplicate in the PR's own grid layer blocks");

// judgeStrict: the verdict the audit's exit code follows.
check(typeof tf.judgeStrict === "function", "judgeStrict exists");
if (typeof tf.judgeStrict === "function") {
  let v = tf.judgeStrict([row("Street_ChopShop.layer", "Workbench_Vice_01")], [], { hardLayers: [] });
  check(v.fail === false && v.lines.some((l) => /WARN/.test(l) && /Workbench_Vice_01/.test(l)), "a pre-existing town finding warns and exits 0 on a normal branch");
  v = tf.judgeStrict([row("Grid_police_gates.layer", "Crate")], [], { hardLayers: ["Grid_police_gates.layer"] });
  check(v.fail === true, "a grid finding in the PR's own layer still blocks");
}

// ---- the gate hands the audit the PR's own non-town layers.
check(tf.hardLayersOf([TOWN, PACK, GRID, "docs/a.md", "addons/LifeFramework/Prefabs/A.et"]).join() === "Grid_police_gates.layer", "hardLayersOf keeps only the non-town layers of the diff");
let args = null;
await runPlacementGate([TOWN, GRID], async (a) => { args = a; return 0; }, () => {});
check(args && args.includes("--strict") && args.includes("--hard-layer") && args.join(" ").includes("Grid_police_gates.layer") && args.includes("--annex"), `the gate must pass the grid layer as hard and measure the annex, got ${JSON.stringify(args)}`);
args = null;
await runPlacementGate([TOWN], async (a) => { args = a; return 0; }, () => {});
check(args && args.includes("--strict") && !args.includes("--hard-layer"), "a town-only diff has no hard layer");
check(placementTrigger([TOWN]).length === 1, "a town layer still triggers the audit (its findings warn)");

// ---- validate: the town-only layout checks warn on a normal branch; the rest stay hard.
check(Array.isArray(TOWN_LAYOUT_CHECKS) && TOWN_LAYOUT_CHECKS.every((c) => LAYOUT_CHECKS.includes(c)), "TOWN_LAYOUT_CHECKS is a subset of LAYOUT_CHECKS");
if (Array.isArray(TOWN_LAYOUT_CHECKS)) {
  const r = judgeChecks([{ s: "validate-town-manifest.mjs", status: 1 }, { s: "validate-road-ids.mjs", status: 1 }, { s: "validate-player-props.mjs", status: 1 }, { s: "validate-world-zones.mjs", status: 1 }, { s: "validate-annex-fixtures.mjs", status: 1 }], { demote: false });
  check(r.warned.length === 3 && r.failed.join() === "validate-world-zones.mjs,validate-annex-fixtures.mjs", `on a normal branch the town checks warn, zones and annex fixtures block; got ${JSON.stringify(r)}`);
}

if (fails.length) {
  console.log("town-warn-selftest: FAIL");
  for (const f of fails) console.log("  " + f);
  process.exit(1);
}
console.log("town-warn-selftest: OK (town findings warn on every branch; grid findings and the zone rule still block)");

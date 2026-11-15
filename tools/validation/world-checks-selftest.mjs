// Proves the world-edit check split (tools/lib/world-checks.mjs): a failing layout check is a warning that
// reaches the PR body, a failing hard check still blocks, and every named check exists.
// inputs: tools/lib/world-checks.mjs, tools/commands/world.mjs, tools/validation/world-checks-selftest.mjs
// red-proof: with LAYOUT_CHECKS made empty this printed "FAIL a failing layout check warns, it does not block" and
//   exited 1 (2026-11-15); restored, it passes.
// red-proof: with LAYER_READERS_KEPT_HARD empty this printed one "FAIL validate-sound-slots.mjs reads .layer files but is
//   neither in HARD_CHECKS..." line per unclassified layer-reading check and exited 1 (2026-11-15); filled, it passes.
import { existsSync, readdirSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { HARD_CHECKS, LAYOUT_CHECKS, LAYER_READERS_KEPT_HARD, MAINTAINER_BRANCH, judgeChecks, layoutWarnings, worldScripts } from "../lib/world-checks.mjs";
import { prBody } from "../commands/world.mjs";
import { diffFiles } from "../lib/world-diff.mjs";

const fails = [];
const check = (ok, msg) => { if (!ok) fails.push(msg); };
const here = dirname(fileURLToPath(import.meta.url));

for (const s of [...HARD_CHECKS, ...LAYOUT_CHECKS]) check(existsSync(join(here, s)), `${s} is named in world-checks.mjs but is not in tools/validation`);
check(LAYOUT_CHECKS.includes("validate-prop-on-surface.mjs"), "an overhanging prop is a layout call: warn only");
check(!HARD_CHECKS.some((s) => LAYOUT_CHECKS.includes(s)), "a check is both hard and layout");
check(worldScripts(["a.mjs", "validate-town-manifest.mjs", "validate-world-zones.mjs", "validate-tests.ps1"]).join() === "validate-town-manifest.mjs,validate-world-zones.mjs", "worldScripts keeps only the named checks");

// Every check that reads .layer files is classified: HARD, LAYOUT, or kept hard with a reason. A new layer-reading
// check that is none of them would block a maintainer's hand edit on a layout judgement (the 2026-11-15
// agent-box-selftest and prop-on-surface failures of a world ship).
const classified = new Set([...HARD_CHECKS, ...LAYOUT_CHECKS, ...Object.keys(LAYER_READERS_KEPT_HARD)]);
const readers = readdirSync(here).filter((f) => /^validate-.*\.(mjs|ps1)$/.test(f) && !f.includes("selftest") && /_Layers|worldLayerDirs|layerFile|townWorld|\.layer\b/.test(readFileSync(join(here, f), "utf8")));
for (const f of readers) check(classified.has(f), `${f} reads .layer files but is neither in HARD_CHECKS, LAYOUT_CHECKS nor LAYER_READERS_KEPT_HARD (tools/lib/world-checks.mjs)`);
for (const f of Object.keys(LAYER_READERS_KEPT_HARD)) check(existsSync(join(here, f)), `${f} is in LAYER_READERS_KEPT_HARD but is not in tools/validation`);

const results = [{ s: "validate-town-manifest.mjs", status: 1 }, { s: "validate-core-village-names.mjs", status: 1 }];
check(judgeChecks(results, { demote: true }).failed.length === 0 && judgeChecks(results, { demote: true }).warned.length === 2, "a failing layout check warns, it does not block");
check(judgeChecks(results, { demote: false }).failed.length === 2, "off a maintainer world edit a layout check still blocks");
const hard = [{ s: "validate-world-zones.mjs", status: 1 }, { s: "validate-repo.ps1", status: 1 }, { s: "validate-guid-paths.mjs", status: 1 }];
check(judgeChecks(hard, { demote: true }).failed.length === 3, "zones, duplicate GUID and unknown GUID checks still block on a world edit");
check(judgeChecks([{ s: "validate-town-manifest.mjs", status: 0 }, { s: "validate-world-zones.mjs", status: 1 }], { demote: true }).failed.join() === "validate-world-zones.mjs", "one hard failure blocks beside a warning");

const log = [
  "PASS  validate/validate-repo.ps1 (0.4 s)",
  "WARN  validate/validate-town-manifest.mjs (0.2 s) layout check on a maintainer world edit: a warning, listed in the PR body",
  "validate-town-manifest: FAIL",
  "  FAIL spawn: town_map sign is 11.186 m away (max 8)",
  "PASS  validate/validate-world-zones.mjs (0.1 s)",
  "slowest: validate-repo.ps1 0.4 s",
  "validate: 3 check(s) run, 0 failed (1.0 s)",
].join("\n");
const w = layoutWarnings(log);
check(w.length === 1 && w[0].check === "validate-town-manifest.mjs" && /town_map sign is 11\.186 m away/.test(w[0].text), `layoutWarnings reads the failing text, got ${JSON.stringify(w)}`);

const body = prBody(diffFiles([]), { slug: "town-maintainer-edits", warnings: ["validate/validate-town-manifest.mjs: spawn: town_map sign is 11.186 m away"] });
check(/## Layout warnings/.test(body) && /town_map sign is 11\.186/.test(body), "the PR body lists the layout warnings");
check(!/## Layout warnings/.test(prBody(diffFiles([]), { slug: "x" })), "no warnings, no section");
check(MAINTAINER_BRANCH.test("ws/town-maintainer-edits") && !MAINTAINER_BRANCH.test("ws/town-chapter"), "maintainer branch pattern");

if (fails.length) {
  for (const f of fails) console.log(`FAIL ${f}`);
  console.log(`world-checks-selftest: ${fails.length} case(s) failed`);
  process.exit(1);
}
console.log("world-checks-selftest: OK (layout checks warn, hard checks block, warnings reach the PR body)");

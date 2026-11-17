// tools/validation/map-areas-selftest.mjs
// inputs: tools/map/rules/areas.mjs, tools/map/rules/index.mjs, addons/LifeFramework/Configs/Map/Rules/areas.cases.json, addons/LifeFramework/Scripts/Game/Feature/Map/LF_MapAreaCases.generated.c
//
// Runs the shared map area case table through the node twin (the emulator's rule) with the emulator's
// own case runner. The Enforce twin runs the same table in LF_Test_MapAreas, so a drift in either fails
// one of them. Also proves the runner can fail: a table copy with a wrong vertex, a wrong style and a
// wrong rule must report all three.
// The game cannot read a loose .json from an addon (observed 2026-11-16), so LF_MapAreaCases.generated.c carries the
// same table as script for the Enforce test; this fails when it is stale: node tools/map/rules/areas.mjs --gen.
// red-proof: observed 2026-11-16: a copy of the table with the box yaw-90 vertices reversed, the SAFE
// fill alpha at 99 and the LAWLESS rule mapped to SAFE made runCases report exactly 3 failures.
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { evaluate, generatedIsCurrent } from "../map/rules/areas.mjs";
import { CASES_DIR, runCases } from "../map/rules/index.mjs";

let failed = false;
const fail = (m) => { console.log(`FAIL ${m}`); failed = true; };

const real = runCases("areas", evaluate);
if (real.missing) fail("areas.cases.json is missing");
for (const f of real.failures) fail(`${f.name}: expected ${JSON.stringify(f.expect)}, got ${JSON.stringify(f.got)}`);
if (!real.missing && real.pass < 20) fail(`only ${real.pass} case(s) ran`);

if (!generatedIsCurrent()) fail("LF_MapAreaCases.generated.c is stale: run node tools/map/rules/areas.mjs --gen");

const dir = mkdtempSync(join(tmpdir(), "lf-map-areas-"));
try {
  const bad = JSON.parse(readFileSync(join(CASES_DIR, "areas.cases.json"), "utf8"));
  bad.cases.find((c) => c.input.shape === "box" && c.input.yaw === 90).expect.outline = "95 180 95 220 105 220 105 180";
  bad.cases.find((c) => c.input.op === "style" && c.input.kind === "SAFE").expect.fillAlpha = 99;
  bad.cases.find((c) => c.input.op === "rule" && c.input.rule === "LAWLESS").expect.kind = "SAFE";
  writeFileSync(join(dir, "areas.cases.json"), JSON.stringify(bad));
  const caught = runCases("areas", evaluate, { dir });
  if (caught.fail !== 3) fail(`a table with three wrong rows reported ${caught.fail} failure(s), expected 3`);
} finally {
  rmSync(dir, { recursive: true, force: true });
}

if (failed) process.exit(1);
console.log(`SUMMARY map-areas-selftest ok ${real.pass} cases`);

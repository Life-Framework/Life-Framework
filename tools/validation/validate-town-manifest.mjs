// Town manifest (agent-box/town.mjs) against the DebugWorld layers: every M1
// inputs: agent-box/**
// site resolves to exactly one placed entity (or its prefab placements) that
// carries the expected actions, and sits within 30 m of another site. Signs
// are warnings until the manifest sets requireSigns (M1-09), then failures.
// Positions live only in the layers, so moving a site (M1-25) is checked here.
// red-proof: with Roadmap_Job_Depot renamed in the manifest this printed
// "FAIL post_office: Roadmap_Job_Depot_X is not in any DebugWorld layer" and exited 1.
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { TOWN, checkTown } from "../../agent-box/town.mjs";

const root = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const result = checkTown(root);
if (!result.ok) {
  console.log("validate-town-manifest: FAIL");
  for (const error of result.errors) console.log(`  FAIL ${error}`);
  process.exit(1);
}
const pending = result.rows.filter((row) => row.signIssues?.length).length;
console.log(
  `validate-town-manifest: OK (${result.rows.length} sites resolve; signs pending on ${pending}; requireSigns=${TOWN.requireSigns})`,
);

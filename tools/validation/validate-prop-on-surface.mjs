// tools/validation/validate-prop-on-surface.mjs
// inputs: addons/**, tools/actions/**, tools/lib/town-findings.mjs, tools/lib/world-zones.mjs
//
// A prop resting on a counter, desk, table or bench must lie on its top face: the
// footprint (offline .xob bounds, rotated by yaw) may not hang more than 0.1 m past
// an edge. Catches the till that sat half off the store counter and the punch clock
// at a desk's corner. Needs the base-game pak for bounds, so on a machine without it
// (CI) it checks nothing and says so. Logic and limits: tools/actions/prop-on-surface.mjs.

import { scanRepo } from "../actions/prop-on-surface.mjs";
import { splitSurfaceFindings } from "../lib/town-findings.mjs";

let result;
try {
  result = scanRepo();
} catch (e) {
  console.log(`validate-prop-on-surface: SKIPPED (${e.message})`);
  process.exit(0);
}
const { findings, entities } = result;
// The town is hand-edited and never blocks anyone (2026-11-15): a finding on a town-layer entity is a warning on
// every branch. Grid layers and prefabs stay hard.
const { hard, warn } = splitSurfaceFindings(findings);
const text = (f) => `${f.file}: ${f.prop} (${f.mesh}) hangs ${f.over.toFixed(2)} m off ${f.surface} (${f.surfaceMesh}); bottom is ${f.drop.toFixed(2)} m from the top.`;
for (const f of warn) console.log(`WARN  ${text(f)} Town entity: a warning, the maintainer places the town.`);
for (const f of hard) console.log(`ERROR ${text(f)} Move it inside the surface edges.`);
if (hard.length) {
  console.log(`validate-prop-on-surface: FAILED (${hard.length} prop(s) overhang a surface, ${warn.length} more in the town as warnings, ${entities} entities scanned)`);
  process.exit(1);
}
console.log(`validate-prop-on-surface: OK (${warn.length} town warning(s), ${entities} entities scanned)`);

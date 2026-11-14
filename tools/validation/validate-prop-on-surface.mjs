// tools/validation/validate-prop-on-surface.mjs
// inputs: addons/**, tools/actions/**
//
// A prop resting on a counter, desk, table or bench must lie on its top face: the
// footprint (offline .xob bounds, rotated by yaw) may not hang more than 0.1 m past
// an edge. Catches the till that sat half off the store counter and the punch clock
// at a desk's corner. Needs the base-game pak for bounds, so on a machine without it
// (CI) it checks nothing and says so. Logic and limits: tools/actions/prop-on-surface.mjs.

import { scanRepo } from "../actions/prop-on-surface.mjs";

let result;
try {
  result = scanRepo();
} catch (e) {
  console.log(`validate-prop-on-surface: SKIPPED (${e.message})`);
  process.exit(0);
}
const { findings, entities } = result;
for (const f of findings) {
  console.log(`ERROR ${f.file}: ${f.prop} (${f.mesh}) hangs ${f.over.toFixed(2)} m off ${f.surface} (${f.surfaceMesh}); bottom is ${f.drop.toFixed(2)} m from the top. Move it inside the surface edges.`);
}
if (findings.length) {
  console.log(`validate-prop-on-surface: FAILED (${findings.length} prop(s) overhang a surface, ${entities} entities scanned)`);
  process.exit(1);
}
console.log(`validate-prop-on-surface: OK (${entities} entities scanned)`);

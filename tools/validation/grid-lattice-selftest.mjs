// tools/validation/grid-lattice-selftest.mjs
// inputs: tools/grid/lattice.mjs, tools/validation/grid-lattice-selftest.mjs
//
// Proves the grid frame (tools/grid/lattice.mjs framePlan) lays no gravel path on or inside a claim that spans several cells
// (the pub cafe in c17r0 and the hostel had path in the building), while every lattice cell still has its marker and plate
// (the in-game test grid/frame-labels wants one per cell; a span's inner cells are marked on their east edge, #2278) and a
// one-cell claim keeps its path.
// red-proof: with the path's inSpan test removed from framePlan this printed "FAIL a gravel path stands on or inside the span
// claim: GridPath_r0_86, ..." and exited 1 (2026-11-17). A first version also skipped the markers inside span claims; it removed
// 12 markers and the main guard's grid/frame-labels failed twice (#2379), so the markers stay.

import { allCells, cellBounds, cellStates, framePlan, spanRects } from "../grid/lattice.mjs";

let failed = 0;
const ok = (cond, what) => {
  if (!cond) {
    failed++;
    console.log(`FAIL ${what}`);
  }
};

// A 2 x 2 claim from c17r0 and a one-cell claim at c3r1.
const claims = [
  { slug: "pubcafe", cell: "c17r0", span: [2, 2] },
  { slug: "shed", cell: "c3r1" },
];
const states = cellStates(claims);
const rects = spanRects(states);
ok(rects.length === 1, `one span rectangle expected, got ${rects.length}`);
const a = cellBounds(17, 0);
const b = cellBounds(18, 1);
const rect = { x0: a.x0, z0: a.z0, x1: b.x1, z1: b.z1 };
ok(rects[0] && rects[0].x0 === rect.x0 && rects[0].z0 === rect.z0 && rects[0].x1 === rect.x1 && rects[0].z1 === rect.z1, `span rectangle is ${JSON.stringify(rects[0])}, want ${JSON.stringify(rect)}`);

const plan = framePlan(states);
const names = new Set(plan.map((e) => e.name));
const paths = plan.filter((e) => e.name.startsWith("GridPath_"));
const inside = paths.filter((e) => e.x >= rect.x0 && e.x <= rect.x1 && e.z >= rect.z0 && e.z <= rect.z1);
ok(inside.length === 0, `a gravel path stands on or inside the span claim: ${inside.map((e) => e.name).join(", ")}`);
ok(paths.some((e) => e.name.startsWith("GridPath_r0_")), "the path along row 0 was dropped altogether");

// Every cell keeps its marker and plate (the in-game test grid/frame-labels wants one per cell): a span's inner cells are marked
// on their east edge, so no marker stands in the claim's west half.
for (const id of ["c17r0", "c18r0", "c17r1", "c18r1", "c3r1", "c16r0", "c19r0", "c17r2", "c0r0"]) ok(names.has(`GridMarker_${id}`) && names.has(`GridPlate_${id}`), `cell ${id} lost its marker or plate`);
ok(allCells().every((c) => names.has(`GridMarker_${c.id}`) && names.has(`GridPlate_${c.id}`)), "a lattice cell has no marker and plate");
ok(names.has("GridMapBoard"), "the map board is gone");
// The path still runs past the span, east of its rectangle.
ok(plan.some((e) => e.name.startsWith("GridPath_r0_") && e.x > rect.x1 + 1), "the path along row 0 does not continue past the span");

console.log(failed ? `grid-lattice-selftest: ${failed} failure(s)` : "grid-lattice-selftest: OK (no path in a span claim, a marker and plate in every cell)");
process.exit(failed ? 1 : 0);

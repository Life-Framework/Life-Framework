// tools/validation/grid-lattice-selftest.mjs
// inputs: tools/grid/lattice.mjs, tools/validation/grid-lattice-selftest.mjs
//
// Proves the grid frame (tools/grid/lattice.mjs framePlan) leaves a claim that spans several cells alone: no pole,
// plate or gravel path stands on or inside its rectangle (the pub cafe in c17r0 and the hostel had poles and a path
// rendering in the building), while a one-cell claim, a free cell and the cells around the span keep theirs.
// red-proof: with the inSpan tests removed from framePlan this printed "FAIL a frame entity stands inside the span claim"
// (the poles of c17r0, c18r0, c17r1 and the path on its south edge) and exited 1 (2026-11-17).

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
const inside = plan.filter((e) => e.x !== undefined && e.name !== "GridMapBoard" && e.x >= rect.x0 && e.x <= rect.x1 && e.z >= rect.z0 && e.z <= rect.z1);
ok(inside.length === 0, `a frame entity stands inside the span claim: ${inside.map((e) => e.name).join(", ")}`);
for (const id of ["c17r0", "c18r0", "c17r1", "c18r1"]) ok(!names.has(`GridMarker_${id}`) && !names.has(`GridPlate_${id}`), `the span's cell ${id} kept a pole`);
ok(plan.some((e) => e.name.startsWith("GridPath_r0_")), "the path along row 0 was dropped altogether");

// What is not inside the span stays: a one-cell claim's and free cells' poles, the neighbours of the span, the board.
for (const id of ["c3r1", "c16r0", "c19r0", "c17r2", "c0r0"]) ok(names.has(`GridMarker_${id}`) && names.has(`GridPlate_${id}`), `cell ${id} lost its pole`);
ok(names.has("GridMapBoard"), "the map board is gone");
// The path still runs past the span, east of its rectangle.
ok(plan.some((e) => e.name.startsWith("GridPath_r0_") && e.x > rect.x1 + 1), "the path along row 0 does not continue past the span");
ok(allCells().length - 4 === states.filter((s) => !(s.col >= 17 && s.col <= 18 && s.row <= 1)).length, "the states of the cells outside the span changed");

console.log(failed ? `grid-lattice-selftest: ${failed} failure(s)` : "grid-lattice-selftest: OK (no pole or path inside a span claim)");
process.exit(failed ? 1 : 0);

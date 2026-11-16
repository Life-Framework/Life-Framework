// tools/validation/grid-world-selftest.mjs
// inputs: tools/validation/validate-grid-world.mjs, tools/validation/validate-world-zones.mjs, tools/lib/world-zones.mjs, tools/grid/**
//
// Proves validate-grid-world's rule on fixtures: a legacy station outside every claimed cell is
// reported, one inside a claimed cell passes, a pending layer, a Grid_ layer, a system layer and the
// annex fence are skipped.
// red-proof: with the cell test in stationsOutsideCells made always true this printed
// "FAIL a station outside every claimed cell is reported" and exited 1 (2026-11-16); restored, it passes.

import { stationsOutsideCells } from "./validate-grid-world.mjs";

let failed = 0;
const ok = (cond, what) => {
  if (!cond) {
    failed++;
    console.log(`FAIL ${what}`);
  }
};

const station = (name, x, z) => `GenericEntity ${name} {\n coords ${x} 0 ${z}\n {\n }\n}\n`;
const fence = "SCR_DestructibleEntity Fence_1 : \"{57873EAF69D5450E}Prefabs/Structures/Walls/Net/NetFence_01/NetFence_01_4m.et\" {\n coords 20 0 112\n}\n";
const claims = [{ slug: "police", cell: "c1r5" }, { slug: "frame", cell: "lattice" }]; // c1r5 = x 34..46, z 82..94

const bad = stationsOutsideCells({ "Out.layer": station("OutStation", 30, 50) }, claims, []);
ok(bad.length === 1 && bad[0].name === "OutStation", "a station outside every claimed cell is reported");
ok(stationsOutsideCells({ "In.layer": station("InStation", 40, 88) }, claims, []).length === 0, "a station inside a claimed cell passes");
ok(stationsOutsideCells({ "Out.layer": station("OutStation", 30, 50) }, claims, ["Out"]).length === 0, "a pending layer is skipped");
ok(stationsOutsideCells({ "Grid_x.layer": station("G", 30, 50), "Gamemode.layer": station("S", 1, 1), "Annex.layer": fence }, claims, []).length === 0, "Grid_, system layers and the annex fence are skipped");

if (failed) process.exit(1);
console.log("grid-world-selftest: OK");

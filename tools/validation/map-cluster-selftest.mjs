// Selftest for the map POI cluster and min-zoom rules (#2232): the shared case table passes through the node
// twin, a deliberately broken table fails it, and the emulator-facing build() draws what the table says.
// The Enforce twin runs the same table in LF_Test_MapPlacesLayout.
// inputs: tools/map/**, addons/LifeFramework/Configs/Map/**
import { build, cellSize, evaluate, generatedIsCurrent, isSolo, loadCases, runCases, showAtZoom } from "../map/rules/cluster.mjs";

const fails = [];
const expect = (cond, msg) => { if (!cond) fails.push(msg); };

const table = loadCases();
const live = runCases(table);
expect(live.length === 0, `the case table must pass: ${live.join("; ")}`);
expect(generatedIsCurrent(), "the Enforce copy of the table is current: run node tools/map/rules/cluster.mjs --gen");
const ops = new Set(table.cases.map((c) => c.input.op));
for (const op of ["minzoom", "mode", "cluster"]) expect(ops.has(op), `the table must have a ${op} case`);
expect(table.cases.length >= 14, "the table must cover min zoom, the solo zoom, drops, edges and negative coordinates");

// Red: a table that expects two points in one cell to stay two singles must fail the rule.
const broken = structuredClone(table);
for (const c of broken.cases) if (c.name.startsWith("two points in one cell")) c.expect = "a,b,c";
expect(runCases(broken).length > 0, "a table whose cluster case expects singles must fail the rule");

// The rule.
expect(showAtZoom(3, 3) && !showAtZoom(2.9, 3), "min zoom is inclusive");
expect(isSolo(4) && !isSolo(3.5), "the solo zoom is 4");
expect(Math.abs(cellSize(3) - 200) < 1e-9, "at zoom 3 the cell is 200 m");
expect(evaluate({ op: "nope", zoom: 3 }).startsWith("unknown"), "an unknown op is not silently accepted");

// build(): a centroid and a count the layer draws the stamp with.
const g = build([{ id: "a", x: 10, z: 10, minZoom: 0 }, { id: "b", x: 30, z: 50, minZoom: 0 }], 3);
expect(g.length === 1 && g[0].ids.length === 2 && g[0].x === 20 && g[0].z === 30, "a cluster sits at the mean of its members");

// 400 POIs across the island: the layer's own pass stays cheap in node too (the in-game numbers are in everon-map.md).
const many = [];
for (let i = 0; i < 400; i++) many.push({ id: `p${i}`, x: (i * 7919) % 12800, z: (i * 104729) % 12800, minZoom: i % 3 });
const t0 = process.hrtime.bigint();
for (let i = 0; i < 100; i++) build(many, 3);
const perPass = Number(process.hrtime.bigint() - t0) / 1e6 / 100;
expect(perPass < 2, `400 points cluster in under 2 ms in node (${perPass.toFixed(3)} ms)`);

if (fails.length) {
  for (const f of fails) console.error(`FAIL ${f}`);
  process.exit(1);
}
console.log(`map-cluster-selftest: ok (400 points, ${perPass.toFixed(3)} ms a pass)`);

// tools/town/street-life.selftest.mjs - the street-life benches stand on probed, level ground (card #3880).
//
// Run: node tools/town/street-life.selftest.mjs
//   - benchGround refuses ground that spans over 13 cm under the seat, or that lies over 20 cm from the house floor (stairs);
//     it stands a bench on the highest of the probed points;
//   - benchPoints gives the middle, both seat ends, front and back of a bench along its facing;
//   - withoutBenches takes the bench entities out of a layer text and keeps every other one;
//   - every bench of the committed Saint-Philippe layer rests on the highest probed point of its footprint (within 2 cm), on
//     ground that spans at most 13 cm, and the pin file holds every one of its five points.
// red-proof: with the step test removed from benchGround, the stairs case printed FAIL (2026-11-21).
import { readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { benchGround, benchPoints, hitBlocks, nearFootprint, withoutBenches } from "./street-life.mjs";
import { pinKey } from "./terrain.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
let failed = 0;
const check = (cond, msg) => {
  if (!cond) {
    failed++;
    console.log(`FAIL ${msg}`);
  }
};

check(benchGround([14.9, 14.9, 14.92, 14.9, 14.9], 14.9).y === 14.92, "level ground: the bench stands on the highest probed point");
check(benchGround([14.9, 14.9, 14.5, 14.9, 14.9], 14.9).why, "a 40 cm drop under one end of the seat is refused");
check(benchGround([14.53, 14.53, 14.53, 14.53, 14.53], 14.9).why, "flat ground 37 cm under the house floor (the entrance stairs) is refused");

const pts = benchPoints(100, 200, 90);
check(pts.length === 5, "five points under a bench");
check(Math.abs(pts[0][0] - 100) < 0.001 && Math.abs(pts[0][1] - 200) < 0.001, "the first point is the middle");
check(Math.abs(pts[3][0] - 100.12) < 0.01 && Math.abs(pts[3][1] - 200) < 0.01, "facing east (yaw 90) the front point lies east of the middle");
check(Math.abs(pts[1][1] - 199.2) < 0.01, "an end of the seat lies 0.8 m along the wall (yaw 90: towards -z)");

check(hitBlocks(["terrain", "WoodPile_02_B", "terrain"]) === "WoodPile_02_B", "a point whose ground is a wood pile is refused");
check(hitBlocks(["terrain", "AsphaltPavement_01_Wide_4m_A", "ConcreteStairs_02_Wide"]) === null, "pavement, stairs and terrain are ground");
check(benchPoints(100, 200, 90, 0.5)[1][1] > 199.45 && benchPoints(100, 200, 90, 0.5)[1][1] < 199.55, "a flower box is probed along 0.5 m each way");

// A washing line never stands inside a neighbouring house: b50's first line stood in b44 (#3888).
const b44 = [[4401.43, 10742.32], [4381.23, 10741.97], [4381.57, 10722.68], [4401.77, 10723.03]];
check(nearFootprint(4394.74, 10735.6, b44, 0.5), "a point inside a footprint is refused");
check(nearFootprint(4394, 10742.5, b44, 0.5) && !nearFootprint(4394, 10744, b44, 0.5), "a point within 0.5 m of an edge is refused, 1.7 m off is clear");

const layer = "GenericEntity Fit_x_street_life_bench_b10 : \"{A}p.et\" {\n coords 1 2 3\n}\nSCR_DestructibleEntity Fit_x_street_life_bin_b10 : \"{B}q.et\" {\n coords 4 5 6\n}\nGenericEntity Fit_x_street_life_bench_red_b09 : \"{C}r.et\" {\n coords 7 8 9\n}\n";
const kept = withoutBenches(layer + "SCR_DestructibleEntity Fit_x_street_life_flowers_b09 : \"{D}s.et\" {\n coords 1 1 1\n}\n");
check(!kept.includes("flowers_b09"), "withoutBenches also drops the flower boxes the probe must not land on");
check(!kept.includes("bench") && kept.includes("bin_b10") && kept.includes("coords 4 5 6"), "withoutBenches drops the benches and keeps the bin");

const layerFile = join(ROOT, "addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers/Place_saint-philippe_street-life.layer");
const pins = JSON.parse(readFileSync(join(ROOT, "tools/town/terrain/saint-philippe-street-life.json"), "utf8")).points;
const blocks = readFileSync(layerFile, "utf8").split("\n}\n");
let benches = 0;
for (const b of blocks) {
  const head = /^\w+ (Fit_\w+_bench\w*_b\d+) /m.exec(b);
  if (!head) continue;
  benches++;
  const c = /coords (\S+) (\S+) (\S+)/.exec(b);
  const yaw = Number(/angleY (\S+)/.exec(b)?.[1] ?? 0);
  const [x, y, z] = [Number(c[1]), Number(c[2]), Number(c[3])];
  const hs = benchPoints(x, z, yaw).map((q) => pins[pinKey(q[0], q[1])]);
  check(hs.every(Number.isFinite), `${head[1]}: the pin file lacks a point under it (run town street-life --terrain)`);
  if (!hs.every(Number.isFinite)) continue;
  check(Math.max(...hs) - Math.min(...hs) <= 0.1301, `${head[1]}: the ground under it spans ${(Math.max(...hs) - Math.min(...hs)).toFixed(3)} m`);
  check(Math.abs(y - Math.max(...hs)) <= 0.02, `${head[1]}: stands at ${y}, the ground is ${Math.max(...hs)}`);
}
check(benches >= 3, `the committed layer holds ${benches} bench(es), expected at least 3`);

if (failed) {
  console.log(`street-life selftest: ${failed} check(s) FAILED`);
  process.exit(1);
}
console.log(`street-life selftest: OK (ground rules, five footprint points, bench stripping, ${benches} committed benches on probed ground)`);

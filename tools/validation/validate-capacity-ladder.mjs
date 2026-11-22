// The capacity table is a ladder (0.3.0 W16, #4189).
// inputs: addons/LifeFramework/Configs/Vehicles/LF_Capacity.conf, addons/LifeFramework/Prefabs/Vehicles/**/*.et, tools/validation/validate-capacity-ladder.mjs
//
// One table says what the body, packs, car, pickup, van and truck carry (native sacks + cargo bed units) and
// which vehicle class each chain's best loop needs. Fails when:
//   - the six classes are not on rungs 0 to 5 in order, or a rung carries no more than the one below it;
//   - a vehicle row's bed units differ from the LF_CargoBedComponent m_iCapacity on its prefab (0 for no bed);
//   - a loop names a class that is not in the table, does not fit its class, or already fits the rung below
//     (it must need the next vehicle up).
//
// LF_CAPACITY_ROOT points the check at another tree (the selftest uses a temp one).
//
// red-proof: 2026-11-22, tools/validation/capacity-ladder-selftest.mjs runs this over a flat ladder (pickup =
// car), a bed that disagrees with its prefab and a loop that fits the rung below; each prints FAIL.
import { existsSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const root = process.env.LF_CAPACITY_ROOT || join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const confRel = "addons/LifeFramework/Configs/Vehicles/LF_Capacity.conf";
const CLASSES = ["body", "packs", "car", "pickup", "van", "truck"];

const bad = [];
const fail = (line, msg) => bad.push(`FAIL ${confRel}:${line} ${msg}`);

const confPath = join(root, confRel);
if (!existsSync(confPath)) {
  console.log(`FAIL ${confRel} is missing`);
  process.exit(1);
}

// Blocks: `LF_CapacityRow "{ID}" {` ... `}`; fields `m_name value`, a string value in quotes.
const rows = [];
const loops = [];
let current = null;
const lines = readFileSync(confPath, "utf8").split("\n");
lines.forEach((text, i) => {
  const open = /^\s*(LF_CapacityRow|LF_CapacityLoop)\s+"\{[0-9A-Fa-f]{16}\}"\s*\{\s*$/.exec(text);
  if (open) {
    current = { kind: open[1], line: i + 1, f: {} };
    (open[1] === "LF_CapacityRow" ? rows : loops).push(current);
    return;
  }
  if (current && /^\s*\}\s*$/.test(text)) {
    current = null;
    return;
  }
  const field = /^\s*(m_\w+)\s+(.+?)\s*$/.exec(text);
  if (current && field) current.f[field[1]] = field[2].replace(/^"(.*)"$/, "$1");
});

const num = (block, name) => {
  const v = block.f[name];
  return v === undefined ? 0 : Number.parseInt(v, 10);
};
const total = (row) => num(row, "m_iNativeSacks") + num(row, "m_iBedUnits");

// Rungs and growth.
if (rows.length !== CLASSES.length) {
  fail(1, `the ladder needs exactly ${CLASSES.length} rows (${CLASSES.join(", ")}), found ${rows.length}`);
} else {
  rows.forEach((row, i) => {
    if (row.f.m_sClass !== CLASSES[i]) fail(row.line, `row ${i} is "${row.f.m_sClass}", expected "${CLASSES[i]}"`);
    if (num(row, "m_iRung") !== i) fail(row.line, `${row.f.m_sClass} is on rung ${num(row, "m_iRung")}, expected ${i}`);
    if (i > 0 && total(row) <= total(rows[i - 1])) {
      fail(row.line, `${row.f.m_sClass} carries ${total(row)}, no more than ${rows[i - 1].f.m_sClass} (${total(rows[i - 1])}): the ladder must grow`);
    }
  });
}

// Bed units against the prefab.
for (const row of rows) {
  const prefab = row.f.m_Prefab;
  if (!prefab) {
    if (num(row, "m_iBedUnits") !== 0) fail(row.line, `${row.f.m_sClass} has no prefab but ${num(row, "m_iBedUnits")} bed units`);
    continue;
  }
  const path = prefab.replace(/^\{[0-9A-Fa-f]{16}\}/, "");
  const file = join(root, "addons", "LifeFramework", path);
  if (!existsSync(file)) {
    fail(row.line, `${row.f.m_sClass} prefab ${path} does not exist`);
    continue;
  }
  const bed = /LF_CargoBedComponent[^\n]*\{[^}]*?m_iCapacity\s+(\d+)/s.exec(readFileSync(file, "utf8"));
  const prefabBed = bed ? Number.parseInt(bed[1], 10) : 0;
  if (prefabBed !== num(row, "m_iBedUnits")) {
    fail(row.line, `${row.f.m_sClass} lists ${num(row, "m_iBedUnits")} bed units, its prefab's cargo bed holds ${prefabBed}`);
  }
}

// Loops need the next vehicle up.
const byClass = new Map(rows.map((r, i) => [r.f.m_sClass, i]));
for (const loop of loops) {
  const needs = loop.f.m_sNeeds;
  const run = num(loop, "m_iBestRun");
  if (!byClass.has(needs)) {
    fail(loop.line, `chain ${loop.f.m_sChain} needs "${needs}", which is not a class`);
    continue;
  }
  const rung = byClass.get(needs);
  if (run > total(rows[rung])) fail(loop.line, `chain ${loop.f.m_sChain} best run ${run} does not fit a ${needs} (${total(rows[rung])})`);
  if (rung > 0 && run <= total(rows[rung - 1])) {
    fail(loop.line, `chain ${loop.f.m_sChain} best run ${run} already fits a ${rows[rung - 1].f.m_sClass} (${total(rows[rung - 1])}): it must need the next vehicle up`);
  }
}

if (bad.length) {
  console.log(bad.join("\n"));
  console.log(`validate-capacity-ladder: ${bad.length} problem(s). Every rung carries more than the one below, and every chain's best loop needs the next vehicle up (#4189).`);
  process.exit(1);
}
console.log(`validate-capacity-ladder: OK (${rows.length} rungs, ${loops.length} loops)`);

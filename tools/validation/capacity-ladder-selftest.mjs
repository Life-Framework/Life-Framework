// tools/validation/capacity-ladder-selftest.mjs - validate-capacity-ladder (#4189).
// inputs: tools/validation/validate-capacity-ladder.mjs, tools/validation/capacity-ladder-selftest.mjs
//
// Runs the validator over a throwaway tree holding a capacity table and two prefabs:
//   - a sound ladder passes;
//   - a pickup that carries what the car does fails (the ladder must grow);
//   - a bed that disagrees with its prefab's cargo bed fails;
//   - a loop that fits the rung below its class fails, and a loop larger than its class fails;
//   - a missing class fails.
//
// red-proof: 2026-11-22 with the growth comparison in the validator turned to `<`, "a flat ladder fails" printed FAIL.
import { spawnSync } from "node:child_process";
import { mkdirSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { scratchDir } from "../lib/scratch.mjs";

let failed = 0;
const check = (ok, msg) => {
  console.log(`${ok ? "PASS" : "FAIL"}  ${msg}`);
  if (!ok) failed++;
};

const validator = join(dirname(fileURLToPath(import.meta.url)), "validate-capacity-ladder.mjs");
const dir = scratchDir("lf-capacity-ladder-");
const addon = join(dir, "addons", "LifeFramework");
mkdirSync(join(addon, "Configs", "Vehicles"), { recursive: true });
mkdirSync(join(addon, "Prefabs", "Vehicles"), { recursive: true });
writeFileSync(join(addon, "Prefabs", "Vehicles", "Car.et"), 'Vehicle : "{0000000000000001}x.et" {\n components {\n }\n}\n');
writeFileSync(
  join(addon, "Prefabs", "Vehicles", "Van.et"),
  'Vehicle : "{0000000000000001}x.et" {\n components {\n  LF_CargoBedComponent "{0000000000000002}" {\n   m_iCapacity 6\n   m_fBedWidth 1.2\n  }\n }\n}\n',
);

const row = (id, cls, rung, prefab, native, bed) =>
  ` LF_CapacityRow "{00000000000000${id}}" {\n  m_sClass "${cls}"\n  m_iRung ${rung}\n` +
  (prefab ? `  m_Prefab "{0000000000000009}Prefabs/Vehicles/${prefab}"\n` : "") +
  `  m_iNativeSacks ${native}\n  m_iBedUnits ${bed}\n }\n`;
const loop = (id, chain, run, needs) =>
  ` LF_CapacityLoop "{00000000000000${id}}" {\n  m_sChain "${chain}"\n  m_iBestRun ${run}\n  m_sNeeds "${needs}"\n }\n`;
const conf = ({ pickupNative = 6, vanBed = 6, loops = [loop("31", "coal", 5, "pickup")], drop = "" } = {}) => {
  const rows = [
    row("01", "body", 0, "", 1, 0),
    row("02", "packs", 1, "", 3, 0),
    row("03", "car", 2, "Car.et", 4, 0),
    row("04", "pickup", 3, "Car.et", pickupNative, 0),
    row("05", "van", 4, "Van.et", 2, vanBed),
    row("06", "truck", 5, "Van.et", 10, 6),
  ].filter((r) => !drop || !r.includes(`"${drop}"`));
  return `LF_CapacityConfig {\n${rows.join("")}${loops.join("")}}\n`;
};
const run = (text) => {
  writeFileSync(join(addon, "Configs", "Vehicles", "LF_Capacity.conf"), text);
  return spawnSync(process.execPath, [validator], { env: { ...process.env, LF_CAPACITY_ROOT: dir }, encoding: "utf8" });
};
const first = (r) => r.stdout.trim().split("\n")[0];

let r = run(conf());
check(r.status === 0, `a sound ladder passes (${first(r)})`);

r = run(conf({ pickupNative: 4 }));
check(r.status === 1 && /pickup carries 4, no more than car/.test(r.stdout), `a flat ladder fails (${first(r)})`);

r = run(conf({ vanBed: 8 }));
check(r.status === 1 && /van lists 8 bed units, its prefab's cargo bed holds 6/.test(r.stdout), `a bed that disagrees with its prefab fails (${first(r)})`);

r = run(conf({ loops: [loop("31", "coal", 4, "pickup")] }));
check(r.status === 1 && /already fits a car/.test(r.stdout), `a loop that fits the rung below fails (${first(r)})`);

r = run(conf({ loops: [loop("31", "coal", 9, "pickup")] }));
check(r.status === 1 && /does not fit a pickup/.test(r.stdout), `a loop larger than its class fails (${first(r)})`);

r = run(conf({ loops: [loop("31", "coal", 5, "lorry")] }));
check(r.status === 1 && /not a class/.test(r.stdout), `a loop that needs an unknown class fails (${first(r)})`);

r = run(conf({ drop: "truck" }));
check(r.status === 1 && /exactly 6 rows/.test(r.stdout), `a missing rung fails (${first(r)})`);

if (failed) {
  console.log(`capacity-ladder-selftest: ${failed} check(s) failed`);
  process.exit(1);
}
console.log("capacity-ladder-selftest: OK");

// tools/town/homes.selftest.mjs - acceptance for slice #3584 (`tools\cli town homes saint-philippe`).
//
// Run: node tools/town/homes.selftest.mjs
//   - every purpose row with purpose "a", a houseId and a tier is a home; the others are not;
//   - the committed layer holds one house marker and one sign per home, both with the home's houseId, markers on the dossier's footprint centre;
//   - the committed overlay prices every home once, by tier, and the tier prices rise with the tier;
//   - no home keeps a ForSaleSoon lock marker, and `town locks --check` agrees with the committed layer;
//   - every sign key the layer names is a row of the pack string table.
//   - a sign beside a door passes and a sign in a leaf's swing or walk-up is refused (fixture), and no committed sign stands in a probed door's swing or walk-up (#3886).
// red-proof (#3886): before the planner checked doors, the committed HomeSign_saint_philippe_sp_b44 stood in a door's swing and the "no committed sign in a door's swing" case printed FAIL.
// red-proof: with homes.json moved away, `town locks` put a ForSaleSoon marker back on every home and the "homes.json is committed", "every home is done" and "locks --check" cases printed FAIL (2026-11-19).
import { existsSync, readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { CONF_DIR, KEY_FOR_SALE, KEY_OWNED, LAYER_DIR, TIERS, doorHit, homeRows, loadDoors, renderOverlay, signBox } from "./homes.mjs";
import { main as locksMain } from "./locks.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
let failed = 0;
const check = (cond, msg) => {
  if (!cond) {
    failed++;
    console.log(`FAIL ${msg}`);
  }
};

const place = "saint-philippe";
const purpose = JSON.parse(readFileSync(join(ROOT, "docs/everon/places", place, "purpose.json"), "utf8"));
const rows = homeRows(purpose);
check(rows.length > 0, "the purpose file has homes for sale");
check(homeRows({ buildings: [{ id: "x", purpose: "b", houseId: "h", tier: "T1" }, { id: "y", purpose: "a", tier: "T1" }, { id: "z", purpose: "a", houseId: "h", tier: "T9" }] }).length === 0, "a shop, a home with no house id and a home with an unknown tier are no homes");
const tierPrices = Object.keys(TIERS).sort().map((t) => TIERS[t].price);
check(tierPrices.every((p, i) => i === 0 || p > tierPrices[i - 1]), "tier prices rise with the tier");

const homesFile = join(ROOT, "docs/everon/places", place, "homes.json");
check(existsSync(homesFile), "homes.json is committed");
const done = existsSync(homesFile) ? JSON.parse(readFileSync(homesFile, "utf8")).homes : [];
check(done.length === rows.length, `every home is done (${done.length} of ${rows.length})`);
check(new Set(done.map((h) => h.houseId)).size === done.length, "house ids are unique");

const layer = readFileSync(join(ROOT, LAYER_DIR, `Homes_${place}.layer`), "utf8");
for (const h of done) {
  const n = (layer.match(new RegExp(`m_sDebugIdentifier "${h.houseId}"`, "g")) ?? []).length;
  const s = (layer.match(new RegExp(`m_sHouseId "${h.houseId}"`, "g")) ?? []).length;
  check(n === 1 && s === 1, `${h.id}: one house marker and one sign carry ${h.houseId} (${n}, ${s})`);
}
check(layer.includes(KEY_FOR_SALE) && layer.includes(KEY_OWNED), "the markers name the for-sale and owned sign keys");

const overlay = readFileSync(join(ROOT, CONF_DIR, "LF_Property_Homes_SaintPhilippe.conf"), "utf8");
check(overlay === renderOverlay(rows), "the committed overlay matches the script");
for (const h of done) check(overlay.includes(`m_sHouseId "${h.houseId}"`) && overlay.includes(`m_iPrice ${TIERS[h.tier].price}`), `${h.id}: the overlay prices ${h.houseId}`);
check(existsSync(join(ROOT, CONF_DIR, "LF_Property_Homes_SaintPhilippe.conf.meta")), "the overlay has a .meta");

const locks = readFileSync(join(ROOT, LAYER_DIR, `Locks_${place}.layer`), "utf8");
for (const h of done) check(!locks.includes(`Lock_${place.replace(/-/g, "_")}_${h.id.replace(":", "_")} `), `${h.id}: no lock marker on a home with a house marker`);
const log = console.log;
console.log = () => {};
const locksOk = locksMain([place, "--check"], { root: ROOT }) === 0;
console.log = log;
check(locksOk, "town locks --check agrees with the committed lock layer");

// A door leaf on the world x axis facing +z, 1.3 m wide, 2.4 m tall, centre at the origin (the audit's door box: half sizes 0.65, 1.2, 0.1).
const door = { root: "Fixture", rp: [0, -1.2, 0], c: [0, 0, 0], h: [0.65, 1.2, 0.1], a: [[1, 0, 0], [0, 1, 0], [0, 0, 1]] };
const at = (x, z) => signBox([x, -1.2, z], 0);
check(doorHit(at(0, 0.5), [door]) !== null, "fixture: a sign 0.5 m in front of the leaf is in its swing");
check(doorHit(at(0, 2.5), [door]) !== null, "fixture: a sign 2.5 m in front of the leaf is in its walk-up");
check(doorHit(at(1.2, 0.05), [door]) === null, "fixture: a sign 1.2 m along the wall from the leaf (beside the frame) is clear");
check(doorHit(at(0, 3.5), [door]) === null, "fixture: a sign 3.5 m in front of the door is out of the walk-up");

const doors = loadDoors(ROOT, place);
check(doors.length > 0, `the door probe tools/town/probes/doors-${place}.json has door leaves`);
for (const m of layer.matchAll(/GenericEntity (HomeSign_\S+) : [^\n]*\{[\s\S]*?coords (\S+) (\S+) (\S+)\s+angleY (\S+)/g)) {
  const hit = doorHit(signBox([Number(m[2]), Number(m[3]), Number(m[4])], Number(m[5])), doors);
  check(!hit, `${m[1]}: no committed sign in a door's swing or walk-up (${hit?.text ?? ""})`);
}

const st = readFileSync(join(ROOT, "addons/LF_Everon1989/Language/lf_everon1989_localization.st"), "utf8");
for (const key of [KEY_FOR_SALE, KEY_OWNED]) check(st.includes(`Id "${key.slice(1)}"`), `${key} is a row of the pack string table`);

if (failed) {
  console.log(`SUMMARY homes-selftest fail ${failed} case(s)`);
  process.exit(1);
}
console.log(`SUMMARY homes-selftest ok ${done.length} homes`);

// tools/town/residenthomes.selftest.mjs - acceptance for slice #3233 (`tools\cli town residenthomes saint-philippe`).
//
// Run: node tools/town/residenthomes.selftest.mjs
//   - the committed layer holds, per resident home, a house marker with its own House place id, a name plate and a door anchor (b15 keeps the shared place: plate only);
//   - the resident overlay names, for every resident of HOMES, a `m_sHomePlaceId` that a layer places (this one or the residents slot's);
//   - no resident home keeps a Private lock marker, and `town locks --check` agrees with the committed layer;
//   - every plate title is a row of the pack string table, and each plate's sign id is a row of the sign overlay file.
// red-proof: with resident-homes.json moved away the "resident-homes.json is committed" and "town locks --check" cases printed FAIL (2026-11-19).
import { existsSync, readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { HOMES, LAYER_DIR, SIGN_DIR, ST_FILE, placeIdOf, signId, titleKey, withTitles } from "./residenthomes.mjs";
import { main as locksMain } from "./locks.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
let failed = 0;
const check = (cond, msg) => {
  if (!cond) {
    failed++;
    console.log(`FAIL ${msg}`);
  }
};
const read = (f) => readFileSync(join(ROOT, f), "utf8");

const place = "saint-philippe";
const layer = read(`${LAYER_DIR}/Homes_${place}_resident.layer`);
const shared = read(`${LAYER_DIR}/Place_${place}_residents.layer`);
const conf = read("addons/LF_Everon1989/Configs/Residents/LF_Residents_Everon1989.conf");
const signs = read(`${SIGN_DIR}/LF_Signs_ResidentHomes.conf`);
const st = read(ST_FILE);
check(new Set(HOMES.map((h) => placeIdOf(place, h))).size >= HOMES.length - 1, "the homes' place ids are distinct (b15 names the shared place)");
for (const h of HOMES) {
  const id = placeIdOf(place, h);
  if (h.existing) check(shared.includes(`m_sId "${id}"`), `${h.resident}: the residents slot places ${id}`);
  else {
    check(layer.includes(`m_sId "${id}"`) && layer.includes('m_sKind "House"'), `${h.resident}: the layer places the House ${id}`);
    check(layer.includes(`m_sSignId "${signId(h)}"`), `${h.resident}: the layer has a plate`);
    check(layer.includes(`ResidentBench_saint_philippe_${h.row.replace(":", "_")} `) && layer.includes('m_sAnchorId "bench"'), `${h.resident}: the layer has a bench anchor outside the house`);
    check(layer.includes("LF_HouseManagerComponent"), "the markers are house markers");
  }
  const at = conf.indexOf(`m_sId "${h.resident}"`);
  const next = conf.indexOf("m_sOp", at); // the next record starts at its own m_sOp
  const body = at < 0 ? "" : conf.slice(at, next < 0 ? conf.length : next);
  check(body.includes(`m_sHomePlaceId "${id}"`), `${h.resident}: the resident overlay names the home ${id}`);
  check(signs.includes(`m_sId "${signId(h)}"`) && signs.includes(`#${titleKey(h)}`), `${h.resident}: the sign overlay has the plate`);
  check(st.includes(`Id "${titleKey(h)}"`), `${h.resident}: the plate title is a pack string`);
}
check(!/m_sOwner/.test(layer), "no resident home has an owner");
check(withTitles(st, HOMES) === st, "the string table already holds every plate title (the generator adds nothing)");
check(existsSync(join(ROOT, "docs/everon/places", place, "resident-homes.json")), "resident-homes.json is committed");
check(locksMain([place, "--check"], { root: ROOT }) === 0, "`town locks --check` agrees with the committed layer");
const locks = read(`${LAYER_DIR}/Locks_${place}.layer`);
for (const row of ["sp_b09", "sp_b17", "sp_b50", "sp_b117"]) check(!locks.includes(`Lock_saint_philippe_${row}`), `${row} keeps no Private lock marker`);

if (failed) {
  console.log(`residenthomes selftest: ${failed} failed`);
  process.exit(1);
}
console.log("residenthomes selftest: OK");

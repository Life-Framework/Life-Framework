// tools/town/locks.selftest.mjs - acceptance for slice #3204 (`node tools/town/locks.mjs`: the Saint-Philippe lock layer).
//
// Run: node tools/town/locks.selftest.mjs
//   - one lock per purpose (private, staff, for sale, opening soon, military, school, to let, closed) with its pack sign key;
//   - a fitted (exists/flight) row, a D1 chapel, sp:b49 and a side building of a fitted business get no lock;
//   - a shed sold with a home stays private, a side workshop of a planned shop is "opening soon";
//   - a lock stands at the footprint centre with the dossier's ground height, and the layer overrides the prefab's component by GUID;
//   - every sign key the layer names is a row of the pack string table;
//   - the committed layer matches the script and `town doors saint-philippe --planned` reports 0 errors (the acceptance).
// red-proof: with the exists/flight skips removed from keyFor, four cases printed FAIL (exists row, side shed, flight row, committed layer) (2026-11-18).
import { existsSync, readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { buildLocks, COMPONENT_GUID, KEYS, layerPath, loadPlace, PREFAB, renderLayer } from "./locks.mjs";
import { checkDoors, loadPlace as loadDoors } from "./doors.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
let failed = 0;
const check = (cond, msg) => {
  if (!cond) {
    failed++;
    console.log(`FAIL ${msg}`);
  }
};

const row = (id, purpose, use, extra = {}) => ({ id, purpose, use, status: "planned", card: "L0", ...extra });
const purpose = {
  buildings: [
    row("sp:b01", "a", "farmhouse", { card: "H1" }),
    row("sp:b02", "b", "bakery", { card: "S7" }),
    row("sp:b03", "e", "farm barn: Propriété privée"),
    row("sp:b04", "e", "dock warehouse: staff only"),
    row("sp:b05", "d", "school: École, summer holidays (locked)", { card: "L0" }),
    row("sp:b06", "e", "army barracks: zone militaire"),
    row("sp:b07", "e", "workshop: À louer bientôt"),
    row("sp:b08", "e", "harbour company offices: fermé"),
    row("sp:b09", "b", "café", { status: "exists", card: "EXISTS" }),
    row("sp:b10", "b", "café shed", { status: "exists", card: "EXISTS", partOf: "sp:b09" }),
    row("sp:b11", "d", "wayside chapel, open", { card: "D1" }),
    row("sp:b49", "b", "org registry", { card: "X33" }),
    row("sp:b12", "a", "", { partOf: "sp:b01", card: "H1" }),
    row("sp:b13", "b", "", { partOf: "sp:b02", card: "S7" }),
    row("sp:b14", "c", "clerk's home", { card: "R1" }),
    row("sp:b15", "b", "fence", { status: "flight", card: "FLIGHT" }),
  ],
};
const dossier = new Map();
for (let i = 1; i <= 15; i++) dossier.set(`sp:b${String(i).padStart(2, "0")}`, { cx: 100 + i, cz: 200 + i, y: 10 + i });
dossier.set("sp:b49", { cx: 1, cz: 2, y: 3 });
const { locks, errors } = buildLocks({ purpose, dossier });
const keyOf = (id) => locks.find((l) => l.id === id)?.key ?? null;

check(errors.length === 0, `no errors: ${errors.join("; ")}`);
check(keyOf("sp:b01") === KEYS.forSale, "a home for sale is for sale soon");
check(keyOf("sp:b02") === KEYS.opening, "a planned shop is opening soon");
check(keyOf("sp:b03") === KEYS.private, "a barn is private");
check(keyOf("sp:b04") === KEYS.staff, "a warehouse is staff only");
check(keyOf("sp:b05") === KEYS.school, "the school is shut for the holidays");
check(keyOf("sp:b06") === KEYS.military, "barracks are a military zone");
check(keyOf("sp:b07") === KEYS.toLet, "a dock workshop is to let soon");
check(keyOf("sp:b08") === KEYS.closed, "company offices are closed");
check(keyOf("sp:b09") === null, "an exists row has no lock");
check(keyOf("sp:b10") === null, "a side shed of a fitted business has no lock");
check(keyOf("sp:b11") === null, "a D1 chapel stays open");
check(keyOf("sp:b49") === null, "sp:b49 (the org registry) has no lock");
check(keyOf("sp:b12") === KEYS.private, "a shed sold with a home is private");
check(keyOf("sp:b13") === KEYS.opening, "a side workshop of a planned shop is opening soon");
check(keyOf("sp:b14") === KEYS.private, "a resident's home is private");
check(keyOf("sp:b15") === null, "a flight row has no lock");
check(new Set(locks.map((l) => l.key)).size === Object.keys(KEYS).length, "one row per purpose");

const one = locks.find((l) => l.id === "sp:b01");
check(one.x === 101 && one.z === 201 && one.y === 11, "the lock stands at the footprint centre, dossier ground height");
const text = renderLayer("saint-philippe", locks);
check(text.includes(`: "${PREFAB}" {`) && text.includes(`LF_BuildingLockComponent "${COMPONENT_GUID}"`), "the layer overrides the prefab's component by GUID");
check(text.includes("coords 101 11 201"), "the layer holds the coordinates");
check(buildLocks({ purpose: { buildings: [row("sp:b90", "e", "x")] }, dossier: new Map() }).errors.length === 1, "a row with no dossier building is an error");

// The committed place.
const pack = readFileSync(join(ROOT, "addons/LF_Everon1989/Language/lf_everon1989_localization.st"), "utf8");
const real = loadPlace(ROOT, "saint-philippe");
const made = buildLocks(real);
check(made.errors.length === 0, `Saint-Philippe locks build: ${made.errors.join("; ")}`);
for (const key of Object.values(KEYS)) check(pack.includes(`Id "${key}"`), `the pack table has ${key}`);
check(new Set(made.locks.map((l) => l.key)).size === Object.keys(KEYS).length, "Saint-Philippe uses every purpose");
const file = layerPath(ROOT, "saint-philippe");
check(existsSync(file) && readFileSync(file, "utf8") === renderLayer("saint-philippe", made.locks), "Locks_saint-philippe.layer matches the script (rerun node tools/town/locks.mjs saint-philippe)");
const doors = checkDoors({ ...loadDoors(ROOT, "saint-philippe"), planned: true });
check(doors.errors.length === 0, `town doors saint-philippe --planned: ${doors.errors.join("; ")}`);
const bare = doors.rows.filter((r) => r.problem === "warn" && /^(a|c|e)$/.test(r.purpose ?? ""));
check(bare.length === 0, `no home or outbuilding is left without a carrier: ${bare.map((r) => r.id).join(", ")}`);

if (failed) {
  console.log(`SUMMARY locks-selftest fail ${failed}`);
  process.exit(1);
}
console.log("SUMMARY locks-selftest ok");

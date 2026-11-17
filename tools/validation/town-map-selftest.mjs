// tools/validation/town-map-selftest.mjs
// inputs: tools/town/**, tools/commands/town.mjs, tools/scout/types.mjs, docs/everon/places/*/dossier.json, docs/everon/places/*/mapping.json
//
// Proves `tools\cli town map` (docs/architecture/everon-world.md 3.2): a place template (tools/town/templates/<kind>.json)
// scored against a scout dossier (schema 1, fixture tools/town/fixtures/map-dossier.json) gives a proposed mapping.json,
// and `--check` refuses an incomplete one. The fixture is an invented place: no real coordinate.
//   - every required slot of the `town` template gets a building or a free plot;
//   - a slot that needs an interior never maps to a building the dossier says has none, and an unverified
//     ("unknown") interior is carried as a `verify` flag, never silently trusted;
//   - one building serves one slot (street slots share a frontage); ruins are never used;
//   - `--check` fails on a missing or `none` required slot, an unknown building or plot, a building without the
//     interior the slot needs, a building claimed twice, and a plot slot with no prefab;
//   - a template holds no coordinate and no place name (`checkTemplate`).
//   - the template's building types and the scout's types are ONE vocabulary (tools/scout/types.mjs TYPE_ALIASES): a legacy
//     name (municipal-office, house-town, shop-*) and the scout's (government, townhouse, shop) resolve to each other, and
//     every type a committed template names is known;
//   - the template is chosen from the flag, else the mapping's own template, else the dossier, else town (`chooseTemplate`);
//   - the real dossiers' halls, shops and houses are found without a hand edit (Saint-Philippe fills the town template, Meaux
//     fills the village one, and Meaux's approved mapping passes `--check` under its own template).
// red-proof: with TYPE_ALIASES emptied this printed "FAIL alias: municipal-office resolves to government" (2026-11-17).
// red-proof: with chooseTemplate ignoring the mapping this printed "FAIL template choice: a mapping for village beats the gazetteer kind" (2026-11-17).
// red-proof: before tools/town/map.mjs existed this printed "FAIL cannot load tools/town/map.mjs: ERR_MODULE_NOT_FOUND" (2026-11-16).
// red-proof: with the interior gate in `eligible()` removed this printed "FAIL slot arrival maps to b02, which has no interior (only candidate left)" (2026-11-16).
// red-proof: with "At 4503, 10772." added to a template's `about` this printed "FAIL template town has problems: .about: ... holds a coordinate pair" (2026-11-16).

import { spawnSync } from "node:child_process";
import { existsSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const FIXTURE = join(ROOT, "tools", "town", "fixtures", "map-dossier.json");
// The legacy names the templates used to carry; a committed template uses the scout's own type instead.
const LEGACY_TYPE_NAMES = new Set(["municipal-office", "house-town", "house-village", "fuel-station", "industrial-shed", "farm-building", "house-addon", "cottage"]);
let failed = 0;
const fail = (msg) => {
  console.log(`FAIL  ${msg}`);
  failed++;
};
const check = (cond, msg) => {
  if (!cond) fail(msg);
};
const clone = (o) => JSON.parse(JSON.stringify(o));

let map;
try {
  map = await import(pathToFileURL(join(ROOT, "tools", "town", "map.mjs")).href);
} catch (e) {
  fail(`cannot load tools/town/map.mjs: ${e.code ?? e.message}`);
  console.log(`SUMMARY town-map-selftest fail ${failed}`);
  process.exit(1);
}

const dossier = JSON.parse(readFileSync(FIXTURE, "utf8"));
const byId = (d) => Object.fromEntries(d.buildings.map((b) => [b.id, b]));
const town = map.loadTemplate("town", ROOT);
const village = map.loadTemplate("village", ROOT);

// ---- templates: shape, and no coordinate or place name anywhere
for (const t of [town, village]) {
  const probs = map.checkTemplate(t);
  check(probs.length === 0, `template ${t.kind} has problems: ${probs.join("; ")}`);
}
const required = town.slots.filter((s) => s.required).map((s) => s.id);
for (const id of ["arrival", "hostel", "cafe", "bank", "police", "clinic", "general-store", "clothing", "gun-counter", "fuel", "fence", "board"]) {
  check(required.includes(id), `town template lacks required slot ${id}`);
}
// ---- one type vocabulary: aliases resolve both ways, and no template names a type nobody knows
{
  const types = await import(pathToFileURL(join(ROOT, "tools", "scout", "types.mjs")).href);
  const eq = (a, b) => JSON.stringify(a) === JSON.stringify(b);
  check(eq(types.canonicalTypes("municipal-office"), ["government"]), "alias: municipal-office resolves to government");
  check(eq(types.canonicalTypes("house-town"), ["townhouse"]) && eq(types.canonicalTypes("house-village"), ["house"]), "alias: house-town and house-village resolve to townhouse and house");
  check(eq(types.canonicalTypes("fuel-station"), ["fuel"]) && eq(types.canonicalTypes("fuel"), ["fuel"]), "alias: fuel-station and fuel are the same type");
  check(types.canonicalTypes("shop-*").includes("shop") && types.canonicalTypes("shop-drug").includes("shop"), "alias: the shop-* family and shop-drug resolve to shop");
  check(types.canonicalTypes("warp-gate").length === 0, "alias: an unknown name resolves to nothing");
  check(types.typeIndex(["municipal-office", "house-town"], "government") === 0, "typeIndex: a legacy template name finds the scout's government");
  check(types.typeIndex(["government", "townhouse"], "house-town") === 1, "typeIndex: the scout's template name finds a hand-written house-town");
  check(types.typeIndex(["shop-*"], "shop") === 0 && types.typeIndex(["shop"], "shop-clothes") === 0, "typeIndex: shop-* and shop meet either way");
  check(types.typeIndex(["house", "townhouse"], "villa") === -1, "typeIndex: a villa is neither a house nor a townhouse");
  check(types.typeIndex(["shop-drug", "shop"], "shop", "ShopHouse_E_2I01t_drugstore") === 0 && types.typeIndex(["shop-drug", "shop"], "shop", "ShopHouse_E_2I01t_clothesstore") === 1, "typeIndex: shop-drug picks the drugstore by its stem and a plain shop still falls through to shop");
  check(types.typeIndex(["shop-drug", "townhouse"], "shop", "ShopHouse_E_2I01t_clothesstore") === -1 && types.typeIndex(["shop-clothes"], "shop-clothes") === 0, "typeIndex: a refinement does not match another kind of shop, but matches its own hand-written type");
  check(types.typeIndex(["house"], "shed") === -1, "typeIndex: a shed is not a house");
  check(types.typeIndex(["mystery"], "mystery") === 0, "typeIndex: a name the table does not know still matches itself");
  check(types.CANONICAL_TYPES.has("government") && types.CANONICAL_TYPES.has("townhouse"), "the canonical set is the scout's building types");
  for (const [alias, to] of Object.entries(types.TYPE_ALIASES)) check(to.length > 0 && to.every((t) => types.CANONICAL_TYPES.has(t)), `alias ${alias} points at a type scout does not know`);
  for (const kind of ["town", "village"]) {
    const t = map.loadTemplate(kind, ROOT);
    for (const slot of t.slots) check(types.unknownTypes(slot.needs.types ?? []).length === 0, `template ${kind} slot ${slot.id} names an unknown building type ${types.unknownTypes(slot.needs.types ?? [])}`);
    check(!t.slots.some((s) => (s.needs.types ?? []).some((x) => LEGACY_TYPE_NAMES.has(x))), `template ${kind} still uses a legacy alias instead of the scout's type`);
  }
  const bad = clone(town);
  bad.slots[0].needs.types = ["warp-gate"];
  check(map.checkTemplate(bad).some((p) => p.includes("warp-gate")), "checkTemplate lets an unknown building type through");
}

// ---- which template a place uses
{
  const c = map.chooseTemplate;
  check(c({ flag: "village", mapping: { template: "town" }, dossier: { place: { kind: "town" } } }).kind === "village", "template choice: the flag wins");
  check(c({ mapping: { template: "village" }, dossier: { place: { kind: "other" } } }).kind === "village", "template choice: a mapping for village beats the gazetteer kind");
  check(c({ mapping: { template: "village" }, dossier: { place: { kind: "town" } } }).kind === "village", "template choice: a mapping beats the dossier's place kind");
  check(c({ dossier: { place: { kind: "other", template: "village" } } }).kind === "village", "template choice: the dossier's scouted template is used");
  check(c({ dossier: { place: { kind: "village" } } }).kind === "village", "template choice: a place kind that is a template name is used");
  check(c({ dossier: { place: { kind: "urban" } } }).kind === "town" && c({}).kind === "town", "template choice: otherwise town");
  check(c({ flag: "../x", dossier: {} }).kind === "town", "template choice: a flag that is not a plain name is ignored");
}

// ---- the real dossiers: halls, shops and houses are found without a hand edit
{
  const place = (slug) => {
    const dir = join(ROOT, "docs", "everon", "places", slug);
    const d = join(dir, "dossier.json");
    return existsSync(d) ? { dossier: JSON.parse(readFileSync(d, "utf8")), mapping: existsSync(join(dir, "mapping.json")) ? JSON.parse(readFileSync(join(dir, "mapping.json"), "utf8")) : null } : null;
  };
  const sp = place("saint-philippe");
  if (sp) {
    const t = map.loadTemplate(map.chooseTemplate({ dossier: sp.dossier, mapping: sp.mapping }).kind, ROOT);
    const prop = map.proposeMapping(sp.dossier, t);
    const types = new Map(sp.dossier.buildings.map((b) => [b.id, b.type]));
    check((prop.missing ?? []).length === 0, `saint-philippe: the proposal leaves ${(prop.missing ?? []).join(", ")} unmapped`);
    check(["government", "townhouse", "house", "villa", "hotel"].includes(types.get(prop.slots.arrival.building)), "saint-philippe: arrival should land on a hall or a home");
    check(types.get(prop.slots.police.building) === "police", `saint-philippe: police should take the police villa, took ${types.get(prop.slots.police.building)}`);
    check(types.get(prop.slots["general-store"].building) === "shop" && types.get(prop.slots.clothing.building) === "shop", "saint-philippe: the stores should take shops");
  }
  const mx = place("meaux");
  if (mx?.mapping) {
    const choice = map.chooseTemplate({ dossier: mx.dossier, mapping: mx.mapping });
    check(choice.kind === "village", `meaux: the template should be village (from its mapping), not ${choice.kind}`);
    const t = map.loadTemplate(choice.kind, ROOT);
    const prop = map.proposeMapping(mx.dossier, t);
    const types = new Map(mx.dossier.buildings.map((b) => [b.id, b.type]));
    check(prop.slots.hall.building && types.get(prop.slots.hall.building) === "government", `meaux: hall should be the government office, got ${prop.slots.hall.building}`);
    check(prop.slots.shop.building && types.get(prop.slots.shop.building) === "shop", `meaux: shop should be the shop, got ${prop.slots.shop.building}`);
    check(["house", "townhouse", "farmhouse"].includes(types.get(prop.slots.house.building)), `meaux: house should be a dwelling, got ${prop.slots.house.building}`);
    check(prop.slots.hall.building === mx.mapping.slots.hall.building && prop.slots.shop.building === mx.mapping.slots.shop.building && prop.slots.pub.building === mx.mapping.slots.pub.building, "meaux: the tool's hall, shop and pub equal the reviewed ones, no hand edit");
    check(map.checkMapping(mx.mapping, mx.dossier, t).length === 0, `meaux: the approved mapping fails --check: ${map.checkMapping(mx.mapping, mx.dossier, t).join("; ")}`);
    check(mx.mapping.status === "approved", `meaux: the lead approved this mapping (#2260), status is ${mx.mapping.status}`);
  }
}

if (failed) {
  console.log(`SUMMARY town-map-selftest fail ${failed}`);
  process.exit(1);
}
const bad = (mutate) => {
  const t = clone(town);
  mutate(t);
  return map.checkTemplate(t);
};
check(bad((t) => (t.slots[0].x = 4503)).length > 0, "checkTemplate lets a coordinate key (x) through");
check(bad((t) => (t.slots[0].needs.position = [1, 2, 3])).length > 0, "checkTemplate lets a position array through");
check(bad((t) => (t.about = "the square is at 4450, 10780")).length > 0, "checkTemplate lets a coordinate pair in a string through");
check(bad((t) => (t.slots[0].needs.types = ["Saint-Philippe hall"])).length > 0, "checkTemplate lets a place name through");
check(bad((t) => t.slots.push(clone(t.slots[0]))).length > 0, "checkTemplate lets a duplicate slot id through");
check(bad((t) => delete t.slots[0].kit).length > 0, "checkTemplate lets a slot with no kit through");

// ---- the proposer on the fixture
const m = map.proposeMapping(dossier, town);
const bld = byId(dossier);
check(m.schema === 1 && m.template === "town" && m.place === "sample-square", `mapping header is ${JSON.stringify({ s: m.schema, t: m.template, p: m.place })}`);
const claimed = new Map();
for (const slot of town.slots) {
  const e = m.slots[slot.id];
  check(!!e, `slot ${slot.id} is missing from the proposed mapping`);
  if (!e) continue;
  if (slot.required) check(!!(e.building || e.plot), `required slot ${slot.id} got neither a building nor a plot (${JSON.stringify(e)})`);
  if (e.building) {
    const b = bld[e.building];
    check(!!b, `slot ${slot.id} maps to unknown building ${e.building}`);
    if (!b) continue;
    check(!b.ruin, `slot ${slot.id} maps to a ruin (${b.id})`);
    if (slot.needs.interior === "required") {
      check(b.interior.value !== "no", `slot ${slot.id} maps to ${b.id}, which has no interior`);
      if (b.interior.value === "unknown") check((e.verify ?? []).includes("interior"), `slot ${slot.id} maps to ${b.id} with an unverified interior and no verify flag`);
    }
    if (!e.shared) {
      check(!claimed.has(b.id), `building ${b.id} is claimed by ${claimed.get(b.id)} and ${slot.id}`);
      claimed.set(b.id, slot.id);
    }
  }
  if (e.plot) {
    const p = dossier.freePlots.plots.find((x) => x.id === e.plot);
    check(!!p, `slot ${slot.id} maps to unknown plot ${e.plot}`);
    check(!!e.prefab && !!e.prefab.stem, `slot ${slot.id} maps to a plot with no prefab`);
  }
  check(Array.isArray(e.why) && e.why.length > 0, `slot ${slot.id} carries no reason`);
}
// Fit: the strong matches the doc names, by the scout's types.
check(m.slots.clothing.building === "b03", `clothing should take the clothes shop b03, took ${m.slots.clothing.building}`);
check(m.slots.fuel.building === "b16", `fuel should take the fuel station b16, took ${m.slots.fuel.building}`);
check(["b17", "b18"].includes(m.slots.fence.building), `fence should take the garage or the barn, took ${m.slots.fence.building}`);
check(m.slots["general-store"].building === "b07" && (m.slots["general-store"].verify ?? []).includes("interior"), `general-store should take the shop b07 with an interior verify flag (${JSON.stringify(m.slots["general-store"])})`);
check(m.slots.board.shared === true && !!m.slots.board.building, "a street slot should name a shared frontage building");
// b02 is a town house close to the square with no interior: no interior slot may use it.
for (const [id, e] of Object.entries(m.slots)) {
  const slot = town.slots.find((s) => s.id === id);
  if (slot.needs.interior === "required" && e.building === "b02") fail(`slot ${id} maps to b02, which has no interior`);
}
check(map.checkMapping(m, dossier, town).length === 0, `the proposed mapping fails its own check: ${map.checkMapping(m, dossier, town).join("; ")}`);
check(JSON.stringify(map.proposeMapping(dossier, town)) === JSON.stringify(m), "proposeMapping is not deterministic");

// ---- --check refuses an incomplete or wrong mapping
const mutated = (fn) => {
  const x = clone(m);
  fn(x);
  return map.checkMapping(x, dossier, town);
};
const mentions = (probs, ...words) => probs.some((p) => words.every((w) => p.includes(w)));
check(mentions(mutated((x) => delete x.slots.bank), "bank"), "check does not fail on a missing required slot");
check(mentions(mutated((x) => (x.slots.police = { none: true, reason: "no building" })), "police"), "check does not fail on a required slot that is none");
check(map.checkMapping({ ...clone(m), slots: Object.fromEntries(Object.entries(m.slots).map(([k, v]) => [k, k === "taxi" ? { none: true, reason: "the lead dropped it" } : v])) }, dossier, town).length === 0, "check should accept `none` with a reason on an optional slot");
check(mentions(mutated((x) => (x.slots.taxi = { none: true })), "taxi", "reason"), "check does not fail on a none with no reason");
check(mentions(mutated((x) => (x.slots.bank = { building: "b02", why: ["hand edit"] })), "bank", "interior"), "check does not fail on an interior slot mapped to a building with no interior");
check(mentions(mutated((x) => (x.slots.bank = { building: x.slots.police.building, why: ["hand edit"] })), "claimed"), "check does not fail on one building claimed twice");
check(mentions(mutated((x) => (x.slots.bank = { building: "b99", why: ["hand edit"] })), "bank", "b99"), "check does not fail on an unknown building");
check(mentions(mutated((x) => (x.slots.bank = { building: "b14", why: [] }, (x.slots.hostel = { building: "b18", why: [] }))), "hostel"), "check does not fail on a hostel in a barn with no interior");
check(mentions(mutated((x) => (x.slots.fuel = { plot: "p3", why: ["hand edit"] })), "fuel", "prefab"), "check does not fail on a plot with no prefab");
check(mentions(mutated((x) => (x.slots.fuel = { plot: "p9", prefab: { type: "fuel-station", stem: "x" }, why: [] })), "fuel", "p9"), "check does not fail on an unknown plot");
check(mentions(mutated((x) => (x.slots.nonsense = { building: "b01", why: [] })), "nonsense"), "check does not fail on a slot the template does not have");
check(mentions(mutated((x) => (x.template = "village")), "template"), "check does not fail on a mapping made for another template");
check(mentions(mutated((x) => (x.slots.taxi = { building: x.slots.bank.building, shared: true, why: [] })), "claimed") === false, "a street slot sharing a frontage should not count as a claim");
check(mentions(mutated((x) => (x.slots.police.shared = true)), "police", "share"), "check does not fail on a non-street slot that shares a building");

// ---- no building for a slot: a free plot with a prefab, then none
const noFuel = clone(dossier);
noFuel.buildings = noFuel.buildings.filter((b) => b.type !== "fuel-station");
const withPlot = map.proposeMapping(noFuel, town);
check(!!withPlot.slots.fuel.plot && withPlot.slots.fuel.prefab?.stem === "FuelStation_E_01_building", `with no fuel station a plot and a fuel prefab were expected, got ${JSON.stringify(withPlot.slots.fuel)}`);
check(map.checkMapping(withPlot, noFuel, town).length === 0, "the plot mapping should pass check");
const noPlots = clone(noFuel);
noPlots.freePlots.plots = [];
const none = map.proposeMapping(noPlots, town);
check(none.slots.fuel.none === true && typeof none.slots.fuel.reason === "string" && none.slots.fuel.reason.length > 0, `with no building and no plot fuel should be none with a reason, got ${JSON.stringify(none.slots.fuel)}`);
check(mentions(map.checkMapping(none, noPlots, town), "fuel"), "check does not fail on the unmappable required slot");
check(none.status === "incomplete" && m.status === "proposed", `status should be incomplete/proposed, got ${none.status}/${m.status}`);

// A tight dossier: the one shop is wanted by two slots; both still get a home or the loser a plot, never a shared building.
const few = clone(dossier);
few.buildings = few.buildings.filter((b) => ["b03", "b01", "b16", "b17", "b12", "b04"].includes(b.id));
const fm = map.proposeMapping(few, town);
const seen = new Map();
for (const [id, e] of Object.entries(fm.slots)) {
  if (e.building && !e.shared) {
    check(!seen.has(e.building), `tight dossier: ${e.building} claimed by ${seen.get(e.building)} and ${id}`);
    seen.set(e.building, id);
  }
}

// The only town house is the closed one: no interior slot may take it, whatever its score.
const closedOnly = clone(dossier);
closedOnly.buildings = closedOnly.buildings.filter((b) => b.id === "b02" || b.type === "fuel-station");
const cm = map.proposeMapping(closedOnly, town);
for (const slot of town.slots) {
  if (slot.needs.interior === "required" && cm.slots[slot.id].building === "b02") fail(`slot ${slot.id} maps to b02, which has no interior (only candidate left)`);
}
check(cm.slots.arrival.none === true, `with only a closed house left arrival should be none, got ${JSON.stringify(cm.slots.arrival)}`);

// ---- dossier schema
check(throws(() => map.proposeMapping({ ...clone(dossier), schema: 2 }, town)), "a schema 2 dossier should be refused");
check(throws(() => map.proposeMapping({ schema: 1 }, town)), "a dossier with no buildings should be refused");
function throws(fn) {
  try {
    fn();
    return false;
  } catch {
    return true;
  }
}

// ---- the CLI: `town map`, `--check`, and no overwrite of a reviewed mapping
const tmp = mkdtempSync(join(tmpdir(), "lf-town-map-"));
try {
  const out = join(tmp, "mapping.json");
  const cli = (...args) => spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "town", ...args], { cwd: ROOT, encoding: "utf8", timeout: 60000, windowsHide: true });
  const noSub = cli();
  check(/\bmap\b/.test(noSub.stdout + noSub.stderr), "`town` with no subcommand should list `map`");
  const unknown = cli("nosuchsub");
  check(unknown.status !== 0, "`town nosuchsub` should fail");
  const traversal = cli("../map");
  check(traversal.status !== 0, "`town ../map` should be refused");
  const r = cli("map", "sample-square", "--dossier", FIXTURE, "--out", out);
  check(r.status === 0 && existsSync(out), `town map failed (${r.status}): ${r.stdout}${r.stderr}`);
  check(/SUMMARY town-map ok/.test(r.stdout), `town map prints no SUMMARY ok line: ${r.stdout}`);
  const r2 = cli("map", "sample-square", "--dossier", FIXTURE, "--out", out);
  check(r2.status !== 0 && /--force/.test(r2.stdout + r2.stderr), "town map should refuse to overwrite an existing mapping without --force");
  const r3 = cli("map", "sample-square", "--dossier", FIXTURE, "--out", out, "--check");
  check(r3.status === 0 && /SUMMARY town-map check ok/.test(r3.stdout), `--check on the proposal should pass: ${r3.stdout}${r3.stderr}`);
  const edited = JSON.parse(readFileSync(out, "utf8"));
  delete edited.slots.bank;
  writeFileSync(out, JSON.stringify(edited));
  const r4 = cli("map", "sample-square", "--dossier", FIXTURE, "--out", out, "--check");
  check(r4.status !== 0 && /bank/.test(r4.stdout) && /SUMMARY town-map check fail/.test(r4.stdout), `--check on a mapping with no bank should fail naming it: ${r4.stdout}${r4.stderr}`);
  const r5 = cli("map", "sample-square", "--dossier", FIXTURE, "--out", out, "--force");
  check(r5.status === 0, "town map --force should rewrite the mapping");
  const bad = cli("map", "../evil", "--dossier", FIXTURE, "--out", out);
  check(bad.status !== 0, "a place slug with .. should be refused");
} finally {
  rmSync(tmp, { recursive: true, force: true });
}

// ---- the committed templates stay out of coordinates on disk too (not just after parsing)
for (const kind of ["town", "village"]) {
  const text = readFileSync(join(ROOT, "tools", "town", "templates", `${kind}.json`), "utf8");
  check(!/\d{3,}\s*,\s*-?\d{3,}/.test(text), `templates/${kind}.json holds a coordinate-looking pair`);
}

if (failed) {
  console.log(`SUMMARY town-map-selftest fail ${failed}`);
  process.exit(1);
}
console.log("SUMMARY town-map-selftest ok");

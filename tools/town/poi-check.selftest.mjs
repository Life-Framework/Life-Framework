// tools/town/poi-check.selftest.mjs - acceptance for slice #2245 (Everon map epic #2223, card M17: town pipeline hook).
//
// A town template slot names the POI kind it puts on the map (`"poi": "<front kind>"`, or `"poi": "none"` when it puts none),
// and `town map --check` (until `town check`, card T4 #2206, calls the same function) fails a slot whose front kind has no
// POI row in the core conf or a pack overlay, or that names no POI kind at all (docs/architecture/everon-map.md "Where the
// data comes from"). Run: node tools/town/poi-check.selftest.mjs
//   - the committed town and village templates pass against the committed POI confs;
//   - a slot whose front kind has no POI row fails (the acceptance);
//   - a slot with no `poi` fails; an explicit "none" passes; a front kind the slot does not register fails;
//   - the POI table is the core conf plus every pack overlay, read from `m_sKind` rows;
//   - `town map <slug> --check` fails on such a template and prints the slot and the way out.
// red-proof: before tools/town/lib/poi-check.mjs existed this printed "FAIL cannot load tools/town/lib/poi-check.mjs: ERR_MODULE_NOT_FOUND" (2026-11-17).
// red-proof: with the `kinds.has(poi)` test removed from checkSlotPois this printed "FAIL a slot whose front kind has no POI row passes (the acceptance)" (2026-11-17).

import { cpSync, mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
let failed = 0;
const fail = (msg) => {
  console.log(`FAIL  ${msg}`);
  failed++;
};
const check = (cond, msg) => {
  if (!cond) fail(msg);
};
const clone = (o) => JSON.parse(JSON.stringify(o));
const url = (...p) => pathToFileURL(join(ROOT, ...p)).href;

// The `town` dispatcher imports every tools/town/*.mjs to list the steps; this file does its work only when run directly.
async function run() {
  let poi;
  let map;
  try {
    poi = await import(url("tools", "town", "lib", "poi-check.mjs"));
    map = await import(url("tools", "town", "map.mjs"));
  } catch (e) {
    fail(`cannot load tools/town/lib/poi-check.mjs: ${e.code ?? e.message}`);
    console.log(`SUMMARY poi-check-selftest fail ${failed}`);
    process.exit(1);
  }

  const town = map.loadTemplate("town", ROOT);
  const village = map.loadTemplate("village", ROOT);
  const { kinds } = poi.loadPoiKinds(ROOT);

  // ---- the POI table: core conf plus the pack overlay
  for (const k of ["Bank", "PoliceStation", "HospitalSite", "Fence"]) check(kinds.has(k), `the POI table lacks kind ${k} (core conf or pack overlay)`);
  const parsed = poi.parsePoiKinds('LF_MapPoiConfig {\n m_aKinds {\n  LF_MapPoiKind "{AAAAAAAAAAAAAAAA}" {\n   m_sKind "Foo"\n   m_sCategory "civic"\n  }\n  LF_MapPoiKind "{BBBBBBBBBBBBBBBB}" {\n   m_sKind "Bar"\n  }\n }\n}\n');
  check(parsed.join(",") === "Foo,Bar", `parsePoiKinds read ${JSON.stringify(parsed)}, expected Foo,Bar`);

  // ---- the committed templates are sound
  for (const t of [town, village]) {
    const probs = poi.checkSlotPois(t, kinds);
    check(probs.length === 0, `template ${t.kind} fails its POI check: ${probs.join("; ")}`);
    for (const s of t.slots) check(typeof s.poi === "string" && s.poi, `template ${t.kind} slot ${s.id} names no "poi"`);
  }

  // ---- planted failures
  const bank = (t) => t.slots.find((s) => s.id === "bank");
  const bad = (mutate) => {
    const t = clone(town);
    mutate(t);
    return poi.checkSlotPois(t, kinds);
  };
  check(bank(town).poi === "Bank", `the bank slot's poi is ${bank(town).poi}, expected Bank`);
  const noRow = bad((t) => {
    bank(t).poi = "Vault9";
    bank(t).places.push("Vault9");
  });
  check(noRow.length > 0, "a slot whose front kind has no POI row passes (the acceptance)");
  check(noRow.some((p) => p.includes("bank") && p.includes("Vault9") && p.includes("none")), `the failure does not name the slot, the kind and the way out: ${noRow.join("; ")}`);
  check(bad((t) => delete bank(t).poi).length > 0, "a slot with no poi passes");
  check(bad((t) => (bank(t).poi = "")).length > 0, "a slot with an empty poi passes");
  check(bad((t) => (bank(t).poi = 7)).length > 0, "a slot with a numeric poi passes");
  check(bad((t) => (bank(t).poi = "none")).length === 0, "an explicit none fails");
  check(bad((t) => (bank(t).poi = "HospitalSite")).length > 0, "a front kind the slot does not register passes");
  check(bad((t) => (bank(t).poi = "NONE")).length > 0, "'NONE' counts as a kind with no row, not as the explicit none");
  check(poi.checkSlotPois(clone(town), new Set()).length > 0, "an empty POI table passes a template that names kinds");

  // ---- `town map <slug> --check` carries it (a temp root: the confs, the templates, the fixture dossier, a mapping)
  const tmp = mkdtempSync(join(tmpdir(), "lf-poi-check-"));
  try {
    for (const rel of [join("addons", "LifeFramework", "Configs", "Map"), join("addons", "LF_Everon1989", "Configs", "Map")]) {
      mkdirSync(join(tmp, rel), { recursive: true });
      cpSync(join(ROOT, rel), join(tmp, rel), { recursive: true });
    }
    mkdirSync(join(tmp, "tools", "town", "templates"), { recursive: true });
    for (const k of ["town", "village"]) cpSync(join(ROOT, "tools", "town", "templates", `${k}.json`), join(tmp, "tools", "town", "templates", `${k}.json`));
    const dossier = JSON.parse(readFileSync(join(ROOT, "tools", "town", "fixtures", "map-dossier.json"), "utf8"));
    const dossierFile = join(tmp, "dossier.json");
    const mappingFile = join(tmp, "mapping.json");
    writeFileSync(dossierFile, JSON.stringify(dossier));
    writeFileSync(mappingFile, JSON.stringify(map.proposeMapping(dossier, town)));
    const run = async () => {
      const lines = [];
      const log = console.log;
      console.log = (...a) => lines.push(a.join(" "));
      let code;
      try {
        code = await map.main([dossier.place.slug, "--check", "--template", "town", "--dossier", dossierFile, "--out", mappingFile], { root: tmp });
      } finally {
        console.log = log;
      }
      return { code, out: lines.join("\n") };
    };
    const tplFile = join(tmp, "tools", "town", "templates", "town.json");
    let r = await run();
    check(r.code === 0, `--check on the committed template exits ${r.code}: ${r.out}`);
    const t = clone(town);
    bank(t).poi = "Vault9";
    bank(t).places.push("Vault9");
    writeFileSync(tplFile, JSON.stringify(t));
    r = await run();
    check(r.code === 1, `--check on a slot whose front kind has no POI row exits ${r.code}, expected 1`);
    check(/slot bank/.test(r.out) && /Vault9/.test(r.out), `--check does not say which slot and kind: ${r.out}`);
    const t2 = clone(town);
    delete bank(t2).poi;
    writeFileSync(tplFile, JSON.stringify(t2));
    r = await run();
    check(r.code === 1 && /slot bank/.test(r.out), `--check on a slot with no poi exits ${r.code}: ${r.out}`);
  } finally {
    rmSync(tmp, { recursive: true, force: true });
  }

  if (failed) {
    console.log(`SUMMARY poi-check-selftest fail ${failed}`);
    process.exit(1);
  }
  console.log("SUMMARY poi-check-selftest ok");
}

if (process.argv[1] && pathToFileURL(resolve(process.argv[1])).href === import.meta.url) await run();

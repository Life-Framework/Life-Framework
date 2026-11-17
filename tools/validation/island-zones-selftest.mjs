// tools/validation/island-zones-selftest.mjs
// inputs: tools/town/**, tools/validation/validate-island-zones.mjs, tools/perf/building-count.mjs, tools/commands/town.mjs
//
// Proves `tools\cli town check <place>` and the `validate-island-zones` guard (docs/architecture/everon-world.md 3.4, 3.5, card #2206)
// on the fixture places in tools/town/fixtures/check-place/ (an invented place from map-dossier.json, no real coordinate): a complete
// place, a missing required slot, a mapped slot without layer and claim, an entity outside its footprint, a double claim, a slot whose
// layer lacks the kind it promises, a layer over the performance budget, and a branch that is not a town-draft. Each case's
// expect.json lists the messages that must appear (substrings); a case with ok true must have no error at all.
// Also: the claim's `destruction.mode: "needs-hook"` is a warning and never an error; the per-place limits within 60 m;
// the guard lets a town-draft branch change only its own layer; the repo as it stands passes validate-island-zones.
// red-proof: before tools/town/check.mjs existed this printed "FAIL cannot load tools/town/check.mjs: ERR_MODULE_NOT_FOUND" (2026-11-17).
// red-proof: with the footprint test left out of checkClaimsAndFootprints() this printed "FAIL outside-footprint: no error contains 'Fit_shop_Stray stands 26 m outside building b03'" (2026-11-17).

import { spawnSync } from "node:child_process";
import { existsSync, readFileSync, readdirSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const FIX = join(ROOT, "tools", "town", "fixtures");
const CASES = join(FIX, "check-place");
let failed = 0;
const fail = (msg) => {
  console.log(`FAIL  ${msg}`);
  failed++;
};
const check = (cond, msg) => {
  if (!cond) fail(msg);
};

let mod;
try {
  mod = await import(pathToFileURL(join(ROOT, "tools", "town", "check.mjs")).href);
} catch (e) {
  fail(`cannot load tools/town/check.mjs: ${e.code ?? e.message}`);
  console.log(`SUMMARY island-zones-selftest fail ${failed}`);
  process.exit(1);
}
const { checkPlace, guardIslandLayers, loadPlace, placeOver, distToPolygon, catalogPlaces, marginOf, LAYER_DIR } = mod;

// The performance counts read prefab text; nothing here touches the game paks. Seven lights in one prefab, an empty prop otherwise.
const lamps = `GenericEntity {\n${Array.from({ length: 7 }, (_, i) => ` LightEntity L${i} {\n  CastShadow 1\n  Type Point\n }`).join("\n")}\n}\n`;
const resolve_ = (ref) => ({ text: /Lamps7/.test(ref) ? lamps : "GenericEntity {\n}\n", where: "fixture" });

const has = (list, sub) => list.some((m) => m.includes(sub));

// ------------------------------------------------------------------ the fixture places
const names = existsSync(CASES) ? readdirSync(CASES, { withFileTypes: true }).filter((d) => d.isDirectory()).map((d) => d.name).sort() : [];
check(names.length >= 8, `check-place holds ${names.length} case(s), expected at least 8`);
for (const name of names) {
  const dir = join(CASES, name);
  const expect = JSON.parse(readFileSync(join(dir, "expect.json"), "utf8"));
  let data;
  try {
    data = loadPlace(dir, "sample-square", {
      dossierFile: join(FIX, "map-dossier.json"),
      mappingFile: join(dir, "mapping.json"),
      templateFile: join(CASES, "template.json"),
      catalogFile: join(CASES, "catalog.conf"),
      layerDir: "layers",
      claimsDir: "claims",
    });
  } catch (e) {
    fail(`${name}: cannot load the case: ${e.message}`);
    continue;
  }
  const r = checkPlace(data, { resolve: resolve_ });
  check(r.ok === expect.ok, `${name}: expected ${expect.ok ? "pass" : "fail"}, got ${r.ok ? "pass" : "fail"}: ${r.errors.join(" | ")}`);
  if (expect.ok) check(r.errors.length === 0, `${name}: expected no error, got ${r.errors.join(" | ")}`);
  for (const sub of expect.errors ?? []) check(has(r.errors, sub), `${name}: no error contains '${sub}' (errors: ${r.errors.join(" | ") || "none"})`);
  for (const sub of expect.warnings ?? []) check(has(r.warnings, sub), `${name}: no warning contains '${sub}' (warnings: ${r.warnings.join(" | ") || "none"})`);
  const cj = join(dir, "case.json");
  if (existsSync(cj)) {
    const c = JSON.parse(readFileSync(cj, "utf8"));
    const g = guardIslandLayers({ branch: c.branch, files: c.files });
    for (const sub of c.guardErrors ?? []) check(has(g.errors, sub), `${name}: the guard on ${c.branch} reports nothing like '${sub}' (got ${g.errors.join(" | ") || "none"})`);
  }
}

// A needs-hook claim only warns; a fitted shop stays a pass.
{
  const dir = join(CASES, "complete");
  const data = loadPlace(dir, "sample-square", { dossierFile: join(FIX, "map-dossier.json"), mappingFile: join(dir, "mapping.json"), templateFile: join(CASES, "template.json"), catalogFile: join(CASES, "catalog.conf"), layerDir: "layers", claimsDir: "claims" });
  const r = checkPlace(data, { resolve: resolve_ });
  check(r.ok && has(r.warnings, "needs-hook"), `complete: the needs-hook claim should be a warning on a passing place (ok ${r.ok}, warnings ${r.warnings.join(" | ")})`);
  check(has(r.notes, "#2245"), "complete: the notes should say the POI-kind check waits on #2245");
  // A kind no system lists is a warning, not an error.
  const odd = { ...data, catalogKinds: new Set(["Spawn"]) };
  const r2 = checkPlace(odd, { resolve: resolve_ });
  check(r2.ok && has(r2.warnings, "kind StoreCounter is in no system's m_aPlaces"), `complete: a kind missing from m_aPlaces should warn only (ok ${r2.ok})`);
}

// ------------------------------------------------------------------ the island guard
{
  const shop = `${LAYER_DIR}/Place_sample-square_shop.layer`;
  const arrival = `${LAYER_DIR}/Place_sample-square_arrival.layer`;
  const g = (branch, files) => guardIslandLayers({ branch, files });
  check(g("ws/fix-bug", [shop]).errors.length === 1, "the guard should refuse a ws/fix-bug branch that changes an island layer");
  check(g("ws/town-pass-x", [shop]).errors.length === 1, "the guard should refuse a ws/town- branch that is not a town-draft");
  check(g("ws/town-draft-sample-square-shop", [shop]).errors.length === 0, "the guard should let ws/town-draft-sample-square-shop change its own layer");
  check(g("ws/town-draft-sample-square-shop", [arrival]).errors.length === 1, "the guard should refuse a town-draft branch that changes another slot's layer");
  check(g("ws/town-draft-saint-philippe-general-store", [`${LAYER_DIR}/Place_saint-philippe_general-store.layer`]).errors.length === 0, "hyphens in the place and the slot must not confuse the guard");
  check(g("ws/town-draft-saint-philippe-general-store", [`${LAYER_DIR}/Place_saint-philippe_clothing.layer`]).errors.length === 1, "a town-draft branch of one slot must not change a sibling's layer");
  check(g("ws/fix-bug", ["docs/features/world.md", `${LAYER_DIR}/Gamemode.layer`]).errors.length === 0, "the guard touches only Place_*.layer files");
  check(g("main", [shop]).errors.length === 0 && g("HEAD", [shop]).errors.length === 0, "main and a detached HEAD are not checked");
}

// ------------------------------------------------------------------ small pieces
{
  check(catalogPlaces('m_aPlaces {\n "A"\n "B"\n}\nm_aPlaces {\n "C"\n}').size === 3, "catalogPlaces should read every m_aPlaces list");
  const sq = [[0, 0], [10, 0], [10, 10], [0, 10]];
  check(distToPolygon(5, 5, sq) === 0 && distToPolygon(13, 5, sq) === 3 && Math.abs(distToPolygon(13, 14, sq) - 5) < 1e-9, "distToPolygon: inside 0, beside 3, off a corner 5");
  check(marginOf({ id: "shop", host: "building" }, { building: "b1" }) === 2 && marginOf({ id: "board", host: "street" }, { building: "b1" }) === 10 && marginOf({ id: "x", host: "street", margin: 4 }, {}) === 4, "marginOf: building 2, board 10, a template margin wins");
  const at = (x, lights, shadow, entities) => ({ slot: `s${x}`, centre: [x, 0], lights, shadow, entities });
  check(placeOver([at(0, 12, 6, 700), at(40, 12, 6, 700)]).length === 0, "placeOver: 24 lights, 12 shadow casters, 1,400 entities in 60 m are at the limit, not over");
  const over = placeOver([at(0, 13, 7, 800), at(40, 12, 6, 800)]);
  check(over.length === 3 && has(over, "lights switched on at night 25 > 24") && has(over, "shadow casters 13 > 12") && has(over, "entities added 1600 > 1500"), `placeOver: three limits expected, got ${over.join(" | ")}`);
  check(placeOver([at(0, 20, 0, 0), at(200, 20, 0, 0)]).length === 0, "placeOver: two slots 200 m apart do not share a view");
}

// ------------------------------------------------------------------ the repo as it stands
{
  const r = spawnSync(process.execPath, [join(ROOT, "tools", "validation", "validate-island-zones.mjs")], { cwd: ROOT, encoding: "utf8", windowsHide: true });
  check(r.status === 0, `validate-island-zones should pass on this checkout: ${(r.stdout || r.stderr).trim().split("\n").slice(0, 4).join(" / ")}`);
}

console.log(failed ? `SUMMARY island-zones-selftest fail ${failed}` : "SUMMARY island-zones-selftest ok");
process.exit(failed ? 1 : 0);

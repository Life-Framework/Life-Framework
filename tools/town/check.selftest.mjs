// tools/town/check.selftest.mjs - acceptance for slice #2937 (town check knows the working component of a slot's kinds).
//
// A fitted slot that registers a kind (`LF_PlaceComponent` m_sKind) with no working component behind it switches its system on and leaves
// it with nothing to run: the police slot promised JailCell with a marker and no cell, the clothing slot had mannequins and no
// `LF_ShopComponent`, and `town check` passed both. `BACKED_KINDS` (tools/town/check.mjs) names, per kind, the component the system reads;
// the slot's layer must carry it, inline or through a prefab it places. With --complete it is an error, otherwise a warning (a draft
// fits one slot at a time; the maintainer's own edits are never blocked). Run: node tools/town/check.selftest.mjs
//   - a clothing slot of mannequins and no shop component fails --complete and names LF_ShopComponent (the acceptance);
//   - the same slot is only a warning without --complete;
//   - the component inline in the layer, or inside a placed prefab (through a parent prefab too), passes;
//   - a kind that needs no component (a clerk is a resident spawned at the marker) passes with the marker alone;
//   - with no prefab resolver the check is skipped with a note, not failed;
//   - a jail slot of a marker and no LF_JailCellComponent fails --complete (the #2975 case);
//   - the cell may sit in a companion slot that rides in the promiser's building (`with: police`, the jail beside the police desk, #3099);
//   - the committed Saint-Philippe place loads and checks without throwing.
// red-proof: before BACKED_KINDS and the component check existed this printed "FAIL clothing [complete]: expected fail, got pass" (2026-11-18).
// red-proof: before a companion slot counted, the "police [cell in the jail slot that rides with it]" case printed FAIL expecting a pass (2026-11-18).
// red-proof: with the prefab walk reduced to the layer's own text this printed "FAIL clothing [shop in a prefab]: expected pass" (2026-11-18).

import { readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const FIX = join(ROOT, "tools", "town", "fixtures");
const CASES = join(FIX, "check-place");
let failed = 0;
const check = (cond, msg) => {
  if (!cond) {
    console.log(`FAIL  ${msg}`);
    failed++;
  }
};
const has = (list, sub) => list.some((m) => m.includes(sub));

// The `town` dispatcher imports every tools/town/*.mjs to list its steps: run only when launched directly (as poi-check.selftest.mjs does).
if (process.argv[1] && pathToFileURL(resolve(process.argv[1])).href === import.meta.url) {
const mod = await import(pathToFileURL(join(ROOT, "tools", "town", "check.mjs")).href);
const { checkPlace, loadPlace, BACKED_KINDS } = mod;
check(BACKED_KINDS && BACKED_KINDS.ClothingCounter === "LF_ShopComponent", "BACKED_KINDS must name LF_ShopComponent for ClothingCounter");
check(BACKED_KINDS && BACKED_KINDS.JailCell === "LF_JailCellComponent", "BACKED_KINDS must name LF_JailCellComponent for JailCell");

// ------------------------------------------------------------------ a fixture place whose `shop` slot becomes the one under test
const dir = join(CASES, "complete");
const base = loadPlace(dir, "sample-square", {
  dossierFile: join(FIX, "map-dossier.json"),
  mappingFile: join(dir, "mapping.json"),
  templateFile: join(CASES, "template.json"),
  catalogFile: join(CASES, "catalog.conf"),
  poiKinds: new Set(["StoreCounter"]),
  layerDir: "layers",
  claimsDir: "claims",
});

// slot: the slot id; kind: the LF_Places kind it promises; extra: layer text after the marker (the entities the kit placed).
function place(slot, kind, extra) {
  const marker = `GenericEntity Fit_${slot}_Place {\n components {\n  LF_PlaceComponent "{AAAAAAAAAAAAAAA1}" {\n   m_sKind "${kind}"\n   m_sId "sample-square.${slot}"\n  }\n }\n coords 980 12.4 2015\n}\n`;
  const claim = { ...base.claims["sample-square-shop.json"], slot, layer: `addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers/Place_sample-square_${slot}.layer` };
  const { shop, ...restMapping } = base.mapping.slots;
  const claims = Object.fromEntries(Object.entries(base.claims).filter(([n]) => n !== "sample-square-shop.json"));
  const layers = Object.fromEntries(Object.entries(base.layers).filter(([n]) => n !== "Place_sample-square_shop.layer"));
  return {
    ...base,
    mapping: { ...base.mapping, slots: { ...restMapping, [slot]: shop } },
    template: { ...base.template, slots: base.template.slots.map((s) => (s.id === "shop" ? { ...s, id: slot, places: [kind], poi: "none" } : s)) },
    claims: { ...claims, [`sample-square-${slot}.json`]: claim },
    layers: { ...layers, [`Place_sample-square_${slot}.layer`]: marker + extra },
  };
}

const mannequins = ["denim", "knit", "suit"].map((n, i) => `GenericEntity Fit_clothing_Mannequin_${n} : "{F55D2611E095EBA${i}}Prefabs/Props/Clothing/LF_Mannequin_${n}.et" {\n coords ${981 + i} 12.4 2014\n}\n`).join("");
const shopBlock = `GenericEntity Fit_clothing_Till {\n components {\n  LF_ShopComponent "{6EA70364DEB62E72}" {\n   m_sOutletId "clothing"\n  }\n }\n coords 982 12.4 2015\n}\n`;
const counter = `GenericEntity Fit_clothing_Counter : "{E214337C6E1931C5}Prefabs/Props/Commercial/CounterShop_01.et" {\n coords 982 12.4 2014\n}\n`;

// Prefab text by path: a mannequin and the counter carry no component; Stall.et inherits Till.et, which has the shop component.
const PREFABS = {
  "Prefabs/Props/Clothing/LF_Mannequin_denim.et": "GenericEntity {\n components {\n  MeshObject \"{59151C6A5551D485}\" {\n  }\n }\n}\n",
  "Prefabs/Props/Commercial/CounterShop_01.et": "GenericEntity {\n}\n",
  "Prefabs/Props/Commercial/Till.et": `GenericEntity {\n components {\n  LF_ShopComponent "{6EA70364DEB62E72}" {\n  }\n }\n}\n`,
  "Prefabs/Props/Commercial/Stall.et": `GenericEntity : "{1111111111111111}Prefabs/Props/Commercial/Till.et" {\n}\n`,
  // A prefab that only NAMES another in a property is not placing it: the component behind it does not count.
  "Prefabs/Props/Commercial/Catalogue.et": `GenericEntity {\n components {\n  ActionsManagerComponent "{2222222222222222}" {\n   m_sSpawn "{1111111111111111}Prefabs/Props/Commercial/Till.et"\n  }\n }\n}\n`,
};
const resolve_ = (ref) => {
  const text = PREFABS[ref.replace(/^\{[0-9A-Fa-f]{16}\}/, "")];
  return { text: text ?? "GenericEntity {\n}\n", where: "fixture" };
};

// ------------------------------------------------------------------ the acceptance: mannequins and no shop component
{
  const data = place("clothing", "ClothingCounter", mannequins + counter);
  const r = checkPlace(data, { resolve: resolve_, complete: true });
  check(!r.ok, `clothing [complete]: expected fail, got pass (errors: ${r.errors.join(" | ") || "none"})`);
  check(has(r.errors, "LF_ShopComponent") && has(r.errors, "slot clothing"), `clothing [complete]: no error names slot clothing and LF_ShopComponent (errors: ${r.errors.join(" | ") || "none"})`);
  const p = checkPlace(data, { resolve: resolve_ });
  check(p.ok && p.errors.length === 0, `clothing [partial]: a maintainer's draft must not fail (errors: ${p.errors.join(" | ") || "none"})`);
  check(has(p.warnings, "LF_ShopComponent"), `clothing [partial]: expected a warning naming LF_ShopComponent (warnings: ${p.warnings.join(" | ") || "none"})`);
}

// ------------------------------------------------------------------ the component is there: inline, in a prefab, in a parent prefab
{
  const inline = checkPlace(place("clothing", "ClothingCounter", mannequins + shopBlock), { resolve: resolve_, complete: true });
  check(inline.ok && !has(inline.warnings, "LF_ShopComponent"), `clothing [shop inline]: expected pass (errors: ${inline.errors.join(" | ")}; warnings: ${inline.warnings.join(" | ")})`);
  const till = `GenericEntity Fit_clothing_Till : "{3333333333333333}Prefabs/Props/Commercial/Till.et" {\n coords 982 12.4 2015\n}\n`;
  const viaPrefab = checkPlace(place("clothing", "ClothingCounter", mannequins + till), { resolve: resolve_, complete: true });
  check(viaPrefab.ok, `clothing [shop in a prefab]: expected pass, got ${viaPrefab.errors.join(" | ")}`);
  const stall = `GenericEntity Fit_clothing_Stall : "{4444444444444444}Prefabs/Props/Commercial/Stall.et" {\n coords 982 12.4 2015\n}\n`;
  const viaParent = checkPlace(place("clothing", "ClothingCounter", mannequins + stall), { resolve: resolve_, complete: true });
  check(viaParent.ok, `clothing [shop in a parent prefab]: expected pass, got ${viaParent.errors.join(" | ")}`);
  const named = `GenericEntity Fit_clothing_Catalogue : "{5555555555555555}Prefabs/Props/Commercial/Catalogue.et" {\n coords 982 12.4 2015\n}\n`;
  const viaName = checkPlace(place("clothing", "ClothingCounter", mannequins + named), { resolve: resolve_, complete: true });
  check(!viaName.ok && has(viaName.errors, "LF_ShopComponent"), `clothing [prefab only named in a property]: expected fail, got ${viaName.ok ? "pass" : viaName.errors.join(" | ")}`);
}

// ------------------------------------------------------------------ no component needed, no resolver, the jail
{
  const clerk = checkPlace(place("desk", "Clerk", counter), { resolve: resolve_, complete: true });
  check(clerk.ok && !has(clerk.warnings, "Component"), `clerk: a marker alone is enough (errors: ${clerk.errors.join(" | ")}; warnings: ${clerk.warnings.join(" | ")})`);
  const blind = checkPlace(place("clothing", "ClothingCounter", mannequins), { resolve: null, complete: true });
  check(blind.ok && has(blind.notes, "LF_ShopComponent"), `no resolver: expected a pass with a note naming LF_ShopComponent (ok ${blind.ok}; notes: ${blind.notes.join(" | ") || "none"})`);
  const jail = place("police", "JailCell", counter);
  const j = checkPlace(jail, { resolve: resolve_, complete: true });
  check(!j.ok && has(j.errors, "LF_JailCellComponent") && has(j.errors, "slot police"), `police [complete]: expected an error naming slot police and LF_JailCellComponent (errors: ${j.errors.join(" | ") || "none"})`);
  const cell = `GenericEntity Fit_police_Cell {\n components {\n  LF_JailCellComponent "{6666666666666666}" {\n  }\n }\n coords 983 12.4 2015\n}\n`;
  check(checkPlace(place("police", "JailCell", cell), { resolve: resolve_, complete: true }).ok, "police [cell inline]: expected pass");
}

// ------------------------------------------------------------------ the cell in a companion slot (#3099): the jail rides in the police building
{
  // A second slot in the same place, mapped to the host's building (a street slot may share it), its layer and claim beside the host's.
  const ride = (host, slot, kind, extra, tweak = {}) => {
    const b = place(slot, kind, extra);
    const entry = { ...b.mapping.slots[slot], shared: true };
    const tslot = { ...b.template.slots.find((s) => s.id === slot), host: "street", ...tweak };
    return {
      ...host,
      mapping: { ...host.mapping, slots: { ...host.mapping.slots, [slot]: entry } },
      template: { ...host.template, slots: [...host.template.slots, tslot] },
      claims: { ...host.claims, [`sample-square-${slot}.json`]: { ...b.claims[`sample-square-${slot}.json`], shared: true } },
      layers: { ...host.layers, [`Place_sample-square_${slot}.layer`]: b.layers[`Place_sample-square_${slot}.layer`] },
    };
  };
  const cellOf = (slot) => `GenericEntity Fit_${slot}_Cell {
 components {
  LF_JailCellComponent "{6666666666666666}" {
  }
 }
 coords 983 12.4 2015
}
`;
  const host = place("police", "JailCell", counter);
  const riding = checkPlace(ride(host, "jail", "JailCell", cellOf("jail"), { with: "police" }), { resolve: resolve_, complete: true });
  check(riding.ok, `police [cell in the jail slot that rides with it]: expected pass (errors: ${riding.errors.join(" | ") || "none"})`);
  const apart = checkPlace(ride(host, "jail", "JailCell", cellOf("jail")), { resolve: resolve_, complete: true });
  check(!apart.ok && has(apart.errors, "slot police") && has(apart.errors, "LF_JailCellComponent"), `police [a jail slot that does not ride with it]: expected an error naming slot police (errors: ${apart.errors.join(" | ") || "none"})`);
  const other = checkPlace(ride(host, "jail", "JailCell", cellOf("jail"), { with: "bank" }), { resolve: resolve_, complete: true });
  check(!other.ok && has(other.errors, "slot police"), `police [a jail slot that rides with another slot]: expected an error naming slot police (errors: ${other.errors.join(" | ") || "none"})`);
  const bare = checkPlace(ride(host, "jail", "JailCell", counter, { with: "police" }), { resolve: resolve_, complete: true });
  check(!bare.ok && has(bare.errors, "slot police"), `police [the riding jail slot has no cell either]: expected an error naming slot police (errors: ${bare.errors.join(" | ") || "none"})`);
}

// ------------------------------------------------------------------ the committed Saint-Philippe place loads and checks
{
  let ok = true;
  try {
    const data = loadPlace(ROOT, "saint-philippe");
    const r = checkPlace(data, { resolve: (ref) => {
      try {
        return { text: readFileSync(join(ROOT, "addons", "LifeFramework", ref.replace(/^\{[0-9A-Fa-f]{16}\}/, "")), "utf8"), where: "repo" };
      } catch {
        return null;
      }
    } });
    check(Array.isArray(r.warnings) && Array.isArray(r.errors), "saint-philippe: checkPlace returned no lists");
  } catch (e) {
    ok = false;
    check(false, `saint-philippe: threw ${e.message}`);
  }
  void ok;
}

console.log(`SUMMARY town-check-selftest ${failed ? "fail" : "ok"} ${failed}`);
process.exit(failed ? 1 : 0);
}

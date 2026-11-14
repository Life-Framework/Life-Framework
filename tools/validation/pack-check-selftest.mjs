// tools/validation/pack-check-selftest.mjs
// inputs: tools/**, addons/**, docs/architecture/**
//
// Proves `tools\cli pack check` and `pack explain` (#969) on a fixture server: a pack with one error
// of each kind is reported with its file and id; a clean pack passes; the explain view names core,
// pack and owner as the source of the right value; every list section in the core has a row in
// tools/pack/layers.mjs; and the real flagship pack passes its own check.
// red-proof: with the "verb on an id no lower layer has" refusal in tools/pack/layers.mjs mergeList
// disabled this crashed with "TypeError: Cannot set properties of undefined (setting 'present')"
// and exited 1 (2026-11-14). #553: before the fixture's core class sorted ahead of the stub the clash case printed
// "FAIL pack check: a pack section named like a core section must be an error" and "FAIL schema: a refused pack
// section must stay out"; a ShopsBad default or a wrong explain path printed one FAIL each (observed 2026-11-14).

import { mkdirSync, mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { checkPack, summary } from "../pack/check.mjs";
import { explain } from "../pack/explain.mjs";
import { SECTIONS, walk } from "../pack/layers.mjs";
import { checkText } from "../config/check.mjs";
import { coreKeysOf, generate, readScripts } from "../config/schema.mjs";
import { loadPackSections } from "../config/pack-sections.mjs";

const REAL = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
let failed = 0;
const fail = (msg) => { failed++; console.log(`FAIL ${msg}`); };

function put(root, path, text) {
  const full = join(root, path);
  mkdirSync(dirname(full), { recursive: true });
  writeFileSync(full, text);
}
const meta = (guid, path) => `MetaFileClass {\n Name "{${guid}}${path}"\n}\n`;
const resource = (root, addon, path, guid, text) => { put(root, `addons/${addon}/${path}`, text); put(root, `addons/${addon}/${path}.meta`, meta(guid, path)); };

const G = { man: "1000000000000001", ov: "1000000000000002", ov2: "1000000000000003", mis: "1000000000000004", pistol: "2000000000000001", rifle: "2000000000000002",
  man2: "3000000000000001", ovB: "3000000000000002", misB: "3000000000000003", ovC: "4000000000000002", manC: "4000000000000001", misC: "4000000000000003" };
const SURFACE = ["LF_PackManifest", "LF_MissionHeader", "LF_ItemCatalogueConfig", "LF_CrimeCatalogConfig", "LF_ShopPricesConfig"];

function pack(root, id, g, manifestText, overlays, extra = () => {}) {
  put(root, `addons/${id}/${id}.gproj`, `GameProject {\n ID "${id}"\n GUID "${g.gproj}"\n}\n`);
  resource(root, id, "Configs/LF_PackManifest.conf", g.man, manifestText);
  for (const [name, guid, text] of overlays.files) resource(root, id, `Configs/${name}`, guid, text);
  resource(root, id, `Missions/${id}.conf`, g.mis,
    `LF_MissionHeader {\n m_sName "${id}"\n m_LF_PackManifest "{${g.man}}Configs/LF_PackManifest.conf"\n m_aLF_ConfigOverlays {\n${overlays.refs.map((r) => `  "${r}"\n`).join("")} }\n}\n`);
  resource(root, id, "Language/pack_localization.st", g.st ?? "5000000000000000", 'StringTable {\n Items {\n  CustomStringTableItem "{A}" {\n   Id "LF_Test-Place"\n  }\n }\n}\n');
  extra();
}

function tree() {
  const root = mkdtempSync(join(tmpdir(), "lf-packcheck-"));
  put(root, "addons/LifeFramework/LifeFramework.gproj", 'GameProject {\n ID "LifeFramework"\n GUID "9000000000000000"\n}\n');
  put(root, "addons/LifeFramework/Scripts/Game/LF_PackManifest.c", 'class LF_PackManifest\n{\n\tstatic const string CORE_VERSION = "0.1.0";\n}\n' + SURFACE.slice(1).map((c) => `class ${c}\n{\n}\n`).join(""));
  put(root, "docs/architecture/public-surface.json", JSON.stringify({ version: 1, configClasses: SURFACE, deprecated: [] }));
  resource(root, "LifeFramework", "Prefabs/Pistol.et", G.pistol, "Resource {\n}\n");
  resource(root, "LifeFramework", "Configs/Catalogue/LF_ItemCatalogue.conf", "5000000000000001",
    `LF_ItemCatalogueConfig {
 m_aItems {
  LF_CatalogueItem "{A1}" {
   m_sId "pistol_m9"
   m_Prefab "{${G.pistol}}Prefabs/Pistol.et"
   m_iPrice 500
   m_sCategory "weapon"
  }
  LF_CatalogueItem "{A2}" {
   m_sId "rifle"
   m_iPrice 900
  }
 }
 m_aOffers {
  LF_OutletOffer "{B1}" {
   m_sOutlet "gun_counter"
   m_sItem "pistol_m9"
  }
 }
}
`);
  resource(root, "LifeFramework", "Configs/Crime/LF_CrimeCatalog.conf", "5000000000000002",
    'LF_CrimeCatalogConfig {\n m_aCrimes {\n  LF_CrimeEntry "{C1}" {\n   m_sCrimeId "#LF-Offence_Robbery"\n   m_iWantedAdd 1\n  }\n }\n}\n');
  resource(root, "LifeFramework", "Configs/Shop/ShopPrices.conf", "5000000000000003",
    `LF_ShopPricesConfig {\n m_iDefaultBuyPrice 50\n m_aPriceRules {\n  LF_ShopPriceRule "{D1}" {\n   m_sFind "bandage"\n   m_iBuyPrice 12\n  }\n }\n}\n`);
  resource(root, "LifeFramework", "Language/core.st", "5000000000000004", 'StringTable {\n Items {\n  CustomStringTableItem "{A}" {\n   Id "LF-Offence_Robbery"\n  }\n }\n}\n');
  return root;
}

const manifest = (id, min = "0.1.0", below = "0.2.0") => `LF_PackManifest {\n m_sPackId "${id}"\n m_sCoreMin "${min}"\n m_sCoreBelow "${below}"\n}\n`;

// ---- The clean fixture pack: a patch, an add, a remove, an owner-free pass ----
const GOOD = `LF_ItemCatalogueConfig {
 m_aItems {
  LF_CatalogueItem "{E1}" {
   m_sOp "patch"
   m_sId "pistol_m9"
   m_iPrice 650
  }
  LF_CatalogueItem "{E2}" {
   m_sOp "add"
   m_sId "hunting_rifle"
   m_Prefab "{${G.pistol}}Prefabs/Pistol.et"
   m_iPrice 1200
  }
  LF_CatalogueItem "{E3}" {
   m_sOp "remove"
   m_sId "rifle"
  }
 }
 m_aOffers {
  LF_OutletOffer "{E4}" {
   m_sOp "add"
   m_sOutlet "gun_counter"
   m_sItem "hunting_rifle"
  }
 }
}
`;

const clean = tree();
const rootsToClean = [clean];
pack(clean, "LF_Test", { gproj: "6000000000000001", man: G.man, mis: G.mis }, manifest("LF_Test"), { files: [["Catalogue.conf", G.ov, GOOD]], refs: [`{${G.ov}}Configs/Catalogue.conf`] });
{
  const r = checkPack(clean, "LF_Test");
  if (r.findings.length) fail(`clean fixture pack reported findings:\n  ${r.findings.map((f) => `${f.area} ${f.file} ${f.id}: ${f.msg}`).join("\n  ")}`);
  else if (!/OK/.test(summary(r).line)) fail(`clean pack summary says ${summary(r).line}`);
}

// ---- One error of each kind in one pack ----
const bad = tree();
rootsToClean.push(bad);
const BAD_OVERLAY = `LF_ItemCatalogueConfig {
 m_aItems {
  LF_CatalogueItem "{F1}" {
   m_sOp "patch"
   m_sId "no_such_item"
   m_iPrice 1
  }
  LF_CatalogueItem "{F2}" {
   m_sOp "add"
   m_sId "rifle"
  }
  LF_CatalogueItem "{F3}" {
   m_sOp "remove"
   m_sId "ghost_item"
  }
  LF_CatalogueItem "{F4}" {
   m_sOp "upsert"
   m_sId "pistol_m9"
  }
  LF_CatalogueItem "{F5}" {
   m_sOp "patch"
   m_sId "pistol_m9"
   m_Prefab "{${G.pistol}}Prefabs/Wrong.et"
  }
  LF_CatalogueItem "{F6}" {
   m_sOp "add"
   m_sId "stale_thing"
   m_Prefab "{7777777777777777}Prefabs/Pistol.et"
  }
 }
 m_aOffers {
  LF_OutletOffer "{F7}" {
   m_sOp "add"
   m_sOutlet "gun_counter"
   m_sItem "not_in_catalogue"
  }
 }
}
`;
const BAD_CRIME = `LF_CrimeCatalogConfig {\n m_aCrimes {\n  LF_CrimeEntry "{G1}" {\n   m_sOp "add"\n   m_sCrimeId "#LF-Offence_Smuggling"\n  }\n }\n}\n`;
const UNLISTED = `LF_ShopPricesConfig {\n}\n`;
pack(bad, "LF_Bad", { gproj: "6000000000000002", man: G.man2, mis: G.misB }, manifest("LF_Bad", "0.5.0", "0.6.0"),
  { files: [["Catalogue.conf", G.ovB, BAD_OVERLAY], ["Crime.conf", G.ovC, BAD_CRIME], ["Unlisted.conf", "6000000000000009", UNLISTED]], refs: [`{${G.ovB}}Configs/Catalogue.conf`, `{${G.ovC}}Configs/Crime.conf`, "{8888888888888888}Configs/Missing.conf"] },
  () => put(bad, "addons/LF_Bad/Scripts/Game/Oops.c", "class LF_Oops\n{\n}\n"));
const planted = [
  ["manifest range excludes the core", /manifest\s+Configs\/LF_PackManifest\.conf.*excludes the running core 0\.1\.0/],
  ["a script in the pack", /boundary.*Oops\.c: script in the pack/],
  ["overlay GUID resolves to nothing", /header\s+Missions\/LF_Bad\.conf.*Missing\.conf resolves to no resource/],
  ["overlay no header loads", /header\s+Configs\/Unlisted\.conf.*not in m_aLF_ConfigOverlays/],
  ["patch of a missing id", /ids\s+Configs\/Catalogue\.conf\s+catalogue\.items no_such_item: patch on an id no lower layer has/],
  ["add of an existing id", /ids\s+Configs\/Catalogue\.conf\s+catalogue\.items rifle: add of an id a lower layer already has/],
  ["remove of a missing id", /ids\s+Configs\/Catalogue\.conf\s+catalogue\.items ghost_item: remove on an id no lower layer has/],
  ["unknown verb", /ids\s+Configs\/Catalogue\.conf\s+catalogue\.items pistol_m9: unknown verb "upsert"/],
  ["offer for a missing item", /ids\s+Configs\/Catalogue\.conf\s+catalogue\.offers gun_counter\/not_in_catalogue: the offer names item "not_in_catalogue"/],
  ["GUID names another path", /guids\s+Configs\/Catalogue\.conf.*Wrong\.et: GUID \{2000000000000001\} is LifeFramework\/Prefabs\/Pistol\.et/],
  ["stale GUID", /guids\s+Configs\/Catalogue\.conf.*7777777777777777.*is in no \.meta, but Prefabs\/Pistol\.et exists/],
  ["missing string key", /strings\s+Configs\/Crime\.conf\s+#LF-Offence_Smuggling: string key LF-Offence_Smuggling is in no string table/],
];
{
  const r = checkPack(bad, "LF_Bad");
  const lines = r.findings.map((f) => `${f.level === "error" ? "ERROR" : "WARN "} ${f.area.padEnd(8)} ${[f.file, f.id].filter(Boolean).join("  ")}: ${f.msg}`);
  for (const [name, re] of planted) if (!lines.some((l) => re.test(l))) fail(`planted error '${name}': nothing matched ${re}\n  got:\n  ${lines.join("\n  ")}`);
  if (!summary(r).errors) fail("a pack with errors summarised as clean");
}

// ---- Two packs on one id: a warning naming both ----
const two = tree();
rootsToClean.push(two);
pack(two, "LF_Aaa", { gproj: "6000000000000003", man: G.man, mis: G.mis }, manifest("LF_Aaa"), { files: [["Catalogue.conf", G.ov, GOOD]], refs: [`{${G.ov}}Configs/Catalogue.conf`] });
pack(two, "LF_Bbb", { gproj: "6000000000000004", man: G.man2, mis: G.misB }, manifest("LF_Bbb"), { files: [["Catalogue.conf", G.ovB, GOOD.replace('m_sOp "add"\n   m_sId "hunting_rifle"', 'm_sOp "patch"\n   m_sId "pistol_m9"').replace("m_iPrice 1200", "m_iPrice 700")]], refs: [`{${G.ovB}}Configs/Catalogue.conf`] });
{
  const r = checkPack(two, "LF_Bbb");
  if (!r.findings.some((f) => f.level === "warn" && f.area === "packs" && /LF_Bbb changes an id LF_Aaa already changed/.test(f.msg))) fail(`two packs on one id: no conflict warning, got ${JSON.stringify(r.findings)}`);
}

// ---- Explain: the right source for a value set by core, pack and owner ----
{
  const owner = join(clean, "owner.json");
  writeFileSync(owner, JSON.stringify({ catalogue: { items: { pistol_m9: { op: "patch", sellPrice: 111 } } }, shops: { defaultBuyPrice: 75 } }));
  const get = (key, opts = {}) => explain(clean, key, { config: owner, ...opts });
  const must = (name, r, re) => { if (!r.lines.some((l) => re.test(l))) fail(`explain ${name}: nothing matched ${re}\n  got:\n  ${r.lines.join("\n  ")}`); };
  const whole = get("catalogue.items.pistol_m9");
  must("core field", whole, /category = "weapon"\s+<- core$/);
  must("pack field", whole, /price = 650\s+<- pack LF_Test \(Configs\/Catalogue\.conf\)\s+\(overridden: core 500\)/);
  must("owner field", whole, /sellPrice = 111\s+<- owner owner\.json/);
  must("trail", whole, /trail: core add > LF_Test patch > owner patch/);
  must("one field", get("catalogue.items.pistol_m9.price"), /price = 650/);
  if (get("catalogue.items.pistol_m9.price").lines.some((l) => /category/.test(l))) fail("explain with a field also listed other fields");
  must("pack-added id", get("catalogue.items.hunting_rifle.price"), /price = 1200\s+<- pack LF_Test/);
  must("removed id", get("catalogue.items.rifle"), /rifle: removed by pack LF_Test/);
  must("scalar owner over core", get("shops.defaultBuyPrice"), /shops\.defaultBuyPrice = 75\s+<- owner owner\.json/);
  must("scalar overridden", get("shops.defaultBuyPrice"), /overridden: 50 from core/);
  must("list view", get("catalogue.items"), /pistol_m9\s+core add > LF_Test patch > owner patch/);
  const missing = get("catalogue.items.nope_item");
  if (missing.code !== 1) fail(`explain of an unknown id exited ${missing.code}, expected 1`);
  if (get("crime.crimes.#LF-Offence_Robbery.wantedAdd").code !== 0) fail("explain of a crime field with # in its id failed");
  if (get("nonsense").code !== 2) fail("explain of a bad key did not exit 2");
}

// ---- Every list section in the core is in the table ----
{
  for (const f of walk(join(REAL, "addons", "LifeFramework", "Scripts")).filter((p) => p.endsWith(".c"))) {
    const text = readFileSync(f, "utf8");
    if (!/classs+w+s*:s*LF_ConfigList/.test(text)) continue;
    for (const m of text.matchAll(/override string GetName()s*{s*return "([^"]+)";/g)) {
      if (!SECTIONS[m[1]]) fail(`${f.slice(REAL.length + 1)} declares the list ${m[1]}; add its row to SECTIONS in tools/pack/layers.mjs`);
    }
  }
}

// ---- Pack config sections (#553): declared in the manifest, merged into the schema, checked, explained ----
{
  const root = tree();
  rootsToClean.push(root);
  put(root, "addons/LifeFramework/Scripts/Game/AA_ShopsConfig.c", '[BaseContainerProps(configRoot: true)]\nclass LF_ShopPricesConfig\n{\n\tstatic const string SECTION = "shops";\n}\n');
  const field = (key, type, def, min = "", max = "") => `    LF_PackConfigField "{F${key}}" {\n     m_sKey "${key}"\n     m_sType "${type}"\n     m_sDefault "${def}"\n${min ? `     m_sMin "${min}"\n` : ""}${max ? `     m_sMax "${max}"\n` : ""}    }\n`;
  const section = (name, ...fields) => `  LF_PackConfigSection "{S${name}}" {\n   m_sName "${name}"\n   m_aFields {\n${fields.join("")}   }\n  }\n`;
  const text = `LF_PackManifest {\n m_sPackId "LF_Shops"\n m_sCoreMin "0.1.0"\n m_sCoreBelow "0.2.0"\n m_aConfigSections {\n${[
    section("shops", field("limit", "int", "5")),
    section("shopsBad", field("limit", "int", "ten")),
    section("elsewhere", field("limit", "int", "5")),
    section("shopsGood", field("limit", "int", "5", "1", "9"), field("open", "bool", "true")),
  ].join("")} }\n}\n`;
  pack(root, "LF_Shops", { gproj: "6000000000000001", man: "6000000000000002", mis: "6000000000000003", st: "6000000000000004" }, text, { files: [], refs: [] });
  const r = checkPack(root, "LF_Shops");
  const cfg = r.findings.filter((f) => f.area === "config");
  const has = (id, re) => cfg.some((f) => f.id === id && re.test(f.msg));
  if (!has("shops", /core section name/)) fail("pack check: a pack section named like a core section must be an error");
  if (!has("shopsBad", /default ten is not a whole number/)) fail("pack check: a section default that is not its type must be an error");
  if (!has("elsewhere", /does not start with the pack namespace/)) fail("pack check: a section outside the namespace must be an error");
  if (cfg.some((f) => f.id === "shopsGood")) fail("pack check: a valid pack section must not be reported");

  // The merged schema holds the valid section, not the refused ones; config-check names the bad field.
  const addons = join(root, "addons");
  const scripts = readScripts(join(addons, "LifeFramework", "Scripts", "Game"));
  const packs = loadPackSections(addons, coreKeysOf(scripts));
  const { schema } = generate(scripts, "static const int SCHEMA_VERSION = 1;", { records: {}, examples: {} }, packs);
  if (!schema.properties.shopsGood || schema.properties.shopsGood["x-pack"] !== "LF_Shops") fail("schema: the valid pack section is merged with x-pack");
  if (schema.properties.shopsGood?.properties.limit.maximum !== 9) fail("schema: a pack field keeps its range");
  if (schema.properties.shopsBad || schema.properties.elsewhere || schema.properties.shops?.["x-pack"]) fail("schema: a refused pack section must stay out, and a clash must not replace the core section");
  const check = (json) => checkText(json, { schema, loops: false }).problems;
  if (check('{"shopsGood":{"limit":4,"open":false}}').length) fail("config-check: a valid pack section must pass");
  if (!check('{"shopsGood":{"limit":40}}').some((p) => p.level === "error" && /limit/.test(p.msg))) fail("config-check: a pack field above its max must be an error naming the field");
  if (!check('{"shopsGood":{"lmit":4}}').some((p) => /lmit/.test(p.msg))) fail("config-check: an unknown key in a pack section must be reported by name");
  if (!check('{"shopsGood":{"open":"yes"}}').some((p) => p.level === "error" && /open/.test(p.msg))) fail("config-check: a wrong-typed pack field must be an error naming the field");

  // explain: the pack default, then the owner's value, or the refusal.
  put(root, "owner.json", '{"shopsGood":{"limit":7}}');
  put(root, "owner-bad.json", '{"shopsGood":{"limit":70}}');
  const e1 = explain(root, "shopsGood.limit", { config: join(root, "owner.json") }).lines.join("\n");
  if (!e1.includes("= 7  <- owner owner.json") || !e1.includes("overridden: 5 from pack LF_Shops")) fail(`explain: the owner value must win over the pack default, got ${e1}`);
  const e2 = explain(root, "shopsGood.limit", { config: join(root, "owner-bad.json") }).lines.join("\n");
  if (!e2.includes("= 5  <- pack LF_Shops") || !/refused: owner owner-bad.json sets 70/.test(e2)) fail(`explain: a refused owner value must leave the pack default, got ${e2}`);
  const e3 = explain(root, "shopsBad", {});
  if (e3.code !== 1 || !/ignores it/.test(e3.lines[0])) fail(`explain: a refused section must say the server ignores it, got ${e3.lines}`);
}

// ---- The real flagship pack passes its own check ----
{
  const r = checkPack(REAL, "LF_Everon1989");
  if (summary(r).errors) fail(`LF_Everon1989 fails its own pack check:\n  ${r.findings.map((f) => `${f.area} ${f.file} ${f.id}: ${f.msg}`).join("\n  ")}`);
}

for (const r of rootsToClean) rmSync(r, { recursive: true, force: true });
if (failed) {
  console.log(`pack-check-selftest: ${failed} case(s) failed`);
  process.exit(1);
}
console.log("pack-check-selftest: OK (one planted error of each kind reported with file and id; explain names core, pack and owner; sections table complete)");

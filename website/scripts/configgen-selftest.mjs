// Selftest for src/lib/configgen.ts against the real docs/config/config.schema.json.
// Run: node scripts/configgen-selftest.mjs (Node 22.6+ strips the types).
import fs from "node:fs";
import assert from "node:assert/strict";
import { defaultOf, minimalDiff, checkConfig, fieldErrors, importConfig, exportText } from "../src/lib/configgen.ts";
import { parseJson } from "../../tools/config/rules.mjs";
import { checkText } from "../../tools/config/check.mjs";

const schema = JSON.parse(fs.readFileSync(new URL("../../docs/config/config.schema.json", import.meta.url), "utf8"));
const base = () => defaultOf(schema);

assert.deepEqual(minimalDiff(schema, base()), { schemaVersion: 1 }, "defaults produce only schemaVersion");
assert.deepEqual(checkConfig(schema, exportText(schema, base())), [], "defaults validate");

const v = base();
v.shops.defaultBuyPrice = 70;
v.shops.priceRules["Prefabs/Items/Food/"] = { prefab: "", buyPrice: 10, sellPrice: 50, hidden: false };
v.shops.priceRules["Prefabs/Weapons/"] = { prefab: "", buyPrice: 100, sellPrice: 50, hidden: true };
v.shops.priceRules["Prefabs/Other/"] = { prefab: "", buyPrice: 100, sellPrice: 50, hidden: false };
v.shops.extraItemPrefabs = ["Prefabs/X.et"];
assert.deepEqual(minimalDiff(schema, v), {
  schemaVersion: 1,
  shops: {
    defaultBuyPrice: 70,
    priceRules: { "Prefabs/Items/Food/": { buyPrice: 10 }, "Prefabs/Weapons/": { hidden: true }, "Prefabs/Other/": {} },
    extraItemPrefabs: ["Prefabs/X.et"],
  },
});

const fixtures = new URL("../../tools/validation/fixtures/config-check/", import.meta.url);
const fx = (f) => fs.readFileSync(new URL(f, fixtures), "utf8");
const plain = (n) => (n.t === "obj" ? Object.fromEntries(n.props.map((p) => [p.key, plain(p.val)])) : n.t === "arr" ? n.items.map(plain) : n.value);

// #287: the page runs config-check's rules. Same messages for every fixture, the offline-only
// shipped-record-key warning aside (it needs the shipped .conf files, which the page does not have).
const offline = (p) => !p.msg.includes("no shipped record has this key");
for (const f of ["bad.json", "typed.json", "zero.json", "too-new.json", "older.json", "syntax.json", "good.json"]) {
  const web = checkConfig(schema, fx(f), f);
  const cli = checkText(fx(f), { schema, source: f, loops: false }).problems.filter(offline);
  assert.deepEqual(web.map((p) => [p.level, p.line, p.col, p.msg]), cli.map((p) => [p.level, p.line, p.col, p.msg]), `${f}: same messages as config-check`);
}
const badErrors = checkConfig(schema, fx("bad.json"), "bad.json").filter((p) => p.level === "error");
assert.ok(badErrors.length >= 5, "bad.json has errors the page reports");
assert.ok(Object.keys(fieldErrors(badErrors)).includes(".shops.defaultSellPrice"), "an error maps to its field");
assert.equal(checkConfig(schema, fx("good.json")).filter((p) => p.level === "error").length, 0, "good.json is exportable");

// The form path: an out-of-range value typed on the page is an error in the file it would export.
const typed = base();
typed.shops.defaultBuyPrice = -1;
typed.shops.priceRules["Prefabs/Items/Food/"] = { prefab: "", buyPrice: 10, sellPrice: 1.5, hidden: false };
const typedErrs = fieldErrors(checkConfig(schema, exportText(schema, typed)));
assert.ok(typedErrs[".shops.defaultBuyPrice"]?.includes("below the minimum 0"), "range error from config-check's wording");
assert.ok(typedErrs[".shops.priceRules.Prefabs/Items/Food/.sellPrice"]?.includes("whole number"));

// #288: import then export of a valid file is byte-equivalent after formatting, unknown keys kept.
for (const text of [fx("good.json"), fs.readFileSync(new URL("../../docs/config/config.example.json", import.meta.url), "utf8")]) {
  const raw = plain(parseJson(text));
  const { value, extras, keep } = importConfig(schema, raw);
  assert.equal(exportText(schema, value, extras, keep), JSON.stringify(raw, null, 2) + "\n", "import then export round-trips");
}
{
  const raw = { schemaVersion: 1, mystery: 1, shops: { defaultBuyPrice: 70, bogus: true, priceRules: { "Prefabs/A/": { buyPrice: 5, colour: 1 }, "Prefabs/B/": 5 } } };
  const { value, extras } = importConfig(schema, raw);
  assert.equal(value.shops.defaultBuyPrice, 70, "known value lands in the form");
  assert.deepEqual(JSON.parse(exportText(schema, value, extras)), raw, "unknown keys and odd shapes survive export");
  const notes = checkConfig(schema, exportText(schema, value, extras));
  assert.ok(notes.some((p) => p.level === "warn" && p.msg.includes("mystery")), "unknown key is noted");
  assert.ok(notes.some((p) => p.level === "error" && p.msg.includes("Prefabs/B/")), "odd record is still an error");
}
{
  const older = plain(parseJson(fx("older.json")));
  const { value, extras } = importConfig(schema, older);
  const v3 = JSON.parse(fs.readFileSync(new URL("schema-v3.json", fixtures), "utf8"));
  const notes = checkConfig(v3, exportText(v3, value, extras));
  assert.ok(notes.some((p) => p.msg.includes("older than this build")), "older schemaVersion shows the migration notice");
}
{
  const { value, extras } = importConfig(schema, plain(parseJson(fx("too-new.json"))));
  assert.ok(checkConfig(schema, exportText(schema, value, extras)).some((p) => p.level === "error"), "a too-new file stays an error, export stays off");
}

// #289: the presets are the harness's own files; each loads into the form, round-trips and checks clean.
// #381: the server.json half is the base game's shape with both addon ids and its own checks.
{
  const { defaultServerOpts, serverJson, serverProblems, modIdList, startupLine } = await import("../src/lib/serverbundle.ts");
  const dir = new URL("../../tools/test/config-fixtures/presets/", import.meta.url);
  const names = fs.readdirSync(dir).filter((f) => f.endsWith(".json") && !f.endsWith(".expect.json"));
  for (const want of ["flagship-1989", "small-server", "hardcore-economy"]) assert.ok(names.includes(want + ".json"), `preset ${want} exists`);
  for (const f of names) {
    const text = fs.readFileSync(new URL(f, dir), "utf8");
    const { value, extras, keep } = importConfig(schema, plain(parseJson(text)));
    const out = exportText(schema, value, extras, keep);
    assert.deepEqual(JSON.parse(out), JSON.parse(text), `${f}: loads into the form and exports unchanged`);
    assert.deepEqual(checkConfig(schema, out).filter((p) => p.level === "error"), [], `${f}: no errors`);
  }
  assert.ok(importConfig(schema, JSON.parse(fs.readFileSync(new URL("hardcore-economy.json", dir), "utf8"))).value.shops.defaultBuyPrice === 80, "hardcore loads its prices into the form");

  const data = {
    template: JSON.parse(fs.readFileSync(new URL("../../docs/playtest/server-config.example.json", import.meta.url), "utf8")),
    core: { id: /GUID "(\w+)"/.exec(fs.readFileSync(new URL("../../addons/LifeFramework/LifeFramework.gproj", import.meta.url), "utf8"))[1], name: "Life Framework", scenario: "{5A1E5A1E5A1E0002}Missions/EL_DebugTest.conf" },
    pack: { id: /GUID "(\w+)"/.exec(fs.readFileSync(new URL("../../addons/LF_Everon1989/LF_Everon1989.gproj", import.meta.url), "utf8"))[1], name: "Life Framework: Everon 1989", scenario: "{C638C03B775C31BB}Missions/LF_Everon1989.conf" },
  };
  const o = defaultServerOpts(data);
  assert.deepEqual(serverProblems({ ...o, adminPassword: "s3cret" }), [], "a filled-in bundle has no problems");
  const sj = JSON.parse(serverJson(data, { ...o, adminPassword: "s3cret", name: "Mine", port: 2002, maxPlayers: 24 }));
  assert.equal(sj.bindPort, 2002); assert.equal(sj.publicPort, 2002); assert.equal(sj.game.name, "Mine"); assert.equal(sj.game.maxPlayers, 24);
  assert.deepEqual(sj.game.mods.map((m) => m.modId), [data.core.id, data.pack.id], "pack scenario lists core then pack");
  assert.equal(sj.game.scenarioId, data.pack.scenario);
  assert.deepEqual(Object.keys(sj), Object.keys(data.template), "same top-level shape as the base game's file");
  assert.deepEqual(JSON.parse(serverJson(data, { ...o, scenario: "core" })).game.mods.map((m) => m.modId), [data.core.id], "core scenario lists only the core");
  assert.equal(modIdList(data, o), `${data.core.id},${data.pack.id}`);
  assert.ok(startupLine().includes("-config server.json"));
  const msgs = (x) => serverProblems({ ...o, ...x }).filter((p) => p.level === "error").map((p) => p.msg).join("|");
  assert.match(msgs({ port: 70000 }), /bindPort 70000/);
  assert.match(msgs({ a2sPort: 2001 }), /must differ/);
  assert.match(msgs({ maxPlayers: 0 }), /maxPlayers/);
  assert.match(msgs({ adminPassword: "a b" }), /space/);
  assert.match(msgs({ name: " " }), /game.name/);
  assert.ok(serverProblems(o).some((p) => p.level === "warn"), "the placeholder password warns");
}
console.log("configgen-selftest: OK");

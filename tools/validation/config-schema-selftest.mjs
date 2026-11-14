// Proves the config schema generator parses attributes and that validate-config-schema
// inputs: tools/**, agent-box/**, .githooks/**
// goes red on a stale schema or a bad example. Fixture: fixtures/config-schema/Demo/*.c.
//
// red-proof: with the range parse in attrInfo nulled the run printed "FAIL cooldown:
// type/default/min/max" and "FAIL below minimum must fail" and exited 1. Adding an attribute to
// LF_ShopPricesConfig without regenerating made validate-config-schema print "FAIL
// docs/config/config.schema.json is stale against the config classes" and exit 1. With the constant
// comparison disabled the run printed "FAIL range: a differing constant must fail". All restored.
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { rangeProblems, readerProblems, generate, readScripts, render, validateJson, jsonKey, runtimeExample, SERVICE } from "../config/schema.mjs";

const here = dirname(fileURLToPath(import.meta.url));
const fx = join(here, "fixtures", "config-schema");
const fails = [];
const expect = (ok, msg) => { if (!ok) fails.push(msg); };
const svc = "static const int SCHEMA_VERSION = 3;";
const overrides = { records: { LF_DemoRecord: { keyFields: ["m_sKey"], key: "The record key." } }, examples: {} };

const files = readScripts(fx);
const { schema, example } = generate(files, svc, overrides);
const demo = schema.properties.demo;

expect(jsonKey("m_iDefaultBuyPrice") === "defaultBuyPrice", "jsonKey: Hungarian prefix and lower camel case");
expect(schema.properties.schemaVersion.maximum === 3, "schemaVersion maximum comes from SCHEMA_VERSION");
expect(demo && demo["x-class"] === "LF_DemoConfig", "the configRoot class with SECTION becomes a section");
const cd = demo.properties.cooldownSec;
expect(cd.type === "integer" && cd.default === 30 && cd.minimum === 5 && cd.maximum === 600, `cooldown: type/default/min/max, got ${JSON.stringify(cd)}`);
expect(cd.description === "Cooldown in seconds", `desc: got ${cd.description}`);
expect(demo.properties.chance.type === "number" && demo.properties.chance.default === 0.5, "float default");
expect(demo.properties.enabled.default === true, "bool default");
expect(demo.properties.name.description === 'Display name, with a comma, and "quotes"', "desc with comma and escaped quotes");
expect(demo.properties.mode.type === "integer" && demo.properties.mode.default === 2, "enum member is an integer");
const rec = demo.properties.records.additionalProperties;
expect(demo.properties.records.type === "object" && rec.properties.amount.default === 7, "record list is an object keyed by the record key");
expect(!("key" in rec.properties), "key fields are not record properties");
expect(example.demo.cooldownSec === 30 && example.demo.records && Object.keys(example.demo.records).length === 0, "example takes the attribute defaults");
expect(validateJson(example, schema).length === 0, `example validates: ${validateJson(example, schema)}`);

// Stale: a new attribute changes the generated text, so the committed copy no longer matches.
const committed = render(schema);
const grown = files.map((f) => ({ ...f, text: f.text.replace("static const string SECTION", '[Attribute("1", UIWidgets.Auto, desc: "New")]\n\tint m_iAdded;\n\n\tstatic const string SECTION') }));
expect(render(generate(grown, svc, overrides).schema) !== committed, "stale: a new attribute must make the committed schema differ");
expect(render(generate(files, svc, overrides).schema) === committed, "generation is deterministic");

// Bad examples go red.
const bad = (patch) => validateJson(patch(structuredClone(example)), schema).length > 0;
expect(bad((e) => { e.demo.cooldownSec = 1; return e; }), "below minimum must fail");
expect(bad((e) => { e.demo.cooldownSec = 601; return e; }), "above maximum must fail");
expect(bad((e) => { e.demo.cooldownSec = "x"; return e; }), "wrong type must fail");
expect(bad((e) => { e.demo.cooldownSec = 12.5; return e; }), "float for an int must fail");
expect(bad((e) => { e.demo.nope = 1; return e; }), "unknown key must fail");
expect(bad((e) => { e.demo.records.a = { amount: "x" }; return e; }), "bad record field must fail");
expect(bad((e) => { e.schemaVersion = 4; return e; }), "too-new schemaVersion must fail");

// Range agreement: params and the ReadInt limits must say the same numbers.
const edit = (from, to) => files.map((f) => ({ ...f, text: f.text.replace(from, to) }));
expect(rangeProblems(files).length === 0, `range: the fixture agrees, got ${rangeProblems(files)}`);
expect(rangeProblems(edit("COOL_MAX = 600", "COOL_MAX = 900")).some((m) => /checks 5\.\.900/.test(m)), "range: a differing constant must fail");
expect(rangeProblems(edit(', params: "5 600 1"', "")).some((m) => /no params range/.test(m)), "range: a checked read without params must fail");
expect(rangeProblems(edit('m_iCooldownSec, COOL_MIN, COOL_MAX', "m_iCooldownSec")).some((m) => /no ReadInt/.test(m)), "range: params without a checked read must fail");
expect(rangeProblems(edit("COOL_MIN = 5", "COOL_MIN = x")).some((m) => /cannot resolve/.test(m)), "range: an unresolvable constant must fail");

// Reader agreement: the schema's fields and the reader's key lists must match, both ways.
const rp = (from, to) => readerProblems(edit(from, to), schema);
expect(readerProblems(files, schema).length === 0, `reader: the fixture agrees, got ${readerProblems(files, schema)}`);
expect(rp('{"amount"}', '{"amount", "ghost"}').some((m) => /handles "ghost"/.test(m)), "reader: a handled key with no schema field must fail");
expect(rp('{"amount"}', '{"ghost"}').some((m) => /lists "amount" but the reader/.test(m)), "reader: a schema field the reader does not handle must fail");
expect(rp('"name", "mode"', '"mode"').some((m) => /lists "name"/.test(m)), "reader: a missing section key must fail");
expect(rp('reader.ReadBool("enabled", m_bEnabled);', "").some((m) => /no Read\*\/BeginRecords call reads it/.test(m)), "reader: a listed key nothing reads must fail");
expect(rp("reader.WarnUnknownKeys(recordKeys);", "").some((m) => /no key list for this level/.test(m)), "reader: a level with no key list must fail");

// The runtime example is rebuilt from the real service source and fits the real schema shape.
const rt = runtimeExample(readFileSync(SERVICE, "utf8"));
expect(rt && rt.schemaVersion >= 1 && rt.shops, "runtimeExample reads LF_ConfigService.WriteExample");

if (fails.length) {
  for (const f of fails) console.log(`FAIL ${f}`);
  process.exit(1);
}
console.log("config-schema-selftest: OK (attributes parsed, stale schema and bad examples red)");

// Proves `tools\cli config-check` (#281): each reject class of an owner config.json comes back
// inputs: tools/**, agent-box/**, .githooks/**
// with the line, column and message the server would log, a clean file passes, warnings alone exit
// 0, and the rules follow the schema (a range or a version changed in the schema changes the
// verdict with no edit to the checker). Fixtures: tools/validation/fixtures/config-check/.
//
// red-proof (#304): with the rename line of `describeChange` in tools/config/migrations.mjs printing "renamed" the run printed
// "FAIL migrate old: changes [...]" and exited 1; restored, it prints OK.
// red-proof: with the maximum test in `range` loosened to `v > sch.maximum + 99999999` the run printed
// "FAIL bad: range above max" and exited 1; with the unknown-key branch of `object` no longer
// adding the warning it printed "FAIL bad: unknown nested key"; restored, it prints
// "config-check-selftest: OK".
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { checkText, parseJson, parseShopRuleKey, main } from "../config/check.mjs";
import { SCHEMA_PATH, SERVICE } from "../config/schema.mjs";
import { loadManifest, renderPlan } from "../config/migrations.mjs";

const here = dirname(fileURLToPath(import.meta.url));
const fx = (f) => join(here, "fixtures", "config-check", f);
const text = (f) => readFileSync(fx(f), "utf8");
const schema = JSON.parse(readFileSync(SCHEMA_PATH, "utf8"));
const sources = {
  shop: fx("shipped/shop.conf"), chains: fx("shipped/chains.conf"), vehicles: fx("shipped/vehicles.conf"),
  chop: fx("shipped/chop.conf"), layers: [join(here, "fixtures", "profit-loop", "lot.layer")], owner: null,
};
const fails = [];
const expect = (ok, msg) => { if (!ok) fails.push(msg); };
const run = (f, opts = {}) => checkText(text(f), { source: "config.json", sources, schema, loops: false, ...opts });
const find = (r, line, col) => r.problems.find((p) => p.line === line && p.col === col);
const has = (p, level, frag) => !!p && p.level === level && p.msg.includes(frag);

// Clean file: nothing at all.
const good = run("good.json");
expect(good.problems.length === 0, `good: expected no problems, got ${JSON.stringify(good.problems)}`);

// Syntax error: one error at the offending character.
const syn = run("syntax.json");
expect(syn.problems.length === 1 && syn.problems[0].line === 5 && syn.problems[0].col === 3 && has(syn.problems[0], "error", "is not valid JSON"), `syntax: ${JSON.stringify(syn.problems)}`);
for (const [src, line, col] of [["", 1, 1], ['{"a": }', 1, 7], ['{"a": "x\n"}', 1, 9], ["{\n\n  1}", 3, 3], ['{"a":1} x', 1, 9], ['{"a":1,}', 1, 8]]) {
  let e = null;
  try { parseJson(src); } catch (x) { e = x; }
  expect(e && e.line === line && e.col === col, `parseJson(${JSON.stringify(src)}): expected ${line}:${col}, got ${e ? `${e.line}:${e.col}` : "no error"}`);
}

// Each reject class of the server's reader, at its line:col.
const bad = run("bad.json");
expect(has(find(bad, 3, 3), "warn", "unknown top-level key mystery; ignored"), "bad: unknown top-level key");
expect(has(find(bad, 5, 24), "error", "shops.defaultBuyPrice is not a whole number"), "bad: type error");
expect(has(find(bad, 6, 25), "error", "shops.defaultSellPrice = -5 is below the minimum 0"), "bad: range below min");
expect(has(find(bad, 7, 5), "warn", "shops.bogus is not a known key; ignored"), "bad: unknown nested key");
expect(has(find(bad, 9, 44), "error", "= 2000000 is above the maximum 1000000"), "bad: range above max");
expect(has(find(bad, 9, 63), "error", ".hidden is not true or false"), "bad: bool type error");
expect(has(find(bad, 9, 70), "warn", ".colour is not a known key; ignored"), "bad: unknown record field");
expect(has(find(bad, 10, 7), "warn", "no shipped record has this key"), "bad: record key not in the shipped config");
expect(has(find(bad, 11, 7), "error", "neither a prefab-path substring, prefab:<GUID> nor type:"), "bad: malformed type key");
expect(has(find(bad, 12, 7), "error", "is not a record (an object); record skipped"), "bad: record that is not an object");
expect(has(find(bad, 14, 25), "error", "shops.extraItemPrefabs is not a list of strings"), "bad: list of non-strings");
expect(has(find(bad, 16, 15), "error", "section vehicles is not an object"), "bad: section not an object");
expect(!bad.problems.some((p) => p.line === 8 || p.line === 13), "bad: a valid line produced a problem");

// A shipped key is fine; "type:64" and "type:64,mode:2" are the same shipped rule.
expect(run("typed.json").problems.length === 0, `typed: ${JSON.stringify(run("typed.json").problems)}`);

// A whole-number list is legal (`jobs.tracks.*.levelThresholds`); a string in it is not.
const thresholds = checkText('{"jobs":{"tracks":{"DELIVERY":{"levelThresholds":[400,1200,3000]}}}}', { sources, schema, loops: false });
expect(thresholds.problems.length === 0, `thresholds: ${JSON.stringify(thresholds.problems)}`);
const mixed = checkText('{"jobs":{"tracks":{"DELIVERY":{"levelThresholds":[400,"1200"]}}}}', { sources, schema, loops: false });
expect(has(mixed.problems[0], "error", "jobs.tracks.DELIVERY.levelThresholds is not a list of whole numbers"), `thresholds string: ${JSON.stringify(mixed.problems)}`);
expect(parseShopRuleKey("type:64").norm === "type:64,mode:2" && parseShopRuleKey("type:64,mode:1").norm === "type:64,mode:1", "ruleKey normalisation");
expect(parseShopRuleKey("type:64,mode:2,x:1") === null && parseShopRuleKey("") === null && parseShopRuleKey("mode:1,type:64").norm === "mode:1,type:64" && parseShopRuleKey("type:") === null, "ruleKey grammar");

expect(parseShopRuleKey("prefab:c9d661e5b0714711").norm === "prefab:C9D661E5B0714711" && parseShopRuleKey("prefab:") === null, "prefab ruleKey");

// schemaVersion: too new, below 1, and, against a newer build, older.
expect(has(run("too-new.json").problems[0], "error", "schemaVersion 9 but this build reads up to 1"), "too-new");
expect(has(run("zero.json").problems[0], "error", "schemaVersion 0 is below 1"), "zero");
expect(has(run("older.json").problems[0], "error", "schemaVersion 2 but this build reads up to 1"), "a 2 against this build is too new");
const v3 = JSON.parse(readFileSync(fx("schema-v3.json"), "utf8"));
expect(has(run("older.json", { schema: v3 }).problems[0], "warn", "older than this build's 3"), "older: a warning, not an error");

// The rules come from the schema: tighten one range there and the verdict follows.
const tight = JSON.parse(JSON.stringify(schema));
tight.properties.shops.properties.defaultBuyPrice.maximum = 10;
expect(has(run("good.json", { schema: tight }).problems[0], "error", "shops.defaultBuyPrice = 60 is above the maximum 10"), "schema drives the range");

// Profit loops: the owner's prices create one the shipped fixture lacks; a baselined one is skipped.
const loop = run("loop.json", { loops: true });
const lp = loop.problems.find((p) => p.loop && p.msg.includes("shop:Ore"));
expect(lp && lp.level === "warn" && lp.msg.includes("profit loop shop:Ore") && loop.loops.find((l) => l.id === "shop:Ore")?.margin === 7, `loop: ${JSON.stringify(loop.problems)}`);
expect(!run("loop.json", { loops: true, baseline: [{ id: "shop:Ore", margin: 7 }] }).loops.some((l) => l.id === "shop:Ore"), "loop: baselined loop skipped");
expect(run("good.json", { loops: true }).loops.length === 0, "good: no loop");
// An out-of-range price is refused by the server, so it makes no loop either.
const refused = checkText('{"shops":{"priceRules":{"Prefabs/Items/Resources/Ore":{"buyPrice":2,"sellPrice":9000000}}}}', { sources, schema, loops: true });
expect(!refused.loops.some((l) => l.id === "shop:Ore") && refused.problems.some((p) => p.level === "error"), "a refused price must not count as a loop");

// Exit codes through main(): errors 1, warnings only 0, --strict turns a warning into 1.
const quiet = console.log;
console.log = () => {};
const codes = [
  main([fx("good.json"), "--no-loops"]),
  main([fx("bad.json"), "--no-loops"]),
  main([fx("older.json"), "--no-loops", "--schema", fx("schema-v3.json")]),
  main([fx("older.json"), "--no-loops", "--schema", fx("schema-v3.json"), "--strict"]),
  main([fx("missing.json")]),
];
console.log = quiet;
expect(codes[0] === 0, `exit: clean file gave ${codes[0]}`);
expect(codes[1] === 1, `exit: errors gave ${codes[1]}`);
expect(codes[2] === 0, `exit: warnings only gave ${codes[2]}`);
expect(codes[3] === 1, `exit: --strict warning gave ${codes[3]}`);
expect(codes[4] === 1, `exit: a missing file gave ${codes[4]}`);

// Migration report (#304): an old file lists its steps in order with the fields each touches, a
// current file says none, a future file is refused, and nothing is ever written.
const manifest = JSON.parse(text("migrations-test.json"));
const mig = (f, o = {}) => run(f, { manifest, schema: v3, ...o }).migrations;
const old = mig("migrate-old.json");
expect(old.status === "older" && old.fileVersion === 1 && old.build === 3 && old.missing === null, `migrate old: ${JSON.stringify(old)}`);
expect(old.steps.map((s) => `${s.from}>${s.to}:${s.name}`).join() === "1>2:shop-to-shops,2>3:add-price-floor", "migrate old: steps in version order");
expect(old.steps[0].changes.map((c) => c.text).join("|") === "rename shop -> shops (in your file)|drop legacyFlag (in your file)", `migrate old: changes ${JSON.stringify(old.steps[0].changes)}`);
expect(old.steps[1].changes[0].text === "default shops.priceFloor = 1 (missing from your file; added)", "migrate old: default");
const oldText = renderPlan(old).join("\n");
expect(oldText.includes("2 would run") && oldText.includes("v1 -> v2  shop-to-shops: Rename the shop section to shops.") && oldText.includes("Dry run: nothing was written"), "migrate old: rendered");
const cur = mig("migrate-current.json");
expect(cur.status === "current" && cur.steps.length === 0 && renderPlan(cur).join("").includes("none needed"), "migrate current: none needed");
const future = mig("too-new.json", { schema });
expect(future.status === "newer" && future.steps.length === 0 && renderPlan(future).join("").includes("refused"), "migrate future: refused");
expect(run("too-new.json", { schema }).problems.some((p) => p.level === "error"), "migrate future: still an error (exit 1)");
expect(mig("good.json", { schema }).status === "current", "migrate: a file at the build's version needs none");
expect(mig("syntax.json").status === "unreadable", "migrate: an unparseable file plans nothing");
const gap = run("migrate-old.json", { manifest: { migrations: [manifest.migrations[1]] }, schema: v3 }).migrations;
expect(gap.missing === 1 && gap.steps.length === 0 && renderPlan(gap).join("").includes("no migration reads version 1"), "migrate: a missing step breaks the chain");
// main(): the report reaches the terminal, and the dry run leaves the file byte for byte as it was.
const before = readFileSync(fx("migrate-old.json"), "utf8");
const lines = [];
console.log = (...a) => lines.push(a.join(" "));
const mcode = main([fx("migrate-old.json"), "--no-loops", "--schema", fx("schema-v3.json"), "--migrations", fx("migrations-test.json")]);
console.log = quiet;
expect(mcode === 0 && lines.some((l) => l.includes("1. v1 -> v2  shop-to-shops")), `main migrate: ${mcode} ${lines.join("|")}`);
expect(readFileSync(fx("migrate-old.json"), "utf8") === before, "dry run must not write the file");
// The shipped manifest matches LF_ConfigMigrator.BuildShipped: same steps, version order, no gaps.
const shipped = loadManifest().migrations;
const buildShipped = /static void BuildShipped[\s\S]*?\n\t\}/.exec(readFileSync(join(dirname(SERVICE), "LF_ConfigMigration.c"), "utf8"))?.[0] ?? "";
const inserted = (buildShipped.match(/steps\.Insert\(/g) ?? []).length;
expect(inserted === shipped.length, `manifest has ${shipped.length} migration(s), BuildShipped inserts ${inserted}; keep docs/config/config.migrations.json in step`);
const buildSchema = Number(/SCHEMA_VERSION\s*=\s*(\d+)/.exec(readFileSync(SERVICE, "utf8"))?.[1]);
expect(shipped.every((m, i) => m.from === i + 1 && m.name && m.description) && shipped.length === buildSchema - 1, "manifest: one entry per version step 1..SCHEMA_VERSION-1, each with a name and description");

if (fails.length) {
  for (const f of fails) console.log(`FAIL ${f}`);
  process.exit(1);
}
console.log("config-check-selftest: OK (migration report, syntax line:col, type, range, unknown key, schemaVersion, record key, profit loop, exit codes; rules follow the schema)");

// tools/validation/livery-selftest.mjs
// inputs: tools/livery/**, tools/commands/livery.mjs
//
// Proves the `tools\cli livery build` generator (tools/livery/generate.mjs) offline: each fixture row in
// tools/livery/fixtures/rows.json, built against the vanilla prefab texts in tools/livery/fixtures/vanilla/,
// writes exactly the files in tools/livery/fixtures/expected/<row id>/ (variant prefab, body material,
// door parts with their door plates, plate sign row), the pack strings in expected/<row id>/strings.st and the
// credit row in expected/<row id>/credit.md. A row without a credit fails, and so do mask lettering (it
// scrambles across UV islands; words go on door plates), a plate text that is not capitals and a GUID shared
// by two rows. Each front door carries one plate and the rear doors none. The real table
// (tools/livery/table.mjs) must build against the same fixtures with unique GUIDs.
// red-proof: before tools/livery/generate.mjs existed this printed
// "FAIL cannot load tools/livery/generate.mjs: ERR_MODULE_NOT_FOUND" (2026-11-23).
// red-proof: with the credit check in planLivery turned off this printed
// "FAIL  a row without a credit: wrong error \"Cannot read properties of undefined (reading 'source')\"" (2026-11-23).
// red-proof: with the door plate child turned off this printed
// "FAIL  addons/LF_Everon1989/Prefabs/Vehicles/Liveries/UAZ469_FixtureBlue_door_L01.et: 0 plates, want 1" (2026-11-23).
//
// `node tools/validation/livery-selftest.mjs --update` rewrites expected/ from the generator (review the diff).

import { existsSync, mkdirSync, readdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const FIX = join(ROOT, "tools", "livery", "fixtures");
const UPDATE = process.argv.includes("--update");
let failed = 0;
const fail = (m) => { console.log(`FAIL  ${m}`); failed++; };

let gen, table;
try {
  gen = await import(pathToFileURL(join(ROOT, "tools", "livery", "generate.mjs")).href);
  table = await import(pathToFileURL(join(ROOT, "tools", "livery", "table.mjs")).href);
} catch (e) {
  console.log(`FAIL cannot load tools/livery/generate.mjs: ${e.code || e.message}`);
  process.exit(1);
}

const vanilla = (rel) => {
  const p = join(FIX, "vanilla", rel);
  if (!existsSync(p)) throw new Error(`fixture has no vanilla file ${rel}`);
  return readFileSync(p, "utf8");
};
const throws = (fn, re, what) => {
  try { fn(); fail(`${what}: no error`); } catch (e) { if (!re.test(e.message)) fail(`${what}: wrong error "${e.message}"`); }
};

const rows = JSON.parse(readFileSync(join(FIX, "rows.json"), "utf8"));
for (const row of rows.good) {
  const plan = gen.planLivery(row, vanilla);
  const dir = join(FIX, "expected", row.id);
  const got = new Map(plan.files.map((f) => [f.rel.replace(/\//g, "__"), f.text]));
  for (const f of plan.files) got.set(f.rel.replace(/\//g, "__") + ".meta", gen.metaText(f));
  got.set("credit.md", plan.credit + "\n");
  if (plan.mask) {
    got.set("mask-meta.txt", plan.mask.meta);
    got.set("mask-spec.json", JSON.stringify(plan.mask.spec) + "\n");
  }
  if (UPDATE) {
    mkdirSync(dir, { recursive: true });
    for (const [n, t] of got) writeFileSync(join(dir, n), t);
    console.log(`updated ${row.id}: ${got.size} files`);
    continue;
  }
  if (!existsSync(dir)) { fail(`${row.id}: no expected/${row.id}/ (run with --update)`); continue; }
  const want = new Set(readdirSync(dir));
  for (const [n, t] of got) {
    if (!want.has(n)) { fail(`${row.id}: unexpected file ${n}`); continue; }
    want.delete(n);
    const w = readFileSync(join(dir, n), "utf8");
    if (w !== t) {
      const a = w.split("\n"), b = t.split("\n");
      const i = a.findIndex((l, k) => l !== b[k]);
      fail(`${row.id}: ${n} differs at line ${i + 1}: want "${a[i]}" got "${b[i]}"`);
    }
  }
  for (const n of want) fail(`${row.id}: expected ${n} was not written`);
}

// A material reference in the variant prefab must name the material file this row writes.
for (const row of rows.good) {
  const plan = gen.planLivery(row, vanilla);
  const emat = plan.files.find((f) => f.rel.endsWith(".emat"));
  const veh = plan.files.find((f) => f.role === "vehicle");
  if (!emat || !veh) { fail(`${row.id}: no material or no vehicle prefab`); continue; }
  if (!veh.text.includes(`"{${emat.guid}}${emat.rel.replace(/^addons\/LF_Everon1989\//, "")}"`)) fail(`${row.id}: the vehicle prefab does not reference its own material`);
}

throws(() => gen.planLivery(rows.noCredit, vanilla), /credit/i, "a row without a credit");
throws(() => gen.planLivery(rows.maskLettering, vanilla), /mask lettering is gone/i, "a row with mask lettering");
throws(() => gen.planLivery(rows.badPlateText, vanilla), /plate.text must be/i, "a plate text that is not capitals");

// One plate per front door, none on the rear doors, and every plate names the row's sign id.
{
  const plan = gen.planLivery(rows.good[0], vanilla);
  const signs = plan.files.find((f) => f.role === "signs");
  const id = signs && /m_sId "([^"]+)"/.exec(signs.text)?.[1];
  if (!id) fail("the plate row wrote no sign row");
  for (const f of plan.files.filter((x) => x.role === "door")) {
    const n = (f.text.match(/LF_SignComponent/g) || []).length;
    const front = /_door_(L01|R01).et$/.test(f.rel);
    if (n !== (front ? 1 : 0)) fail(`${f.rel}: ${n} plates, want ${front ? 1 : 0}`);
    if (front && !f.text.includes(`m_sSignId "${id}"`)) fail(`${f.rel}: the plate does not name sign ${id}`);
  }
  if (!plan.strings.some((s) => signs?.text.includes(`"#${s.id}"`) && s.text === "POLICE")) fail("the sign row does not use the plate's string");
}
throws(() => gen.checkUnique([rows.good[0], { ...rows.good[0] }].map((r) => gen.planLivery(r, vanilla))), /GUID .* twice/i, "two rows with one id");

// The real table builds against the fixture vanilla files and no GUID repeats.
try {
  gen.checkUnique(table.LIVERIES.map((r) => gen.planLivery(r, vanilla)));
} catch (e) { fail(`table.mjs: ${e.message}`); }

// The credit rows go under "## Liveries" in docs/credits.md, once.
const cmd = await import(pathToFileURL(join(ROOT, "tools", "commands", "livery.mjs")).href);
const credits = rows.good.map((r) => gen.planLivery(r, vanilla).credit);
const once = cmd.addCreditRows("# Credits\n\n## Fonts\n\n| a |\n", credits.slice(0, 1));
const twice = cmd.addCreditRows(once, credits);
if (!/## Liveries\n[\s\S]*FixtureBlue/.test(once)) fail("addCreditRows did not open a Liveries section with the row");
if (twice.split("FixtureBlue").length !== 2 || !twice.includes("FixtureWhite")) fail(`addCreditRows repeated or dropped a row:\n${twice}`);
if (cmd.addCreditRows(twice, credits) !== twice) fail("addCreditRows changed a file that already has every row");
const mid = cmd.addCreditRows(cmd.addCreditRows("# C\n\n## Liveries\n\n| x |\n\n## Music\n\n| m |\n", credits.slice(0, 1)), credits);
if (!/## Liveries\n\n\| x \|\n.*FixtureBlue.*\n.*FixtureWhite.*\n\n## Music/.test(mid)) fail(`addCreditRows did not append inside a Liveries section that is not last:\n${mid}`);

// The plate strings go into the pack table once, inside its item list.
const items = gen.planLivery(rows.good[0], vanilla).strings;
const st = "StringTableSource {\n Items {\n    CustomStringTableItem \"{0000000000000001}\" {\n     Id \"A\"\n    }\n }\n}\n";
const st1 = cmd.addStrings(st, items, gen.stringItem);
if (!/Id "A"\n    }\n    CustomStringTableItem "\{[0-9A-F]{16}\}" \{\n     Id "LF-Sign_Livery_UAZ469_FixtureBlue_Title"\n     Target_en_us "POLICE"[\s\S]*\n }\n}\n$/.test(st1)) fail(`addStrings did not add the plate string inside Items:\n${st1}`);
if (cmd.addStrings(st1, items, gen.stringItem) !== st1) fail("addStrings added a string the table already has");

if (failed) { console.log(`livery-selftest: ${failed} failure(s)`); process.exit(1); }
console.log(`livery-selftest: OK (${rows.good.length} fixture rows, ${table.LIVERIES.length} table rows)`);

// tools/validation/validate-terms.mjs
// inputs: addons/**/Language/*.st, addons/**/Configs/**/*.conf (terms files), tools/validation/terms-baseline.json
//
// The terms layer (#958). Core text names a term in a placeholder ({police}, {Town}, {currency:sym})
// so a pack or an owner can rename it in every language. Two rules:
//  1. A placeholder in any string table, in any language, names a term and a form that exist.
//  2. A core key's English text does not hard-code a listed term's word. Keys written before the
//     terms layer sit in terms-baseline.json; the list only shrinks: a new key that hard-codes a
//     term fails, and a baseline key that no longer does must leave the list.
// Fix for rule 2: write the placeholder in every language of the key and use LF_Terms.Text (or the
// toast, board or dialogue path that calls it); see addons/LifeFramework/Scripts/Game/Feature/Terms/Feature.md.
// red-proof: tools/validation/terms-selftest.mjs
import { readFileSync, readdirSync, writeFileSync, existsSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { CORE_TABLE, hardCoded, loadTerms, parseTable, placeholderProblems } from "../lib/terms.mjs";

const here = dirname(fileURLToPath(import.meta.url));
const BASELINE = join(here, "terms-baseline.json");

export function tables(root) {
  const out = [];
  for (const addon of readdirSync(join(root, "addons"))) {
    const dir = join(root, "addons", addon, "Language");
    if (!existsSync(dir)) continue;
    for (const name of readdirSync(dir)) if (name.endsWith(".st")) out.push(`addons/${addon}/Language/${name}`);
  }
  return out;
}

// Pure: the failures for one set of tables, terms and baseline. { problems: [], hard: {key:[terms]} }
export function check(coreItems, packTables, terms, baseline) {
  const failures = [];
  for (const [name, items] of [["core table", coreItems], ...packTables]) {
    for (const p of placeholderProblems(items, terms)) failures.push(`${name}: ${p.key} (${p.lang}) ${p.problem}`);
  }
  const hard = hardCoded(coreItems);
  for (const [key, list] of Object.entries(hard)) {
    const known = baseline[key];
    if (!known) failures.push(`core table: ${key} hard-codes ${list.join(", ")} in English; write the placeholder (e.g. {${list[0]}}) in every language`);
    else for (const t of list) if (!known.includes(t)) failures.push(`core table: ${key} now hard-codes ${t}, which is not in its baseline entry`);
  }
  for (const [key, list] of Object.entries(baseline)) {
    const now = hard[key] || [];
    const gone = list.filter((t) => !now.includes(t));
    if (gone.length) failures.push(`terms-baseline.json: ${key} no longer hard-codes ${gone.join(", ")}; remove it from the baseline`);
  }
  return { failures, hard };
}

if (process.argv[1] && process.argv[1].split("\\").join("/").endsWith("validate-terms.mjs")) {
  const root = join(here, "..", "..");
  const terms = loadTerms(root);
  const core = parseTable(readFileSync(join(root, CORE_TABLE), "utf8"));
  if (process.argv.includes("--write-baseline")) {
    writeFileSync(BASELINE, JSON.stringify(hardCoded(core), null, 1) + "\n");
    console.log(`validate-terms: wrote ${BASELINE}`);
    process.exit(0);
  }
  const packs = tables(root).filter((t) => t !== CORE_TABLE).map((t) => [t, parseTable(readFileSync(join(root, t), "utf8"))]);
  const baseline = existsSync(BASELINE) ? JSON.parse(readFileSync(BASELINE, "utf8")) : {};
  const { failures, hard } = check(core, packs, terms, baseline);
  for (const f of failures) console.log(`FAIL ${f}`);
  if (failures.length) process.exit(1);
  console.log(`validate-terms: OK (${Object.keys(terms).length} terms; ${Object.keys(hard).length} older core keys still hard-code one, baseline)`);
}

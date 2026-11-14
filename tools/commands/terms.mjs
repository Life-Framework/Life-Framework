// tools\cli terms: every term, its forms, and the string keys that use it (#958).
import { readFileSync } from "node:fs";
import { join } from "node:path";
import { CORE_TABLE, PLACEHOLDER, hardCoded, loadTerms, parseTable } from "../lib/terms.mjs";
import { tables } from "../validation/validate-terms.mjs";

export const help = "terms [--json]               list every term (core and pack overlays), its forms, the keys that use it, and how many older core keys still hard-code it";

export async function main(args, ctx) {
  const root = ctx.root;
  const terms = loadTerms(root);
  const uses = {};
  for (const table of tables(root)) {
    for (const item of parseTable(readFileSync(join(root, table), "utf8"))) {
      for (const text of Object.values(item.texts)) {
        for (const m of text.matchAll(PLACEHOLDER)) {
          const id = m[1][0].toLowerCase() + m[1].slice(1);
          (uses[id] ||= new Set()).add(item.id);
        }
      }
    }
  }
  const hard = hardCoded(parseTable(readFileSync(join(root, CORE_TABLE), "utf8")));
  const rows = Object.entries(terms).map(([id, t]) => ({
    id,
    forms: t.forms,
    files: t.files,
    keys: [...(uses[id] || [])].sort(),
    hardCoded: Object.entries(hard).filter(([, list]) => list.includes(id)).length,
  }));
  if (args.includes("--json")) {
    console.log(JSON.stringify(rows, null, 1));
    return 0;
  }
  for (const r of rows) {
    console.log(`${r.id}   ${Object.entries(r.forms).map(([f, v]) => `${f}=${v}`).join("  ")}`);
    console.log(`  defined in: ${r.files.join(", ")}`);
    console.log(`  used by ${r.keys.length} key(s): ${r.keys.join(", ") || "none"}`);
    console.log(`  ${r.hardCoded} older core key(s) still hard-code it (tools/validation/terms-baseline.json)`);
  }
  console.log("\nWrite {id} for the singular, {Id} to capitalise, {id:plural} or {id:pl}, {id:form} for another form. Owner: config.json \"terms\" section (docs/config/owner-config.md).");
  return 0;
}

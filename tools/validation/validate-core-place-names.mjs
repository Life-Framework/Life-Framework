// tools/validation/validate-core-place-names.mjs
// inputs: addons/LifeFramework/Language/*.st
//
// The core's string table holds neutral text; place names belong to the pack's table
// (#325, docs/architecture/core-vs-pack.md "Strings"). This fails when a core key's English
// text names Everon, St Philippe or 1989. The fix is to give the core key neutral text and
// put the flavour in the pack's table under the same Id (the pack's text wins when it loads).
// red-proof: tools/validation/core-place-names-selftest.mjs plants each name in a fixture table.
import { readFileSync, readdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

export const PLACE_NAMES = /\b(Everon|St\.?\s+Philippe|Saint\s+Philippe|1989)\b/i;
// A quoted text may hold braces (a term placeholder such as {police}, #958), so the body skips quoted strings whole.
const ITEM = /CustomStringTableItem\s+"\{[0-9A-Fa-f]+\}"\s*\{((?:[^}"]|"(?:[^"\\]|\\.)*")*)\}/g;

// [{ id, text }] for every core key whose English text names a place.
export function placeNameHits(stText) {
  const hits = [];
  for (const m of stText.matchAll(ITEM)) {
    const id = /Id\s+"([^"]+)"/.exec(m[1])?.[1];
    const en = /Target_en_us\s+"([^"]*)"/.exec(m[1])?.[1];
    if (id && en && PLACE_NAMES.test(en)) hits.push({ id, text: en });
  }
  return hits;
}

if (process.argv[1] && process.argv[1].split("\\").join("/").endsWith("validate-core-place-names.mjs")) {
  const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
  const dir = join(root, "addons", "LifeFramework", "Language");
  let bad = 0;
  for (const f of readdirSync(dir).filter((n) => n.endsWith(".st"))) {
    for (const h of placeNameHits(readFileSync(join(dir, f), "utf8"))) {
      bad++;
      console.log(`FAIL ${f}: ${h.id} names a place in English ("${h.text.slice(0, 60)}"): neutral text in the core, the flavour in LF_Everon1989's table`);
    }
  }
  if (bad) process.exit(1);
  console.log("validate-core-place-names: OK");
}

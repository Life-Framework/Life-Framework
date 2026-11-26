// tools/validation/validate-npc-lines-voiced.mjs - every source of NPC speech reaches a recorded, current line.
// inputs: tools/assets/dialogue/**, addons/**/Scripts/**/*.c, addons/**/Configs/**/*Quest*.conf, addons/**/Configs/**/*Rumor*.conf,
// addons/**/Prefabs/**/*.et, the string tables, tools/lib/npc-lines-voiced.mjs
//
// "Every NPC dialogue line is voiced" (maintainer, 0.3.0 rule). validate-dialogue-voices judges the lines a dialogue lists.
// This judges what feeds them: a dialogue hook's spoken keys, a quest giver's description / hand-in / idle strings, a rumor
// row, an announced key, and the pin that says a spoken paraphrase still matches the string it stands for (the market errand
// of #4931 changed its string while the clerk kept the old words). Rules and fixes: tools/lib/npc-lines-voiced.mjs.
// `--update` rewrites every for_key line's key_pin after you re-read the lines. `--report` prints coverage and exits 0.
// red-proof: tools/validation/npc-lines-voiced-selftest.mjs plants each gap in a scratch repo.
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { check, writePins } from "../lib/npc-lines-voiced.mjs";

const here = dirname(fileURLToPath(import.meta.url));
const root = process.env.LF_NPC_LINES_ROOT || join(here, "..", "..");

if (process.argv.includes("--update")) {
  console.log(`validate-npc-lines-voiced: ${writePins(root)} key pin(s) written`);
  process.exit(0);
}

const { fails, total, covered } = check(root);
if (process.argv.includes("--report")) {
  for (const f of fails) console.log(`GAP ${f}`);
  console.log(`${covered} of ${total} speech sources reach a recorded line`);
  process.exit(0);
}
if (fails.length) {
  for (const f of fails) console.log(`FAIL ${f}`);
  console.log(`validate-npc-lines-voiced: ${fails.length} problem(s); ${covered} of ${total} speech sources voiced`);
  process.exit(1);
}
console.log(`validate-npc-lines-voiced: OK (${covered} of ${total} speech sources voiced)`);

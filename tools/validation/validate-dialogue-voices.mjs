// tools/validation/validate-dialogue-voices.mjs - every line an Everon NPC speaks has a clip made with that NPC's voice.
// inputs: tools/assets/dialogue/**, tools/assets/outfits/roles.json, addons/**/*.et, addons/LF_Everon1989/Worlds/**,
// tools/lib/dialogue-voices.mjs
//
// "Can we ensure all dialogs are voiced" (maintainer, 2026-11-22). Resolves each character prefab the pack owns or
// places to its conversation (dialogue, role conversation, or none) and fails when a line has no recording, the clip
// was made with another voice than the dialogue's, the clip is stale, one voice speaks for two NPCs, or an NPC with a
// shop, ATM, sale or quest action has nothing to say. Voice sex against the body: validate-voice-sex.
// There is no baseline: the last gap, the role conversations, was voiced on 2026-11-22 (role_<id> dialogues).
// `--report` prints voiced / total per conversation and exits 0.
// red-proof: tools/validation/dialogue-voices-selftest.mjs plants each violation in a scratch repo.
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { judge, scan, totals } from "../lib/dialogue-voices.mjs";

const here = dirname(fileURLToPath(import.meta.url));
const root = process.env.LF_DIALOGUE_VOICES_ROOT || join(here, "..", "..");
const rows = scan(root);
const sum = totals(rows);

if (process.argv.includes("--report")) {
  for (const r of rows) console.log(`${r.key.padEnd(26)} ${r.kind.padEnd(8)} ${String(r.voiced).padStart(2)}/${String(r.lines).padEnd(2)} ${(r.voice || "-").padEnd(36)} ${r.who.map((w) => w.split("/").pop().replace(/\.et$/, "")).join(", ")}${r.issues.length ? "  !! " + r.issues.join("; ") : ""}`);
  console.log(`${sum.voiced} of ${sum.lines} island lines voiced in ${rows.length} conversation(s)`);
  process.exit(0);
}

const fails = judge(rows);
if (fails.length) {
  for (const f of fails) console.log(`FAIL ${f}`);
  console.log(`validate-dialogue-voices: ${fails.length} problem(s); ${sum.voiced} of ${sum.lines} island lines voiced. Generate: tools\\cli asset dialogue <npc>`);
  process.exit(1);
}
console.log(`validate-dialogue-voices: OK (${sum.voiced} of ${sum.lines} island lines voiced in ${rows.length} conversation(s))`);

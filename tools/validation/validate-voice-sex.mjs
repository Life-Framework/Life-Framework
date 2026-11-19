// tools/validation/validate-voice-sex.mjs - a voiced NPC's voice matches its body (#3831).
// inputs: tools/assets/dialogue/*/dialogue.json, addons/**/*.et, tools/lib/voice-sex.mjs
//
// "A woman's voice comes from a man" (the cafe, 2026-11-19): the owner's lines were a female voice on a male body.
// The base game has male character bodies only, so every voiced NPC that has a body needs a male voice, and each
// spec says which sex its voice is (`voice.sex`, the voice library's labels.gender). `--report` lists every voiced
// NPC, its voice, its sex and the prefabs that speak it, and exits 0.
// red-proof: 2026-11-19, with the cafe owner's spec on Mariva (voice.sex "female") this printed
// "FAIL cafe_owner: female voice "Mariva Viva Muse ..." on a male body (...LF_Npc_Townsfolk_CafeOwner.et)" and exited 1;
// tools/validation/voice-sex-selftest.mjs holds the fixture.
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { faults, voicedNpcs } from "../lib/voice-sex.mjs";

const root = process.env.LF_VOICE_SEX_ROOT || join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const npcs = voicedNpcs(root);

if (process.argv.includes("--report")) {
  for (const n of npcs) console.log(`${n.id.padEnd(22)} ${(n.sex || "?").padEnd(7)} ${n.voice}  ${n.prefabs.join(", ") || "(no body: heard at a marker)"}`);
  console.log(`${npcs.length} voiced NPC(s)`);
  process.exit(0);
}

const bad = faults(npcs);
if (bad.length) {
  for (const b of bad) console.log(`FAIL ${b}`);
  console.log(`validate-voice-sex: ${bad.length} voiced NPC(s) whose voice is not their body's sex.`);
  process.exit(1);
}
console.log(`validate-voice-sex: OK (${npcs.length} voiced NPC(s))`);

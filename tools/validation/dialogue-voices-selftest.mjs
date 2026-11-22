// tools/validation/dialogue-voices-selftest.mjs - validate-dialogue-voices and tools/lib/dialogue-voices.mjs.
// inputs: tools/validation/validate-dialogue-voices.mjs, tools/lib/dialogue-voices.mjs, tools/validation/dialogue-voices-selftest.mjs
//
// Builds a throwaway repo (a pack NPC prefab, its dialogue spec, provenance and clip) and checks the judge:
//   - a recorded line, made with the dialogue's voice from its current text, passes;
//   - a text-only dialogue, a missing clip, a clip made with another voice and a stale clip each fail;
//   - one voice speaking for two NPCs fails;
//   - an NPC with a shop action and no conversation fails, passes while baselined, and a baseline entry that is
//     voiced or gone fails (the list only shrinks).
//
// red-proof: 2026-11-22, with the `p.voice_id !== voice.id` comparison in lineIssues() removed the "another voice" case printed FAIL.
import { mkdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { scratchDir } from "../lib/scratch.mjs";
import { judge, scan } from "../lib/dialogue-voices.mjs";
import { lineHash } from "../assets/pipeline/dialogue.mjs";

let failed = 0;
const check = (ok, msg) => {
  console.log(`${ok ? "PASS" : "FAIL"}  ${msg}`);
  if (!ok) failed++;
};

const voice = (id) => ({ id, name: `Voice ${id}`, sex: "male", model: "eleven_multilingual_v2" });
const prefab = (cls, body) => `${cls} : "{0000000000000000}Prefabs/Characters/Base.et" {\n components {\n${body} }\n}\n`;
const dlgComponent = (id) => `  LF_DialogueComponent "{1111111111111111}" {\n   m_sDialogueId "${id}"\n  }\n`;
const action = (name) => `  ActionsManagerComponent "{2222222222222222}" {\n   additionalActions + {\n    ${name} "{3333333333333333}" {\n    }\n   }\n  }\n`;

// npcs: [{ id, voiceId, text_only, clip, recordedWith, recordedText }]
function repo(npcs, extra = []) {
  const root = scratchDir("lf-dialogue-voices-");
  const pack = join(root, "addons", "LF_Everon1989");
  mkdirSync(join(pack, "Prefabs"), { recursive: true });
  for (const n of npcs) {
    const spec = { id: n.id, name: "Npc", addon: "LF_Everon1989", start: "greet", lines: { greet: { text: "Good day to you." } }, nodes: [{ id: "greet", line: "greet" }], replies: {} };
    if (n.text_only) spec.text_only = true;
    else spec.voice = voice(n.voiceId);
    const dir = join(root, "tools", "assets", "dialogue", n.id);
    mkdirSync(dir, { recursive: true });
    writeFileSync(join(dir, "dialogue.json"), JSON.stringify(spec));
    if (!n.text_only) {
      const used = voice(n.recordedWith || n.voiceId);
      const hash = lineHash(used, n.recordedText || "Good day to you.");
      writeFileSync(join(dir, "elevenlabs.json"), JSON.stringify({ id: n.id, lines: { greet: { hash, voice_id: used.id, voice_name: used.name } } }));
      if (!n.noClip) {
        mkdirSync(join(pack, "Sounds", "Dialogue", n.id), { recursive: true });
        writeFileSync(join(pack, "Sounds", "Dialogue", n.id, `${n.id}_greet.wav`), "RIFF");
      }
    }
    writeFileSync(join(pack, "Prefabs", `Npc_${n.id}.et`), prefab("LF_NpcBaseEntity", dlgComponent(n.id)));
  }
  for (const [name, text] of extra) writeFileSync(join(pack, "Prefabs", name), text);
  return root;
}

const fails = (root, baseline = {}) => judge(scan(root), baseline);

let f = fails(repo([{ id: "baker", voiceId: "AAA" }]));
check(f.length === 0, "a recorded line in the dialogue's voice passes");

f = fails(repo([{ id: "baker", text_only: true }]));
check(f.length === 1 && /baker: 1 line\(s\) with no voice clip/.test(f[0]), "a text-only dialogue fails, naming the NPC");

f = fails(repo([{ id: "baker", voiceId: "AAA", noClip: true }]));
check(f.length === 1 && /clip .* is missing/.test(f[0]), "a missing clip fails");

f = fails(repo([{ id: "baker", voiceId: "AAA", recordedWith: "BBB" }]));
check(f.length === 1 && /recorded with voice/.test(f[0]), "a clip made with another voice fails");

f = fails(repo([{ id: "baker", voiceId: "AAA", recordedText: "Old words." }]));
check(f.length === 1 && /stale/.test(f[0]), "a clip of old text fails");

f = fails(repo([{ id: "baker", voiceId: "AAA" }, { id: "butcher", voiceId: "AAA" }]));
check(f.length === 1 && /each NPC keeps its own voice/.test(f[0]), "one voice for two NPCs fails");

const silent = [["Fence.et", prefab("LF_NpcBaseEntity", action("LF_ShopAction"))]];
f = fails(repo([{ id: "baker", voiceId: "AAA" }], silent));
check(f.length === 1 && /silent:Fence/.test(f[0]) && /no conversation at all/.test(f[0]), "a shop NPC with nothing to say fails");

f = fails(repo([{ id: "baker", voiceId: "AAA" }], silent), { "silent:Fence": "not yet" });
check(f.length === 0, "a baselined gap passes");

f = fails(repo([{ id: "baker", voiceId: "AAA" }]), { "baker": "stale entry" });
check(f.length === 1 && /baseline: baker is voiced or gone/.test(f[0]), "a baseline entry that is voiced fails (the list only shrinks)");

if (failed) {
  console.log(`dialogue-voices-selftest: ${failed} FAILED`);
  process.exit(1);
}
console.log("dialogue-voices-selftest: OK");

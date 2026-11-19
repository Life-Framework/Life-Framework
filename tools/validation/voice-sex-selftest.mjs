// tools/validation/voice-sex-selftest.mjs - validate-voice-sex and tools/lib/voice-sex.mjs (#3831).
// inputs: tools/validation/validate-voice-sex.mjs, tools/lib/voice-sex.mjs, tools/validation/voice-sex-selftest.mjs
//
// Builds a throwaway repo (two dialogue specs and the prefab that speaks one) and checks the validator:
//   - passes a male voice on the male body;
//   - fails a female voice on a body, naming the NPC, the voice and the prefab (the cafe owner as Odile with Mariva);
//   - passes a female voice that no body speaks (a line heard at a marker);
//   - fails a spec with no `voice.sex`, and one with a sex that is neither male nor female.
//
// red-proof: 2026-11-19, with the body-sex comparison in faults() removed the "female voice on a body fails" case printed FAIL.
import { spawnSync } from "node:child_process";
import { mkdirSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { scratchDir } from "../lib/scratch.mjs";

let failed = 0;
const check = (ok, msg) => {
  console.log(`${ok ? "PASS" : "FAIL"}  ${msg}`);
  if (!ok) failed++;
};

const validator = join(dirname(fileURLToPath(import.meta.url)), "validate-voice-sex.mjs");

function run(specs, prefabs) {
  const root = scratchDir("lf-voice-sex-");
  for (const [id, voice] of Object.entries(specs)) {
    mkdirSync(join(root, "tools", "assets", "dialogue", id), { recursive: true });
    writeFileSync(join(root, "tools", "assets", "dialogue", id, "dialogue.json"), JSON.stringify({ id, voice }));
  }
  mkdirSync(join(root, "addons", "Pack", "Configs"), { recursive: true });
  for (const [name, id] of Object.entries(prefabs)) writeFileSync(join(root, "addons", "Pack", "Configs", name), `GenericEntity {\n m_sDialogueId "${id}"\n}\n`);
  const r = spawnSync(process.execPath, [validator], { encoding: "utf8", env: { ...process.env, LF_VOICE_SEX_ROOT: root } });
  return { ok: r.status === 0, out: r.stdout + r.stderr };
}

let r = run({ cafe_owner: { name: "Marcel", sex: "male" } }, { "Cafe.et": "cafe_owner" });
check(r.ok, "a male voice on the male body passes");

r = run({ cafe_owner: { name: "Mariva", sex: "female" } }, { "Cafe.et": "cafe_owner" });
check(!r.ok && /cafe_owner/.test(r.out) && /Mariva/.test(r.out) && /Cafe\.et/.test(r.out), "a female voice on a body fails, naming the NPC, the voice and the prefab");

r = run({ priest: { name: "Mariva", sex: "female" } }, {});
check(r.ok, "a female voice no body speaks passes");

r = run({ cafe_owner: { name: "Marcel" } }, { "Cafe.et": "cafe_owner" });
check(!r.ok && /no voice\.sex/.test(r.out), "a voice with no declared sex fails");

r = run({ cafe_owner: { name: "Marcel", sex: "robot" } }, { "Cafe.et": "cafe_owner" });
check(!r.ok && /neither male nor female/.test(r.out), "an unknown sex fails");

if (failed) {
  console.log(`voice-sex-selftest: ${failed} FAILED`);
  process.exit(1);
}
console.log("voice-sex-selftest: OK");

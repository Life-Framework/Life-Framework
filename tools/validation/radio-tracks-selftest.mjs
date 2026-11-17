// The six period radio tracks (#1015) are ElevenLabs Music takes (replacing #1967's procedural ones).
// This proves ambience.json carries a music entry, a dated ElevenLabs record with the prompt, a pick and a
// credit row ("Music generated with ElevenLabs") for each, that each committed sample is mono 24 kHz, about
// a minute long and peaks near -3 dBFS, and that a 429 "system busy" from the service is retried.
// inputs: tools/assets/atmosphere/**, tools/assets/pipeline/**, addons/LF_Everon1989/Sounds/Atmosphere/**, docs/credits.md
// red-proof: 2026-11-16, each restored after its FAIL was seen:
//   the Radio-prefixed credit row for the kolo deleted from docs/credits.md -> FAIL credits (observed)
//   radio_accordion_kolo given a "synth" field in ambience.json -> FAIL ... procedural (observed)
//   callWithRetry changed to throw on the first 429 -> FAIL retry (observed)
import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { callWithRetry, samplePath } from "../assets/pipeline/ambience.mjs";
import { wavInfo } from "../assets/pipeline/sound.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const PACK = join(ROOT, "addons", "LF_Everon1989");
const IDS = ["radio_estrada_ballad", "radio_estrada_dance", "radio_accordion_waltz", "radio_accordion_kolo", "radio_orchestral_morning", "radio_orchestral_serenade"];
let failed = 0;
const ok = (cond, name) => {
  if (cond) return;
  console.log(`FAIL ${name}`);
  failed++;
};

const rec = JSON.parse(readFileSync(join(ROOT, "tools", "assets", "atmosphere", "ambience.json"), "utf8"));
const credits = readFileSync(join(ROOT, "docs", "credits.md"), "utf8");
for (const id of IDS) {
  const s = rec.sounds.find((x) => x.id === id);
  ok(!!s, `${id}: ambience.json has no sound with this id`);
  if (!s) continue;
  ok(s.kind === "music" && s.radio === true && s.rate === 24000, `${id}: must be a radio music entry at 24000 Hz`);
  ok(s.synth === undefined, `${id}: still names a procedural synth script; the tracks are ElevenLabs Music`);
  ok(Number.isInteger(s.pick) && !!s.reason, `${id}: needs a pick and a reason`);
  const g = (rec.generations || []).find((x) => x.id === id && x.take === s.pick);
  ok(!!g && /^\d{4}-\d{2}-\d{2}$/.test(g.date) && g.tool === "ElevenLabs Music" && /^music_v/.test(g.model) && g.prompt === s.prompt && g.credits > 0, `${id}: the picked take needs a dated ElevenLabs Music row with model, the prompt and its credits`);
  const file = samplePath(s);
  const row = credits.split("\n").find((l) => l.includes(file.split("/").pop()));
  ok(!!row && row.includes("Music generated with ElevenLabs"), `credits: docs/credits.md has no "Music generated with ElevenLabs" row for ${file}`);
  let info;
  try {
    info = wavInfo(readFileSync(join(PACK, file)));
  } catch {
    ok(false, `${id}: ${file} is not in LF_Everon1989`);
    continue;
  }
  ok(info.channels === 1 && info.rate === 24000 && info.bits === 16, `${id}: ${file} must be 16-bit mono 24 kHz`);
  ok(info.seconds >= 50 && info.seconds <= 70, `${id}: ${file} is ${info.seconds.toFixed(1)} s, about 60 s expected`);
  ok(info.peakDb <= -2.5 && info.peakDb >= -3.5, `${id}: ${file} peaks at ${info.peakDb.toFixed(1)} dBFS, -3 expected`);
}

// A 429 system_busy is retried, a 4xx that is the request's own is not.
const keyFile = join(ROOT, "tmp", "radio-tracks-selftest-key");
mkdirSync(dirname(keyFile), { recursive: true });
writeFileSync(keyFile, "selftest-not-a-key");
process.env.LF_ELEVENLABS_KEY_FILE = keyFile;
const res = (status, body = "x") => ({ ok: status < 400, status, text: async () => body, arrayBuffer: async () => new ArrayBuffer(4), headers: { get: () => "" } });
const quiet = console.log;
console.log = () => {};
let calls = 0;
const busyThenOk = async () => (++calls < 3 ? res(429, "system_busy") : res(200));
const got = await callWithRetry({ path: "/v1/music", body: {} }, { fetchImpl: busyThenOk }, 1).catch((e) => e);
let hardCalls = 0;
const hard = await callWithRetry({ path: "/v1/music", body: {} }, { fetchImpl: async () => (++hardCalls, res(422, "bad")) }, 1).catch((e) => e);
let alwaysBusy = 0;
const spent = await callWithRetry({ path: "/v1/music", body: {} }, { fetchImpl: async () => (++alwaysBusy, res(429, "system_busy")) }, 1, 3).catch((e) => e);
console.log = quiet;
ok(calls === 3 && got?.ok === true, `retry: two 429s then a 200 should succeed on the third call (calls ${calls})`);
ok(hardCalls === 1 && hard instanceof Error, `retry: a 422 must fail at once (calls ${hardCalls})`);
ok(alwaysBusy === 3 && spent instanceof Error, `retry: a service that stays busy must give up after the tries (calls ${alwaysBusy})`);

if (failed) {
  console.log(`radio-tracks-selftest: FAILED (${failed})`);
  process.exit(1);
}
console.log(`radio-tracks-selftest: OK (${IDS.length} tracks match their record, credit rows and sample format; the busy retry works)`);

// The hospital robbery alarm sample (Samples/Alarm_Robbery_LF_ALARM_HOSPITAL_LP_01.wav) is synthesised by
// tools/assets/hospital-alarm/synth.mjs; this proves the committed file is exactly what the script writes
// today, that the notes, rests, loop seam and level are the design (a rising G5, B5, D6 chime, then a long
// rest), and that it is its own sound: none of the other robbery alarm clips carries any of its pitches.
// inputs: tools/assets/hospital-alarm/**, addons/LifeFramework/Sounds/Alarm/Robbery/Samples/**
// red-proof: 2026-11-16, each restored after its FAIL was seen:
//   NOTES_HZ[0] changed to 700 in synth.mjs -> FAIL committed: the wav is what the synth writes (observed)
//   the seam case feeds check() a loop that starts mid-wave and needs it flagged (built in, no edit)
import { readFileSync, readdirSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { check, synth, wav, share, RATE, CYCLES, CALL_MS, NOTES_HZ } from "../assets/hospital-alarm/synth.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const DIR = join(ROOT, "addons", "LifeFramework", "Sounds", "Alarm", "Robbery", "Samples");
const WAV = join(DIR, "Alarm_Robbery_LF_ALARM_HOSPITAL_LP_01.wav");
let failed = 0;
const ok = (cond, name) => {
  console.log(`${cond ? "ok" : "FAIL"} ${name}`);
  if (!cond) failed++;
};

//! Mono 16-bit samples of a wav, the first channel of a wider one.
function pcmOf(buf) {
  const ch = buf.readUInt16LE(22);
  const n = Math.floor((buf.length - 44) / (2 * ch));
  const out = new Int16Array(n);
  for (let i = 0; i < n; i++) out[i] = buf.readInt16LE(44 + i * 2 * ch);
  return out;
}

//! The highest share any hospital note pitch takes of a clip's power: a clip with none of the pitches reads near 0.
function noteShare(pcm) {
  return Math.max(...NOTES_HZ.map((hz) => share(pcm, 0, pcm.length, hz)));
}

const pcm = synth();
ok(check(pcm).length === 0, `design: ${check(pcm).join("; ") || "three rising notes, silent rests and seam, level"}`);
ok(pcm.length === Math.round((CALL_MS / 1000) * RATE) * CYCLES, "length: whole calls, so the loop wraps on the rest");
ok(readFileSync(WAV).equals(wav(pcm)), "committed: the wav is what the synth writes (rerun tools/assets/hospital-alarm/synth.mjs)");

const bad = pcm.slice();
bad[0] = 12000;
ok(check(bad).some((f) => f.includes("seam")), "seam: a loop that starts mid-wave is flagged");

const others = readdirSync(DIR).filter((f) => f.endsWith(".wav") && !f.includes("HOSPITAL"));
ok(others.length >= 4, `distinct: found the other robbery clips (${others.length})`);
for (const f of others) {
  const s = noteShare(pcmOf(readFileSync(join(DIR, f))));
  ok(s < 0.02, `distinct: ${f} carries none of the hospital pitches (${(s * 100).toFixed(2)}% of its power)`);
}
ok(noteShare(pcmOf(readFileSync(WAV))) >= 0.005, "distinct: the same measure does see the hospital clip's own pitches");

if (failed) {
  console.log(`hospital-alarm-selftest: FAILED (${failed})`);
  process.exit(1);
}
console.log("hospital-alarm-selftest: OK");

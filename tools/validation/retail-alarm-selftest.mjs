// The retail-store alarm sample (Samples/Alarm_Robbery_LF_ALARM_STORE_LP_01.wav) is synthesised by
// tools/assets/retail-alarm/synth.mjs; this proves the committed file is exactly what the script writes
// today and that the tones, loop seam and level are the design: two tones, 1400 then 1000 Hz, 350 ms each.
// inputs: tools/assets/retail-alarm/**, addons/LifeFramework/Sounds/Alarm/Robbery/Samples/**
// red-proof: 2026-11-16, each restored after its FAIL was seen:
//   HIGH_HZ changed to 1300 in synth.mjs  -> FAIL committed: the wav is what the synth writes (observed)
//   the seam case feeds check() a loop that starts mid-wave and needs it flagged (built in, no edit)
import { readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { check, synth, wav, RATE, CYCLES, SEGMENT_MS } from "../assets/retail-alarm/synth.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const WAV = join(ROOT, "addons", "LifeFramework", "Sounds", "Alarm", "Robbery", "Samples", "Alarm_Robbery_LF_ALARM_STORE_LP_01.wav");
let failed = 0;
const ok = (cond, name) => {
  console.log(`${cond ? "ok" : "FAIL"} ${name}`);
  if (!cond) failed++;
};

const pcm = synth();
ok(check(pcm).length === 0, `tones: ${check(pcm).join("; ") || "two tones, silent seam, level"}`);
ok(pcm.length === Math.round((SEGMENT_MS / 1000) * RATE) * 2 * CYCLES, "length: whole cycles, so the loop wraps on a tone boundary");
ok(readFileSync(WAV).equals(wav(pcm)), "committed: the wav is what the synth writes (rerun tools/assets/retail-alarm/synth.mjs)");

const bad = pcm.slice();
bad[0] = 12000;
ok(check(bad).some((f) => f.includes("seam")), "seam: a loop that starts mid-wave is flagged");

if (failed) {
  console.log(`retail-alarm-selftest: FAILED (${failed})`);
  process.exit(1);
}
console.log("retail-alarm-selftest: OK");

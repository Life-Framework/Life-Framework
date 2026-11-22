// The jailbreak siren sample (Sounds/Alarm/Jail/Samples/Alarm_Jail_LF_ALARM_JAILBREAK_LP_01.wav) is synthesised by
// tools/assets/jail-siren/synth.mjs; this proves the committed file is exactly what the script writes today and that
// the blasts, rests, loop seam and level are the design.
// inputs: tools/assets/jail-siren/**, addons/LifeFramework/Sounds/Alarm/Jail/Samples/**
// red-proof: the seam case feeds check() a loop that starts mid-wave and needs it flagged (built in, no edit).
import { readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { check, synth, wav, RATE, PERIOD_MS, BLASTS } from "../assets/jail-siren/synth.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const WAV = join(ROOT, "addons", "LifeFramework", "Sounds", "Alarm", "Jail", "Samples", "Alarm_Jail_LF_ALARM_JAILBREAK_LP_01.wav");
let failed = 0;
const ok = (cond, name) => {
  console.log(`${cond ? "ok" : "FAIL"} ${name}`);
  if (!cond) failed++;
};
const pcm = synth();
ok(check(pcm).length === 0, `design: ${check(pcm).join("; ") || "four 196 Hz blasts, silent rests and seam, level"}`);
ok(pcm.length === Math.round((PERIOD_MS / 1000) * RATE) * BLASTS, "length: whole periods, so the loop wraps on the rest");
ok(readFileSync(WAV).equals(wav(pcm)), "committed: the wav is what the synth writes (rerun tools/assets/jail-siren/synth.mjs)");
const cut = pcm.slice(Math.round(0.35 * RATE));
ok(check(cut).length > 0, "a loop that starts mid-blast is flagged");
console.log(failed ? "SUMMARY jail-siren-selftest fail" : "SUMMARY jail-siren-selftest ok");
process.exit(failed ? 1 : 0);

#!/usr/bin/env node
// The jailbreak siren (jail kind in LF_Police.conf): the prison klaxon that sounds when a charge goes off on the
// jail's outer door or cell door. Four low buzzer blasts, 0.7 s on and 0.3 s off, a 196 Hz sawtooth (harmonics
// 1 to 6 at 1/n) with a slow 5 Hz wobble of 0.8% so it reads as an electric horn, not a tone. Synthesised, not
// generated: no model, no sample, no licence, the same bytes every run. Each blast has a 12 ms attack and release
// and the sound ends on the rest, so the loop seam is silent by construction. Mono 16-bit 48 kHz, the format of
// the other alarm samples.
//
// Built to sound unlike the other kinds: the bank is a struck bell, the fuel alarm a shrill pulsed buzz, the retail
// alarm two hard alternating square tones, the hospital a soft rising chime, the van one honk, the plain siren a
// sweeping wail. Here: a low steady buzz, 196 Hz, four blasts of one pitch with a regular gap.
//
//   node tools/assets/jail-siren/synth.mjs [--out <wav>] [--check]
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

export const RATE = 48000;
export const BASE_HZ = 196;
export const BLASTS = 4;
export const ON_MS = 700;
export const PERIOD_MS = 1000;
export const PEAK = 0.7;
const ATTACK_MS = 12;
const HARMONICS = 6;
const WOBBLE_HZ = 5;
const WOBBLE_DEPTH = 0.008;

const here = path.dirname(fileURLToPath(import.meta.url));
const DEFAULT_OUT = path.resolve(here, "../../../addons/LifeFramework/Sounds/Alarm/Jail/Samples/Alarm_Jail_LF_ALARM_JAILBREAK_LP_01.wav");

export function synth() {
  const period = Math.round((PERIOD_MS / 1000) * RATE);
  const on = Math.round((ON_MS / 1000) * RATE);
  const att = Math.round((ATTACK_MS / 1000) * RATE);
  const pcm = new Float64Array(period * BLASTS);
  let phase = 0;
  for (let b = 0; b < BLASTS; b++) {
    for (let i = 0; i < on; i++) {
      const t = i / RATE;
      const hz = BASE_HZ * (1 + WOBBLE_DEPTH * Math.sin(2 * Math.PI * WOBBLE_HZ * t));
      phase += (2 * Math.PI * hz) / RATE;
      let v = 0;
      for (let h = 1; h <= HARMONICS; h++) v += Math.sin(h * phase) / h;
      let g = 1;
      if (i < att) g = i / att;
      if (i >= on - att) g = (on - 1 - i) / att;
      pcm[b * period + i] = v * g;
    }
  }
  let peak = 0;
  for (const v of pcm) peak = Math.max(peak, Math.abs(v));
  const out = new Int16Array(pcm.length);
  for (let i = 0; i < pcm.length; i++) out[i] = Math.round((pcm[i] / peak) * PEAK * 32767);
  return out;
}

export function wav(pcm) {
  const data = Buffer.alloc(pcm.length * 2);
  for (let i = 0; i < pcm.length; i++) data.writeInt16LE(pcm[i], i * 2);
  const head = Buffer.alloc(44);
  head.write("RIFF", 0);
  head.writeUInt32LE(36 + data.length, 4);
  head.write("WAVEfmt ", 8);
  head.writeUInt32LE(16, 16);
  head.writeUInt16LE(1, 20);
  head.writeUInt16LE(1, 22);
  head.writeUInt32LE(RATE, 24);
  head.writeUInt32LE(RATE * 2, 28);
  head.writeUInt16LE(2, 32);
  head.writeUInt16LE(16, 34);
  head.write("data", 36);
  head.writeUInt32LE(data.length, 40);
  return Buffer.concat([head, data]);
}

//! Fraction of the samples that are audible (above 2% of full scale).
export function audibleFraction(pcm) {
  let n = 0;
  for (const v of pcm) if (Math.abs(v) > 655) n++;
  return n / pcm.length;
}

//! Zero-crossing pitch of the middle of a blast: the buzzer's fundamental.
export function pitchOfBlast(pcm, b) {
  const period = Math.round((PERIOD_MS / 1000) * RATE);
  const from = b * period + Math.round(0.2 * RATE);
  const to = b * period + Math.round(0.5 * RATE);
  let up = 0;
  for (let i = from + 1; i < to; i++) if (pcm[i - 1] < 0 && pcm[i] >= 0) up++;
  return up / ((to - from) / RATE);
}

export function check(pcm) {
  const fails = [];
  const period = Math.round((PERIOD_MS / 1000) * RATE);
  if (pcm.length !== period * BLASTS) fails.push(`length ${pcm.length} is not ${BLASTS} whole periods`);
  for (let b = 0; b < BLASTS; b++) {
    const hz = pitchOfBlast(pcm, b);
    if (Math.abs(hz - BASE_HZ) > BASE_HZ * 0.06) fails.push(`blast ${b}: pitch ${hz.toFixed(0)} Hz, expected about ${BASE_HZ} Hz`);
    let restPeak = 0;
    for (let i = b * period + Math.round((ON_MS / 1000) * RATE) + 50; i < (b + 1) * period; i++) restPeak = Math.max(restPeak, Math.abs(pcm[i]));
    if (restPeak / 32768 > 0.01) fails.push(`blast ${b}: the rest is not silent`);
  }
  const audible = audibleFraction(pcm);
  if (audible < 0.4 || audible > 0.75) fails.push(`${(audible * 100).toFixed(0)}% of the loop is audible; the design is about 70%`);
  const edge = Math.max(Math.abs(pcm[0]), Math.abs(pcm[pcm.length - 1])) / 32768;
  if (edge > 0.01) fails.push(`loop seam is ${(edge * 100).toFixed(1)}% of full scale; expected silence`);
  let peak = 0;
  for (const v of pcm) peak = Math.max(peak, Math.abs(v));
  if (peak / 32768 > 0.75 || peak / 32768 < 0.6) fails.push(`peak ${(peak / 327.68).toFixed(0)}% of full scale; expected about 70%`);
  return fails;
}

if (process.argv[1] && path.resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const args = process.argv.slice(2);
  const outAt = args.indexOf("--out");
  const out = outAt >= 0 ? path.resolve(args[outAt + 1]) : DEFAULT_OUT;
  const pcm = synth();
  const fails = check(pcm);
  if (fails.length) {
    for (const f of fails) console.error("synth: " + f);
    console.error("SUMMARY jail-siren fail");
    process.exit(1);
  }
  if (!args.includes("--check")) {
    fs.mkdirSync(path.dirname(out), { recursive: true });
    fs.writeFileSync(out, wav(pcm));
    console.log(`wrote ${out}`);
  }
  console.log(`SUMMARY jail-siren ok ${(pcm.length / RATE).toFixed(2)} s, ${BASE_HZ} Hz x ${BLASTS}, ${pcm.length * 2 + 44} bytes`);
}

#!/usr/bin/env node
// The hospital robbery alarm (hospital kind in LF_Police.conf): a slow rising triple chime, the
// nurses'-station "code" call a 1980s ward used. Three soft bell-like notes climbing G5, B5, D6, each
// struck and left to ring out, then a long rest before the next call. Synthesised, not generated: no
// model, no sample, no licence, the same bytes every run. Each note is a sine with a quiet second and
// third partial and a fast-attack exponential decay, starting at zero phase and dying to silence, so
// the loop seam is silent by construction. Mono 16-bit 48 kHz, the format of the other Alarm_Robbery
// samples.
//
// It is built to sound unlike the other kinds: the retail alarm (tools/assets/retail-alarm) is two
// hard square tones alternating without a gap, the bank is a clanging bell at a steady strike rate,
// the fuel alarm is a shrill pulsed buzz, the van a single horn honk and the police siren a sweeping
// wail. Here: pure soft tones, three rising pitches, a decay on each and more than a third of every
// call left silent.
//
//   node tools/assets/hospital-alarm/synth.mjs [--out <wav>] [--check]
//
// --check synthesises to memory, measures each note's pitch with a Goertzel filter, the rests and the
// loop seam, and exits 1 when the numbers drift from the design (the selftest runs it).
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

export const RATE = 48000;
export const NOTES_HZ = [784, 988, 1175];
export const NOTE_MS = 320;
export const CALL_MS = 1800;
export const CYCLES = 3;
export const PEAK = 0.7;
const ATTACK_MS = 6;
const DECAY_TAU_MS = 90;
const PARTIALS = [1, 0.18, 0.06];

const here = path.dirname(fileURLToPath(import.meta.url));
const DEFAULT_OUT = path.resolve(here, "../../../addons/LifeFramework/Sounds/Alarm/Robbery/Samples/Alarm_Robbery_LF_ALARM_HOSPITAL_LP_01.wav");

//! One struck note at hz, NOTE_MS long: fast attack, exponential decay, silent at the end.
function note(hz) {
  const samples = Math.round((NOTE_MS / 1000) * RATE);
  const attack = Math.round((ATTACK_MS / 1000) * RATE);
  const out = new Float64Array(samples);
  for (let i = 0; i < samples; i++) {
    const t = i / RATE;
    let v = 0;
    for (let p = 0; p < PARTIALS.length; p++) v += PARTIALS[p] * Math.sin(2 * Math.PI * hz * (p + 1) * t);
    let g = Math.exp(-(t * 1000) / DECAY_TAU_MS);
    if (i < attack) g *= i / attack;
    // A short fade over the last 10 ms takes the tail the rest of the way to zero.
    const tail = Math.round(0.01 * RATE);
    if (i >= samples - tail) g *= (samples - 1 - i) / tail;
    out[i] = v * g;
  }
  return out;
}

//! Where note n starts inside one call, in samples: the notes follow each other without a gap.
export function noteStart(n) {
  return n * Math.round((NOTE_MS / 1000) * RATE);
}

export function synth() {
  const call = Math.round((CALL_MS / 1000) * RATE);
  const pcm = new Float64Array(call * CYCLES);
  for (let c = 0; c < CYCLES; c++)
    NOTES_HZ.forEach((hz, n) => pcm.set(note(hz), c * call + noteStart(n)));
  let peak = 0;
  for (const v of pcm) peak = Math.max(peak, Math.abs(v));
  const scale = PEAK / peak;
  const out = new Int16Array(pcm.length);
  for (let i = 0; i < pcm.length; i++) out[i] = Math.round(pcm[i] * scale * 32767);
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

//! Goertzel power of one frequency over a stretch of samples, relative to the stretch's total power.
export function share(pcm, from, to, hz) {
  const w = (2 * Math.PI * hz) / RATE;
  const k = 2 * Math.cos(w);
  let s1 = 0, s2 = 0, total = 0;
  for (let i = from; i < to; i++) {
    const s = pcm[i] / 32768 + k * s1 - s2;
    s2 = s1;
    s1 = s;
    total += (pcm[i] / 32768) ** 2;
  }
  const power = s1 * s1 + s2 * s2 - k * s1 * s2;
  return (power * 2) / ((to - from) * total || 1);
}

//! Fraction of the samples that are audible (above 2% of full scale): how much of the loop is sound, not rest.
export function audibleFraction(pcm) {
  let n = 0;
  for (const v of pcm) if (Math.abs(v) > 655) n++;
  return n / pcm.length;
}

export function check(pcm) {
  const call = Math.round((CALL_MS / 1000) * RATE);
  const fails = [];
  for (let c = 0; c < CYCLES; c++) {
    NOTES_HZ.forEach((hz, n) => {
      const a = c * call + noteStart(n) + Math.round(0.01 * RATE);
      const b = a + Math.round(0.15 * RATE);
      const on = share(pcm, a, b, hz);
      if (on < 0.6) fails.push(`call ${c} note ${n}: ${hz} Hz carries only ${(on * 100).toFixed(0)}% of the power`);
      for (const other of NOTES_HZ) {
        if (other === hz) continue;
        const leak = share(pcm, a, b, other);
        if (leak > on / 20) fails.push(`call ${c} note ${n}: ${other} Hz leaks ${(leak * 100).toFixed(1)}% beside ${hz} Hz`);
      }
    });
    // The rest after the third note: silence, so the call reads as a call and the loop gap is clear.
    const restFrom = c * call + noteStart(NOTES_HZ.length);
    let restPeak = 0;
    for (let i = restFrom; i < (c + 1) * call; i++) restPeak = Math.max(restPeak, Math.abs(pcm[i]));
    if (restPeak / 32768 > 0.01) fails.push(`call ${c}: the rest is not silent (${(restPeak / 327.68).toFixed(1)}% of full scale)`);
  }
  const audible = audibleFraction(pcm);
  if (audible > 0.5) fails.push(`${(audible * 100).toFixed(0)}% of the loop is audible; the design leaves over half of it silent`);
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
    console.error("SUMMARY hospital-alarm fail");
    process.exit(1);
  }
  if (!args.includes("--check")) {
    fs.writeFileSync(out, wav(pcm));
    console.log(`wrote ${out}`);
  }
  console.log(`SUMMARY hospital-alarm ok ${(pcm.length / RATE).toFixed(2)} s, ${NOTES_HZ.join("/")} Hz x ${CYCLES}, ${pcm.length * 2 + 44} bytes`);
}

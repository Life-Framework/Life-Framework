#!/usr/bin/env node
// The retail-store burglar alarm (store kind in LF_Police.conf): the electronic two-tone "ee-aw" a
// 1989 shop's alarm box makes. Synthesised, not generated: no model, no licence, the same bytes every
// run. Two hard-switched tones from a piezo-style square wave (odd harmonics only, band-limited so
// nothing aliases), each segment starting and ending at zero phase with a 3 ms ramp, so the loop seam
// is silent by construction. Mono 16-bit 48 kHz, the format of the other Alarm_Robbery samples.
//
//   node tools/assets/retail-alarm/synth.mjs [--out <wav>] [--check]
//
// --check synthesises to memory, measures the two tone frequencies with a Goertzel filter and the loop
// seam, and exits 1 when the numbers drift from the design (the selftest runs it).
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

export const RATE = 48000;
export const HIGH_HZ = 1400;
export const LOW_HZ = 1000;
export const SEGMENT_MS = 350;
export const CYCLES = 4;
export const PEAK = 0.7;
const RAMP_MS = 3;
const HARMONICS_UP_TO_HZ = 9000;

const here = path.dirname(fileURLToPath(import.meta.url));
const DEFAULT_OUT = path.resolve(here, "../../../addons/LifeFramework/Sounds/Alarm/Robbery/Samples/Alarm_Robbery_LF_ALARM_STORE_LP_01.wav");

//! One segment of a band-limited square wave at hz.
function tone(hz, samples) {
  const out = new Float64Array(samples);
  const ramp = Math.round((RAMP_MS / 1000) * RATE);
  for (let i = 0; i < samples; i++) {
    const t = i / RATE;
    let v = 0;
    for (let n = 1; n * hz <= HARMONICS_UP_TO_HZ; n += 2) v += Math.sin(2 * Math.PI * n * hz * t) / n;
    let g = 1;
    if (i < ramp) g = i / ramp;
    else if (i >= samples - ramp) g = (samples - 1 - i) / ramp;
    out[i] = v * g;
  }
  return out;
}

export function synth() {
  const seg = Math.round((SEGMENT_MS / 1000) * RATE);
  const hi = tone(HIGH_HZ, seg);
  const lo = tone(LOW_HZ, seg);
  const pcm = new Float64Array(seg * 2 * CYCLES);
  for (let c = 0; c < CYCLES; c++) {
    pcm.set(hi, seg * 2 * c);
    pcm.set(lo, seg * (2 * c + 1));
  }
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

export function check(pcm) {
  const seg = Math.round((SEGMENT_MS / 1000) * RATE);
  const margin = Math.round(0.02 * RATE);
  const fails = [];
  for (let i = 0; i < CYCLES * 2; i++) {
    const want = i % 2 === 0 ? HIGH_HZ : LOW_HZ;
    const other = i % 2 === 0 ? LOW_HZ : HIGH_HZ;
    const a = i * seg + margin, b = (i + 1) * seg - margin;
    const on = share(pcm, a, b, want);
    const off = share(pcm, a, b, other);
    if (on < 0.3) fails.push(`segment ${i}: ${want} Hz carries only ${(on * 100).toFixed(0)}% of the power`);
    if (off > on / 20) fails.push(`segment ${i}: ${other} Hz leaks ${(off * 100).toFixed(1)}% beside the ${want} Hz tone`);
  }
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
    console.error("SUMMARY retail-alarm fail");
    process.exit(1);
  }
  if (!args.includes("--check")) {
    fs.writeFileSync(out, wav(pcm));
    console.log(`wrote ${out}`);
  }
  console.log(`SUMMARY retail-alarm ok ${(pcm.length / RATE).toFixed(2)} s, ${HIGH_HZ}/${LOW_HZ} Hz x ${CYCLES}, ${pcm.length * 2 + 44} bytes`);
}

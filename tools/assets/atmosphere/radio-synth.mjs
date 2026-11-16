// Six period radio tracks (#1967), composed and rendered by this script: no sample, no recording,
// no model, no outside music. Every note, chord and drum hit comes from the tables below and a seeded
// random generator, so the committed WAVs are this file's output and nobody else's work.
//
//   node tools/assets/atmosphere/radio-synth.mjs [--id <sound id>] [--out <dir>] [--record]
//
// Each track is a small band (reed accordion, clarinet, strings, electric piano, plucked guitar,
// bass, drum machine or brushes) playing 4-bar phrases over a chord loop in the style of late-1980s
// Eastern European / Mediterranean regional radio: estrada ballad and dance numbers, an accordion waltz
// and kolo, light orchestral. The melody is generated per phrase from a rhythm cell and a seed; chords
// and bass follow the progression. The mix is rendered as a circular buffer (the reverb tail folds onto
// the start), so the loop closes with no crossfade, then squeezed through a small-radio-speaker band
// (180 Hz to 5.2 kHz) and a soft clipper.
//
// `--record` fills the "generations" rows in ambience.json for these sounds (tool, date, measured
// levels). docs/credits.md says the same: original work, same licence as the repo.
import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { encodeWav, wavInfo } from "../pipeline/sound.mjs";
import { wrapRatio } from "../pipeline/ambience.mjs";

export const RATE = 24000;
const TAU = Math.PI * 2;
const SCALES = {
  major: [0, 2, 4, 5, 7, 9, 11],
  minor: [0, 2, 3, 5, 7, 8, 10],
  harmonic: [0, 2, 3, 5, 7, 8, 11],
  dorian: [0, 2, 3, 5, 7, 9, 10],
  hijaz: [0, 1, 4, 5, 7, 8, 10], // phrygian dominant: the Balkan and Aegean colour
};
const QUALITY = { M: [0, 4, 7], m: [0, 3, 7], 7: [0, 4, 7, 10], m7: [0, 3, 7, 10], M7: [0, 4, 7, 11], dim: [0, 3, 6] };

// ---------------------------------------------------------------- the six tracks
// prog: one chord per bar, [semitones above the key, quality]. cells: rhythm cells in beats that fill one
// bar (negative is a rest); the last cell of `ends` closes a phrase.
export const TRACKS = {
  radio_estrada_ballad: {
    seed: 1989, bpm: 92, beats: 4, root: 57, scale: "harmonic", lo: 64, hi: 83, form: "AaBa", breakPhrase: -1,
    prog: { A: [[0, "m"], [8, "M"], [3, "M"], [7, "7"]], B: [[5, "m"], [0, "m"], [8, "M"], [7, "7"]] },
    cells: [[1.5, 0.5, 1, 1], [1, 1, 1, 1], [0.5, 0.5, 1, 2], [2, 1, 0.5, 0.5]], ends: [[1, 1, 2], [1.5, 0.5, 2]],
    lead: "flute", harmony: false, pad: "strings", padGain: 0.55, comp: "epiano", bass: "pluck", drums: "estrada80", leadGain: 1, reverb: 0.5,
  },
  radio_estrada_dance: {
    seed: 4021, bpm: 122, beats: 4, root: 62, scale: "minor", lo: 69, hi: 86, form: "AABA", breakPhrase: 5,
    prog: { A: [[0, "m"], [0, "m"], [8, "M"], [10, "M"]], B: [[5, "m"], [5, "m"], [10, "M"], [7, "M"]] },
    cells: [[0.5, 0.5, 0.5, 0.5, 1, 1], [0.5, 0.5, 1, 0.5, 0.5, 1], [1, 0.5, 0.5, 1, 1], [0.75, 0.75, 0.5, 1, 1]], ends: [[1, 1, 2], [0.5, 0.5, 1, 2]],
    lead: "synth", harmony: false, pad: "strings", padGain: 0.35, comp: "synthArp", bass: "synth", drums: "dance", leadGain: 0.9, reverb: 0.3,
  },
  radio_accordion_waltz: {
    seed: 7310, bpm: 150, beats: 3, root: 53, scale: "major", lo: 65, hi: 84, form: "AaBa", breakPhrase: -1,
    prog: { A: [[0, "M"], [5, "M"], [7, "7"], [0, "M"]], B: [[9, "m"], [2, "m"], [7, "7"], [0, "M"]] },
    cells: [[1.5, 0.5, 1], [1, 1, 1], [2, 1], [1, 0.5, 0.5, 1], [0.5, 0.5, 1, 1]], ends: [[1, 2], [3]],
    lead: "accordion", harmony: true, pad: "none", padGain: 0, comp: "oompahAccordion", bass: "oompah", drums: "waltz", leadGain: 1, reverb: 0.35,
  },
  radio_accordion_kolo: {
    seed: 2468, bpm: 124, beats: 2, root: 50, scale: "hijaz", lo: 62, hi: 83, form: "AAaB", breakPhrase: -1,
    prog: { A: [[0, "M"], [0, "M"], [5, "m"], [0, "M"]], B: [[5, "m"], [0, "M"], [10, "M"], [0, "M"]] },
    cells: [[0.5, 0.5, 0.5, 0.5], [0.5, 0.25, 0.25, 0.5, 0.5], [1, 0.5, 0.5], [0.25, 0.25, 0.5, 1]], ends: [[1, 1], [2]],
    lead: "clarinet", harmony: true, pad: "none", padGain: 0, comp: "oompahAccordion", bass: "oompah", drums: "kolo", leadGain: 1, reverb: 0.25,
  },
  radio_orchestral_morning: {
    seed: 5150, bpm: 104, beats: 4, root: 55, scale: "major", lo: 67, hi: 88, form: "AaBa", breakPhrase: -1,
    prog: { A: [[0, "M"], [9, "m"], [5, "M"], [7, "7"]], B: [[2, "m"], [7, "M"], [9, "m"], [5, "M"]] },
    cells: [[1, 1, 0.5, 0.5, 1], [2, 1, 1], [1.5, 0.5, 1, 1], [1, 1, 1, 1]], ends: [[1, 3], [2, 2]],
    lead: "flute", harmony: false, pad: "strings", padGain: 0.7, comp: "harp", bass: "pluck", drums: "soft", leadGain: 1, reverb: 0.55,
  },
  radio_orchestral_serenade: {
    seed: 9034, bpm: 88, beats: 4, root: 57, scale: "dorian", lo: 62, hi: 81, form: "AaBA", breakPhrase: -1,
    prog: { A: [[0, "m7"], [0, "m7"], [5, "M"], [0, "m7"]], B: [[7, "m"], [5, "M"], [3, "M"], [7, "7"]] },
    cells: [[1.5, 1.5, 1], [1, 1, 2], [0.5, 0.5, 1, 1, 1], [2, 1, 1]], ends: [[2, 2], [1, 3]],
    lead: "clarinet", harmony: false, pad: "strings", padGain: 0.6, comp: "strum", bass: "pluck", drums: "bolero", leadGain: 1, reverb: 0.5,
  },
};

// ---------------------------------------------------------------- tools
function rngOf(seed) {
  let s = seed >>> 0;
  return () => {
    s = (s + 0x6d2b79f5) >>> 0;
    let t = s;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}
const mtof = (m) => 440 * 2 ** ((m - 69) / 12);
const mod = (a, n) => ((a % n) + n) % n;

// A bar of the mix: `dry` plus a `send` that feeds the reverb. Notes are rendered into small arrays.
function put(m, start, arr, gain, send) {
  const n = Math.min(arr.length, m.dry.length - start);
  for (let i = 0; i < n; i++) {
    const v = arr[i] * gain;
    m.dry[start + i] += v;
    m.send[start + i] += v * send;
  }
}
const lenOf = (dur, rel) => Math.max(8, Math.ceil((dur + rel) * RATE));
const gate = (t, a, dur, r) => Math.min(1, t / a) * (t < dur ? 1 : Math.max(0, 1 - (t - dur) / r));

// A bank of detuned reeds: the accordion, the pad, the clarinet colour are all this with other numbers.
function reeds(f, dur, { voices, harm, a, r, trem = 0, tremHz = 5, vib = 0, vibHz = 5.4, vibDelay = 0.2, octDown = 0, breath = 0 }, rnd) {
  const n = lenOf(dur, r);
  const out = new Float32Array(n);
  const K = Math.max(1, Math.min(harm.length, Math.floor(4800 / f)));
  const ph = voices.map(() => rnd() * TAU);
  for (let i = 0; i < n; i++) {
    const t = i / RATE;
    const g = gate(t, a, dur, r);
    if (g <= 0) continue;
    const bend = 1 + (t > vibDelay ? vib * Math.sin(TAU * vibHz * t) * Math.min(1, (t - vibDelay) / 0.3) : 0);
    let s = 0;
    for (let v = 0; v < voices.length; v++) {
      ph[v] += (TAU * f * voices[v][0] * bend) / RATE;
      let x = 0;
      for (let k = 1; k <= K; k++) x += harm[k - 1] * Math.sin(k * ph[v]);
      s += x * voices[v][1];
    }
    if (octDown) {
      const p = (TAU * f * 0.5 * t) % TAU;
      for (let k = 1; k <= Math.min(5, K); k++) s += octDown * harm[k - 1] * Math.sin(k * p);
    }
    if (breath) s += (rnd() - 0.5) * breath;
    out[i] = s * g * (1 + trem * Math.sin(TAU * tremHz * t));
  }
  return out;
}
const powH = (n, p, odd = false) => Array.from({ length: n }, (_, i) => (odd && i % 2 === 1 ? 0.2 : 1) / (i + 1) ** p);

const VOICES = {
  // Musette tuning: one reed in tune, two a few cents either side, plus the low octave.
  accordion: (f, dur, rnd) => reeds(f, dur, { voices: [[0.9965, 0.4], [1, 0.5], [1.0038, 0.4]], harm: powH(14, 0.85), a: 0.03, r: 0.07, trem: 0.05, tremHz: 5.4, octDown: 0.18 }, rnd),
  accordionChord: (f, dur, rnd) => reeds(f, dur, { voices: [[0.997, 0.5], [1.003, 0.5]], harm: powH(10, 1), a: 0.012, r: 0.05 }, rnd),
  clarinet: (f, dur, rnd) => reeds(f, dur, { voices: [[1, 1]], harm: powH(13, 1.05, true).map((v, i) => (i % 2 === 0 ? v : v * 0.5)), a: 0.05, r: 0.1, vib: 0.004, breath: 0.03 }, rnd),
  flute: (f, dur, rnd) => reeds(f, dur, { voices: [[1, 1]], harm: [1, 0.18, 0.07, 0.03, 0.015], a: 0.06, r: 0.14, vib: 0.006, vibDelay: 0.15, breath: 0.05 }, rnd),
  synth: (f, dur, rnd) => reeds(f, dur, { voices: [[0.997, 0.5], [1.003, 0.5]], harm: powH(16, 1), a: 0.012, r: 0.1, vib: 0.003, vibDelay: 0.3 }, rnd),
  strings: (f, dur, rnd) => reeds(f, dur, { voices: [[0.9975, 0.33], [1, 0.33], [1.0028, 0.33]], harm: powH(9, 1.3), a: 0.22, r: 0.4, trem: 0.05, tremHz: 0.35, vib: 0.003, vibDelay: 0.1 }, rnd),
};

// Plucked string (Karplus-Strong). `bright` is the damping: a guitar is darker than a harp.
function pluck(f, dur, { decay = 0.996, bright = 0.5, rel = 0.12 }, rnd) {
  const n = lenOf(dur, rel);
  const out = new Float32Array(n);
  const L = Math.max(2, Math.round(RATE / f));
  const buf = new Float32Array(L);
  for (let i = 0; i < L; i++) buf[i] = rnd() * 2 - 1;
  let prev = 0;
  for (let i = 0; i < L; i++) { buf[i] = prev * (1 - bright) + buf[i] * bright; prev = buf[i]; }
  for (let i = 0; i < n; i++) {
    const j = i % L;
    const nxt = buf[(j + 1) % L];
    buf[j] = decay * (0.5 * buf[j] + 0.5 * nxt);
    const t = i / RATE;
    out[i] = buf[j] * (t < dur ? 1 : Math.max(0, 1 - (t - dur) / rel));
  }
  return out;
}

// FM electric piano: a tine on top of the body, both dying away.
function epiano(f, dur) {
  const rel = 0.25;
  const n = lenOf(dur, rel);
  const out = new Float32Array(n);
  const dec = 1.6 + f / 400;
  for (let i = 0; i < n; i++) {
    const t = i / RATE;
    const g = t < dur ? 1 : Math.max(0, 1 - (t - dur) / rel);
    const body = Math.sin(TAU * f * t + 1.4 * Math.exp(-t * 4) * Math.sin(TAU * f * t));
    const tine = 0.25 * Math.exp(-t * 11) * Math.sin(TAU * f * 7 * t);
    out[i] = (body + tine) * Math.exp(-t * dec) * g * Math.min(1, t / 0.004);
  }
  return out;
}

// A bass that survives a 180 Hz speaker: weak fundamental, strong second to fifth harmonics.
function bassNote(f, dur, kind) {
  const rel = 0.06;
  const n = lenOf(dur, rel);
  const out = new Float32Array(n);
  const harm = kind === "synth" ? [0.6, 1, 0.7, 0.55, 0.4, 0.3, 0.2] : [0.5, 1, 0.55, 0.3, 0.18, 0.1];
  let ph = 0;
  for (let i = 0; i < n; i++) {
    const t = i / RATE;
    const g = t < dur ? 1 : Math.max(0, 1 - (t - dur) / rel);
    ph += (TAU * f) / RATE;
    let s = 0;
    for (let k = 1; k <= harm.length; k++) s += harm[k - 1] * Math.sin(k * ph);
    const e = kind === "synth" ? 0.5 + 0.5 * Math.exp(-t * 7) : 0.3 + 0.7 * Math.exp(-t * 5);
    out[i] = s * e * g * Math.min(1, t / 0.006);
  }
  return out;
}

// ---------------------------------------------------------------- drums
function noiseHit(dur, rnd, { decay, hp = 0, tone = 0, toneHz = 190, toneDecay = 30, gated = false }) {
  const n = Math.ceil(dur * RATE);
  const out = new Float32Array(n);
  let p1 = 0, p2 = 0;
  for (let i = 0; i < n; i++) {
    const t = i / RATE;
    let x = rnd() * 2 - 1;
    for (let h = 0; h < hp; h++) { const y = x - p1; p1 = x; x = y; } // each pass is a one-pole highpass
    void p2;
    const env = gated ? (t < dur * 0.8 ? 1 : Math.max(0, 1 - (t - dur * 0.8) / (dur * 0.2))) : Math.exp(-t * decay);
    out[i] = x * env + tone * Math.sin(TAU * toneHz * t) * Math.exp(-t * toneDecay);
  }
  return out;
}
function kick(level) {
  const n = Math.ceil(0.22 * RATE);
  const out = new Float32Array(n);
  let ph = 0;
  for (let i = 0; i < n; i++) {
    const t = i / RATE;
    ph += (TAU * (80 + 130 * Math.exp(-t * 30))) / RATE;
    out[i] = (Math.sin(ph) * Math.exp(-t * 17) + (i < 60 ? 0.5 * (1 - i / 60) : 0)) * level;
  }
  return out;
}
const DRUM_PATTERNS = {
  // 8 eighth-note steps for 4/4; 6 for 3/4; 4 for 2/4. k kick, s snare or clap, h hat, t tambourine, r rim.
  estrada80: { steps: 8, k: "x...x.x.", s: "..x...x.", h: "x.x.x.x.", gatedSnare: true },
  dance: { steps: 8, k: "x.x.x.x.", s: "..x...x.", h: ".x.x.x.x", gatedSnare: false },
  waltz: { steps: 6, k: "x.....", s: "..x.x.", h: "......", brush: true },
  kolo: { steps: 4, k: "x...", s: "..x.", h: "xxxx", tamb: "x.xx" },
  soft: { steps: 8, k: "x.......", s: "........", h: "x.x.x.x.", quiet: true },
  bolero: { steps: 8, k: "x.....x.", s: "........", h: "x.x.x.x.", rim: "x..x..x." },
};
function drumBar(m, rnd, pat, at, spStep, gain, phrase) {
  const P = DRUM_PATTERNS[pat];
  for (let s = 0; s < P.steps; s++) {
    const o = at + Math.round(s * spStep);
    const a = (c) => c !== undefined && c[s] === "x";
    if (a(P.k)) put(m, o, kick(P.quiet ? 0.5 : 1), 0.9 * gain, 0.05);
    if (a(P.s)) {
      if (P.brush) put(m, o, noiseHit(0.16, rnd, { decay: 14, hp: 1 }), 0.28 * gain, 0.2);
      else put(m, o, noiseHit(P.gatedSnare ? 0.2 : 0.14, rnd, { decay: 20, hp: 1, tone: 0.55, gated: !!P.gatedSnare }), 0.55 * gain, 0.35);
    }
    if (a(P.h)) put(m, o, noiseHit(0.05, rnd, { decay: 85, hp: 2 }), (P.quiet ? 0.18 : 0.26) * gain, 0.1);
    if (a(P.tamb)) put(m, o, noiseHit(0.11, rnd, { decay: 38, hp: 2, tone: 0.1, toneHz: 6200, toneDecay: 60 }), 0.3 * gain, 0.15);
    if (a(P.rim)) put(m, o, noiseHit(0.03, rnd, { decay: 100, hp: 1, tone: 0.7, toneHz: 1750, toneDecay: 120 }), 0.3 * gain, 0.2);
  }
  void phrase;
}

// ---------------------------------------------------------------- reverb (Schroeder)
function reverb(x, amount) {
  const delays = [0.0297, 0.0371, 0.0411, 0.0437].map((d) => Math.round(d * RATE));
  const fb = 0.72 + 0.12 * amount;
  const out = new Float32Array(x.length);
  for (const d of delays) {
    const buf = new Float32Array(d);
    let lp = 0;
    for (let i = 0; i < x.length; i++) {
      const j = i % d;
      lp = lp * 0.55 + buf[j] * 0.45;
      const y = x[i] + lp * fb;
      buf[j] = y;
      out[i] += y * 0.25;
    }
  }
  for (const d0 of [0.005, 0.0017]) {
    const d = Math.round(d0 * RATE);
    const buf = new Float32Array(d);
    for (let i = 0; i < out.length; i++) {
      const j = i % d;
      const b = buf[j];
      const y = -0.5 * out[i] + b;
      buf[j] = out[i] + 0.5 * y;
      out[i] = y;
    }
  }
  return out;
}

// ---------------------------------------------------------------- composition
const scaleMidis = (cfg) => {
  const pcs = SCALES[cfg.scale].map((s) => mod(cfg.root + s, 12));
  const out = [];
  for (let m = cfg.lo; m <= cfg.hi; m++) if (pcs.includes(mod(m, 12))) out.push(m);
  return out;
};
const chordPcs = (cfg, ch) => QUALITY[ch[1]].map((s) => mod(cfg.root + ch[0] + s, 12));
const chordMidis = (cfg, ch, lo, count = 3) => {
  const out = QUALITY[ch[1]].map((s) => {
    const pc = mod(cfg.root + ch[0] + s, 12);
    return lo + mod(pc - lo, 12);
  }).sort((a, b) => a - b);
  return out.slice(0, count);
};

// One phrase of melody: [{at (beats), dur (beats), midi}].
export function melody(cfg, letter) {
  const base = letter.toUpperCase();
  const answer = letter !== base; // lowercase: the answer, closing on the tonic
  const rnd = rngOf(cfg.seed * 31 + base.charCodeAt(0));
  const rnd2 = rngOf(cfg.seed * 17 + letter.charCodeAt(0) * 3);
  const sc = scaleMidis(cfg);
  const prog = cfg.prog[base === "A" || base === "B" ? base : "A"];
  const cellA = cfg.cells[Math.floor(rnd() * cfg.cells.length)];
  const cellB = cfg.cells[Math.floor(rnd() * cfg.cells.length)];
  const end = cfg.ends[Math.floor(rnd() * cfg.ends.length)];
  const rhythm = [cellA, cellA, cellB, end];
  const mid = (cfg.lo + cfg.hi) / 2 + (base === "B" ? 3 : 0);
  let idx = sc.reduce((best, m, i) => (Math.abs(m - mid) < Math.abs(sc[best] - mid) ? i : best), 0);
  let dir = base === "B" ? -1 : 1;
  const notes = [];
  for (let b = 0; b < 4; b++) {
    const r = b >= 2 && answer ? rnd2 : rnd;
    const pcs = chordPcs(cfg, prog[b]);
    let pos = 0;
    for (let q = 0; q < rhythm[b].length; q++) {
      const d = rhythm[b][q];
      const strong = pos === 0 || pos === cfg.beats / 2;
      const last = b === 3 && q === rhythm[b].length - 1;
      if (last) {
        const target = answer ? mod(cfg.root, 12) : mod(cfg.root + prog[3][0] + QUALITY[prog[3][1]][2], 12);
        let best = idx;
        for (let i = 0; i < sc.length; i++) if (mod(sc[i], 12) === target && Math.abs(sc[i] - sc[idx]) < Math.abs(sc[best] - sc[idx]) + (mod(sc[best], 12) === target ? 0 : 99)) best = i;
        idx = best;
      } else if (strong) {
        let best = idx;
        let bd = 99;
        for (let i = Math.max(0, idx - 3); i <= Math.min(sc.length - 1, idx + 3); i++) {
          if (!pcs.includes(mod(sc[i], 12))) continue;
          const dd = Math.abs(i - idx) + (i === idx ? 0.5 : 0);
          if (dd < bd) { bd = dd; best = i; }
        }
        idx = best;
      } else {
        if (r() < 0.3) dir = -dir;
        const step = r() < 0.2 ? 2 : 1;
        idx = Math.max(0, Math.min(sc.length - 1, idx + dir * step));
        if (idx === 0 || idx === sc.length - 1) dir = -dir;
      }
      notes.push({ at: b * cfg.beats + pos, dur: d, midi: sc[idx] });
      pos += d;
    }
  }
  return { notes, prog, sc };
}

export function render(id) {
  const cfg = TRACKS[id];
  if (!cfg) throw new Error(`no track ${id}`);
  const rnd = rngOf(cfg.seed);
  const spb = (60 / cfg.bpm) * RATE;
  const phraseSec = (4 * cfg.beats * 60) / cfg.bpm;
  const phrases = Math.max(2, Math.round(60 / phraseSec));
  const bars = phrases * 4;
  const body = Math.round(bars * cfg.beats * spb);
  const tail = Math.round(2.2 * RATE);
  const m = { dry: new Float32Array(body + tail), send: new Float32Array(body + tail) };
  const lead = VOICES[cfg.lead];
  const spStep = (cfg.beats * spb) / DRUM_PATTERNS[cfg.drums].steps;
  const bassLo = 45;
  for (let p = 0; p < phrases; p++) {
    const letter = cfg.form[p % cfg.form.length];
    const mel = melody(cfg, letter);
    const intro = p === 0;
    const broken = p === cfg.breakPhrase;
    const final = p === phrases - 1;
    const p0 = p * 4 * cfg.beats; // beats
    // melody
    if (!intro) {
      for (const n of mel.notes) {
        const at = Math.round((p0 + n.at) * spb);
        const dur = n.dur * (60 / cfg.bpm) * (n.dur >= 1 ? 0.97 : 0.88);
        put(m, at, lead(mtof(n.midi), dur, rnd), 0.36 * cfg.leadGain, 0.3);
        if (cfg.harmony) {
          const i = mel.sc.indexOf(n.midi);
          const third = mel.sc[Math.max(0, i - 2)];
          if (third !== undefined) put(m, at, lead(mtof(third), dur, rnd), 0.17 * cfg.leadGain, 0.3);
        }
      }
    }
    for (let b = 0; b < 4; b++) {
      const bar = p * 4 + b;
      const ch = mel.prog[b];
      const at = Math.round(bar * cfg.beats * spb);
      const barSec = (cfg.beats * 60) / cfg.bpm;
      const beatSec = 60 / cfg.bpm;
      const rootPc = mod(cfg.root + ch[0], 12);
      const rootMidi = bassLo + mod(rootPc - bassLo, 12);
      const fifth = rootMidi + 7;
      const chord = chordMidis(cfg, ch, 55, 4);
      // pad
      if (cfg.pad !== "none") for (const c of chordMidis(cfg, ch, 52, 3)) put(m, at, VOICES.strings(mtof(c), barSec * 0.98, rnd), 0.12 * cfg.padGain, 0.45);
      // bass
      const bassAt = (beat, midi, d, kind) => put(m, at + Math.round(beat * spb), bassNote(mtof(midi), d * beatSec, kind), 0.4, 0.08);
      if (cfg.bass === "oompah") {
        bassAt(0, rootMidi, 0.6, "pluck");
        if (cfg.beats === 4) bassAt(2, fifth, 0.6, "pluck");
        if (cfg.beats === 2) bassAt(1, fifth, 0.6, "pluck");
      } else if (cfg.bass === "synth") {
        for (let s = 0; s < cfg.beats * 2; s++) bassAt(s / 2, s % 4 === 3 ? fifth : rootMidi, 0.4, "synth");
      } else {
        bassAt(0, rootMidi, 1.4, "pluck");
        bassAt(cfg.beats / 2, fifth, 1.2, "pluck");
        if (cfg.beats === 4) bassAt(3.5, rootMidi + 12, 0.45, "pluck");
      }
      // comping
      const c4 = (i) => chord[i % chord.length] + 12 * Math.floor(i / chord.length);
      if (cfg.comp === "oompahAccordion") {
        const beats = cfg.beats === 3 ? [1, 2] : cfg.beats === 2 ? [1] : [1, 3];
        for (const bt of beats) for (const c of chord.slice(0, 3)) put(m, at + Math.round(bt * spb), VOICES.accordionChord(mtof(c + 12), 0.14, rnd), 0.12, 0.25);
        if (cfg.beats === 2) for (const c of chord.slice(0, 3)) put(m, at + Math.round(1.5 * spb), VOICES.accordionChord(mtof(c + 12), 0.1, rnd), 0.08, 0.25);
      } else if (cfg.comp === "epiano") {
        for (let s = 0; s < cfg.beats * 2; s++) put(m, at + Math.round((s / 2) * spb), epiano(mtof(c4([0, 1, 2, 1][s % 4]) + 12), beatSec * 0.5), 0.2, 0.4);
      } else if (cfg.comp === "synthArp") {
        for (let s = 0; s < cfg.beats * 4; s++) put(m, at + Math.round((s / 4) * spb), VOICES.synth(mtof(c4([0, 1, 2, 3, 2, 1][s % 6]) + 12), beatSec * 0.2, rnd), 0.11, 0.2);
      } else if (cfg.comp === "harp") {
        for (let s = 0; s < cfg.beats * 2; s++) put(m, at + Math.round((s / 2) * spb), pluck(mtof(c4([0, 1, 2, 3, 2, 1][s % 6]) + 12), beatSec * 1.6, { decay: 0.9975, bright: 0.85 }, rnd), 0.2, 0.5);
      } else if (cfg.comp === "strum") {
        for (const [bt, dn] of [[0.5, 1], [1.5, 1], [2, -1], [2.5, 1], [3.5, -1]]) {
          const order = dn > 0 ? chord : [...chord].reverse();
          order.forEach((c, i) => put(m, at + Math.round(bt * spb) + i * 160, pluck(mtof(c), beatSec * 0.6, { decay: 0.995, bright: 0.45 }, rnd), 0.2, 0.3));
        }
      }
      // drums
      if (!broken && !(intro && b < 0)) drumBar(m, rnd, cfg.drums, at, spStep, final ? 0.8 : 1, p);
    }
  }
  // reverb, then fold the tail onto the start so the loop is circular
  const wet = reverb(m.send, cfg.reverb);
  const x = new Float64Array(body);
  for (let i = 0; i < body + tail; i++) x[i % body] += m.dry[i] + wet[i] * cfg.reverb * 0.9;
  // a small radio speaker: 180 Hz high-pass, 5.2 kHz low-pass (24 dB), run over two laps so the loop stays seamless
  const y = new Float64Array(body * 2);
  y.set(x);
  y.set(x, body);
  biquad(y, "hp", 180, 0.71);
  biquad(y, "lp", 5200, 0.71);
  biquad(y, "lp", 5200, 0.71);
  let peak = 0;
  for (let i = body; i < y.length; i++) peak = Math.max(peak, Math.abs(y[i]));
  const out = new Int16Array(body);
  const sq = (v) => Math.tanh(1.1 * v) / Math.tanh(1.1); // squeezed dynamics
  let top = 0;
  const sqd = new Float64Array(body);
  for (let i = 0; i < body; i++) { sqd[i] = sq(y[body + i] / peak); top = Math.max(top, Math.abs(sqd[i])); }
  const scale = (32767 * 10 ** (-3 / 20)) / top;
  for (let i = 0; i < body; i++) out[i] = Math.round(sqd[i] * scale);
  return out;
}

function biquad(x, type, fc, q) {
  const w = (TAU * fc) / RATE;
  const al = Math.sin(w) / (2 * q);
  const c = Math.cos(w);
  let b0, b1, b2;
  if (type === "hp") { b0 = (1 + c) / 2; b1 = -(1 + c); b2 = (1 + c) / 2; } else { b0 = (1 - c) / 2; b1 = 1 - c; b2 = (1 - c) / 2; }
  const a0 = 1 + al, a1 = -2 * c, a2 = 1 - al;
  let x1 = 0, x2 = 0, y1 = 0, y2 = 0;
  for (let i = 0; i < x.length; i++) {
    const v = (b0 * x[i] + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2) / a0;
    x2 = x1; x1 = x[i]; y2 = y1; y1 = v;
    x[i] = v;
  }
}

// ---------------------------------------------------------------- command line
function main(root, flags) {
  const value = (name) => { const i = flags.indexOf(name); return i >= 0 ? flags[i + 1] : undefined; };
  const only = value("--id");
  const out = resolve(value("--out") || join(root, "tmp", "assets", "atmosphere", "wav"));
  mkdirSync(out, { recursive: true });
  const specFile = join(root, "tools", "assets", "atmosphere", "ambience.json");
  const rec = JSON.parse(readFileSync(specFile, "utf8"));
  const today = new Date().toISOString().slice(0, 10);
  for (const id of Object.keys(TRACKS)) {
    if (only && only !== id) continue;
    const t0 = Date.now();
    const x = render(id);
    const wav = encodeWav(x, RATE);
    writeFileSync(join(out, `${id}_01.wav`), wav);
    const info = { ...wavInfo(wav), wrap: wrapRatio(x) };
    console.log(`${id}: ${info.seconds.toFixed(2)} s, peak ${info.peakDb.toFixed(1)} dB, rms ${info.rmsDb.toFixed(1)} dB, loop step ${info.wrap.toFixed(2)}x, ${((Date.now() - t0) / 1000).toFixed(1)} s to render`);
    if (flags.includes("--record")) {
      const s = rec.sounds.find((x2) => x2.id === id);
      if (!s) throw new Error(`ambience.json has no sound ${id}`);
      const gens = (rec.generations ||= []).filter((g) => g.id !== id || g.take !== 1);
      rec.generations = gens;
      rec.generations.push({
        id, take: 1, date: today, tool: "Procedural synth (tools/assets/atmosphere/radio-synth.mjs)", model: "none", prompt: s.prompt,
        seconds: Math.round(info.seconds * 100) / 100, bytes: wav.length, credits: 0,
        processed: { seconds: Math.round(info.seconds * 100) / 100, peak_db: Math.round(info.peakDb * 10) / 10, rms_db: Math.round(info.rmsDb * 10) / 10, wrap: Math.round(info.wrap * 100) / 100 },
      });
    }
  }
  if (flags.includes("--record")) writeFileSync(specFile, JSON.stringify(rec, null, 2) + "\n");
  return 0;
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  process.exit(main(resolve(dirname(fileURLToPath(import.meta.url)), "..", "..", ".."), process.argv.slice(2)));
}

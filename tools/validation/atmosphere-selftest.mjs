// The town's background sound, offline (docs/architecture/atmosphere.md): the spec is sound, every
// wired project is what the tool would write today, every sample is a mono 16-bit loop whose ends
// meet, and every emitter in a prefab or layer starts inside its project's audible range.
// red-proof: 2026-11-13, with LF_PoliceDutyDesk.et's m_fRadius set to 40 (outer range 14) this printed
// "Prefabs/Police/LF_PoliceDutyDesk.et: radius 40 m is outside Atmosphere_PoliceDesk.acp's 14 m range".
import { existsSync, readFileSync, readdirSync, statSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { ambientAcpText, loopProblems, projectPath, samplePath, specFile, specProblems, wrapRatio } from "../assets/pipeline/ambience.mjs";
import { wavInfo } from "../assets/pipeline/sound.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const ADDON = join(ROOT, "addons", "LifeFramework");
const fails = [];
const check = (ok, msg) => { if (!ok) fails.push(msg); };

// Every LF_AmbientEmitterComponent block in a prefab or layer text: its project and radius.
export function emittersIn(text) {
  const out = [];
  for (const m of text.matchAll(/LF_AmbientEmitterComponent "\{[0-9A-F]{16}\}" \{([\s\S]*?)\n\s*\}/g)) {
    const project = /m_sSoundProject "\{[0-9A-F]{16}\}([^"]+)"/.exec(m[1]);
    const radius = /m_fRadius ([\d.]+)/.exec(m[1]);
    if (project) out.push({ project: project[1], radius: radius ? Number(radius[1]) : 20 });
  }
  return out;
}

export function radiusProblems(file, text, rec) {
  const out = [];
  for (const e of emittersIn(text)) {
    const s = rec.sounds.find((x) => projectPath(x) === e.project);
    if (!s) continue; // a vanilla or hand-made project: its range is not ours to know
    const name = e.project.split("/").pop();
    if (e.radius >= s.outer_m) out.push(`${file}: radius ${e.radius} m is outside ${name}'s ${s.outer_m} m range`);
    else if (e.radius < s.outer_m * 0.5) out.push(`${file}: radius ${e.radius} m is under half of ${name}'s ${s.outer_m} m range; the sound would start loud`);
  }
  return out;
}

function walk(dir, out = []) {
  for (const name of readdirSync(dir)) {
    const p = join(dir, name);
    if (statSync(p).isDirectory()) walk(p, out);
    else if (/\.(et|layer)$/.test(name)) out.push(p);
  }
  return out;
}

// The checks themselves, on made-up data.
const fake = { budget: { credits: 100 }, sounds: [{ id: "a", name: "A", kind: "loop", prompt: "x", seconds: 10, rate: 48000, volume_db: -10, inner_m: 2, outer_m: 14, seam_ms: 400, takes: 1 }] };
check(specProblems(fake).length === 0, "a good spec was refused");
check(specProblems({ ...fake, sounds: [{ ...fake.sounds[0], outer_m: 1 }] }).length === 1, "outer_m under inner_m passed");
check(specProblems({ ...fake, budget: { credits: 50000 } }).length === 1, "a budget over the cap passed");
const block = (r) => `  LF_AmbientEmitterComponent "{0123456789ABCDEF}" {\n   m_sSoundProject "{0123456789ABCDEF}Sounds/Atmosphere/Atmosphere_A.acp"\n   m_fRadius ${r}\n  }`;
check(radiusProblems("x.et", block(13), fake).length === 0, "a radius just inside the range was refused");
check(radiusProblems("x.et", block(40), fake).length === 1, "a radius outside the range passed");
check(radiusProblems("x.et", block(3), fake).length === 1, "a radius far inside the range passed");
const smooth = new Int16Array(4800).map((_, i) => Math.round(8000 * Math.sin((i / 4800) * 2 * Math.PI * 10)));
const click = smooth.slice(0, 4680); // cut a quarter period from the end: the wrap jumps from the crest to zero
check(wrapRatio(smooth) < 1.5, `a whole number of periods reads as a click (${wrapRatio(smooth).toFixed(2)})`);
check(wrapRatio(click) > 1.5, `a loop cut mid-wave passes (${wrapRatio(click).toFixed(2)})`);
check(loopProblems({ tag: 1, bits: 16, channels: 2, rate: 48000 }, fake.sounds[0]).length === 1, "a stereo sample passed");

// The real files.
const rec = JSON.parse(readFileSync(specFile(ROOT), "utf8"));
for (const p of specProblems(rec)) fails.push(`ambience.json: ${p}`);
let wired = 0;
for (const s of rec.sounds) {
  const w = rec.wired?.[s.id];
  if (!w) continue;
  wired++;
  const acp = join(ADDON, projectPath(s));
  const wav = join(ADDON, samplePath(s));
  if (!existsSync(acp) || !existsSync(wav)) { fails.push(`${s.id}: wired, and ${existsSync(acp) ? samplePath(s) : projectPath(s)} is missing`); continue; }
  check(readFileSync(acp, "utf8") === ambientAcpText(s, w.sample), `${projectPath(s)} differs from what the spec gives; run: tools\\cli asset ambience --only wire`);
  const bytes = readFileSync(wav);
  const info = { ...wavInfo(bytes), wrap: wrapRatio(new Int16Array(bytes.buffer, bytes.byteOffset + 44, (bytes.length - 44) >> 1)) };
  for (const p of loopProblems(info, s)) fails.push(`${samplePath(s)}: ${p}`);
}
let emitters = 0;
for (const f of walk(join(ADDON, "Prefabs")).concat(walk(join(ADDON, "Worlds")))) {
  const text = readFileSync(f, "utf8");
  if (!text.includes("LF_AmbientEmitterComponent")) continue;
  emitters += emittersIn(text).length;
  fails.push(...radiusProblems(f.slice(ADDON.length + 1).split("\\").join("/"), text, rec));
}

if (fails.length) {
  console.log("atmosphere-selftest: FAIL");
  for (const f of fails) console.log("  " + f);
  process.exit(1);
}
console.log(`atmosphere-selftest: OK (${wired} wired sound(s) match the spec, ${emitters} emitter(s) start inside their range)`);

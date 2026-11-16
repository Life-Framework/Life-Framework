// The six period radio tracks (#1967) are composed by tools/assets/atmosphere/radio-synth.mjs. This proves
// ambience.json carries a music entry, a dated record and a credit row for each, and that every committed
// sample is what the script renders today (within 2 steps of a 16-bit sample, so a different Node build's
// maths cannot fail it).
// inputs: tools/assets/atmosphere/**, tools/assets/pipeline/**, addons/LF_Everon1989/Sounds/Atmosphere/**, docs/credits.md
// red-proof: 2026-11-16, each restored after its FAIL was seen:
//   seed of radio_accordion_kolo changed in radio-synth.mjs -> FAIL committed ... differs (observed)
//   the Radio-prefixed credit row for the kolo deleted from docs/credits.md -> FAIL credits (observed)
import { readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { TRACKS, RATE, render } from "../assets/atmosphere/radio-synth.mjs";
import { samplePath } from "../assets/pipeline/ambience.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const PACK = join(ROOT, "addons", "LF_Everon1989");
let failed = 0;
const ok = (cond, name) => {
  if (cond) return;
  console.log(`FAIL ${name}`);
  failed++;
};

// The comparison itself, on made-up data: a sample off by 3 steps is a different render.
export const closeTo = (a, b, tol = 2) => a.length === b.length && a.every((v, i) => Math.abs(v - b[i]) <= tol);
ok(closeTo(Int16Array.from([1, 2, 3]), Int16Array.from([2, 2, 4])), "closeTo: one step off was refused");
ok(!closeTo(Int16Array.from([1, 2, 3]), Int16Array.from([1, 2, 6])), "closeTo: three steps off passed");

const rec = JSON.parse(readFileSync(join(ROOT, "tools", "assets", "atmosphere", "ambience.json"), "utf8"));
const credits = readFileSync(join(ROOT, "docs", "credits.md"), "utf8");
const ids = Object.keys(TRACKS);
ok(ids.length === 6, `six tracks are composed, found ${ids.length}`);
for (const id of ids) {
  const s = rec.sounds.find((x) => x.id === id);
  ok(!!s, `${id}: ambience.json has no sound with this id`);
  if (!s) continue;
  ok(s.kind === "music" && s.radio === true && s.synth === "tools/assets/atmosphere/radio-synth.mjs", `${id}: must be a radio music entry with a synth script`);
  ok(s.rate === RATE, `${id}: rate ${s.rate}, the synth writes ${RATE}`);
  const g = (rec.generations || []).find((x) => x.id === id);
  ok(!!g && /^\d{4}-\d{2}-\d{2}$/.test(g.date) && /Procedural/.test(g.tool) && g.credits === 0, `${id}: needs a dated generation row from the procedural synth`);
  const file = samplePath(s);
  ok(credits.includes(file.split("/").pop()), `credits: docs/credits.md has no row for ${file}`);
  let committed;
  try {
    const bytes = readFileSync(join(PACK, file));
    committed = new Int16Array(bytes.buffer.slice(bytes.byteOffset + 44, bytes.byteOffset + bytes.length));
  } catch {
    ok(false, `${id}: ${file} is not in LF_Everon1989`);
    continue;
  }
  ok(closeTo(committed, render(id)), `committed: ${file} differs from what radio-synth.mjs renders (rerun: node tools/assets/atmosphere/radio-synth.mjs, then tools\\cli asset ambience --only wire)`);
}

if (failed) {
  console.log(`radio-tracks-selftest: FAILED (${failed})`);
  process.exit(1);
}
console.log(`radio-tracks-selftest: OK (${ids.length} tracks match their spec, credit rows and committed samples)`);

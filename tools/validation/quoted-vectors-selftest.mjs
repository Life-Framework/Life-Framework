// tools/validation/quoted-vectors-selftest.mjs - validate-quoted-vectors.
// inputs: tools/validation/validate-quoted-vectors.mjs, tools/lib/quoted-vectors.mjs, tools/validation/quoted-vectors-selftest.mjs
//
// Runs the validator over a throwaway tree:
//   - a layer with bare vectors passes;
//   - `m_vHalfExtents "5 3 7"` in a layer fails with file:line (the #3665 weighbridge bug);
//   - a quoted `coords`, `Offset` or `m_vOffset` in a layer or prefab fails;
//   - a conf `Pos "x y z"` string passes, a conf `m_vHalfExtents "5 3 7"` fails;
//   - a generator that writes a quoted vector fails; a clean generator, a comment and an
//     `lf-quoted-vector-ok` line pass.
//
// red-proof: 2026-11-19 with quotedVectorLines returning [], "a quoted m_vHalfExtents in a layer fails" printed FAIL.
import { spawnSync } from "node:child_process";
import { mkdirSync, rmSync, writeFileSync } from "node:fs";
import { scratchDir } from "../lib/scratch.mjs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { quotedVectorEmitters, quotedVectorLines } from "../lib/quoted-vectors.mjs";

let failed = 0;
const check = (ok, msg) => {
  console.log(`${ok ? "PASS" : "FAIL"}  ${msg}`);
  if (!ok) failed++;
};

const validator = join(dirname(fileURLToPath(import.meta.url)), "validate-quoted-vectors.mjs");
const dir = scratchDir("lf-quoted-vectors-");
const layers = join(dir, "addons", "LF_X", "Worlds", "W", "W_Layers");
const prefabs = join(dir, "addons", "LF_X", "Prefabs");
const confs = join(dir, "addons", "LF_X", "Configs");
const gens = join(dir, "tools", "town");
for (const d of [layers, prefabs, confs, gens]) mkdirSync(d, { recursive: true });
const run = () => spawnSync(process.execPath, [validator], { env: { ...process.env, LF_QVEC_ROOT: dir }, encoding: "utf8" });
const first = (r) => r.stdout.trim().split("\n")[0];

const GOOD = 'GenericEntity Row : "{AB}Prefabs/x.et" {\n coords 1 2 3\n angles 0 90 0\n components {\n  LF_Act "{01}" {\n   m_vHalfExtents 5 3 7\n   Offset 0 1 0\n  }\n }\n}\n';

try {
  check(quotedVectorLines('  m_vHalfExtents "5 3 7"\n').length === 1, "a quoted m_vHalfExtents is found");
  check(quotedVectorLines('  m_vHalfExtents "5 3 7"\n')[0].line === 1, "the 1-based line is reported");
  check(quotedVectorLines('  m_vHalfExtents 5 3 7\n  m_fRadius "2"\n  ID "{01}"\n  m_sName "a b c"\n').length === 0, "bare vectors, one quoted scalar and a string name are not flagged");
  check(quotedVectorLines('  coords "1.5 -2 3e2"\n').length === 1, "a decimal, negative and exponent triple is found");
  check(quotedVectorLines('  Pos "4592.3 0 10818.2"\n').length === 0, "an unknown name (Pos) is not flagged in the loose mode");
  check(quotedVectorLines('  coords "1 2 3"\n', { strict: true }).length === 0, "strict mode only names m_v members");
  check(quotedVectorEmitters('  `m_vHalfExtents "${x} ${y} ${z}"`,\n').length === 1, "a template that writes a quoted m_v vector is found");
  check(quotedVectorEmitters('  l.push(`   coords ${a} ${b} ${c}`);\n').length === 0, "an unquoted generator line is not flagged");

  writeFileSync(join(layers, "Place_ok.layer"), GOOD);
  writeFileSync(join(confs, "pos.conf"), 'Row {\n Pos "4592.3 0 10818.2"\n Centre "1 2 3"\n}\n');
  writeFileSync(join(gens, "fit.mjs"), 'const l = [`   coords ${a} ${b} ${c}`, `   m_vHalfExtents ${w} ${h} ${d}`];\n// m_vHalfExtents "5 3 7" is the bug\n');
  let r = run();
  check(r.status === 0, `a clean tree passes (${first(r)})`);

  writeFileSync(join(layers, "Place_x.layer"), 'GenericEntity Row : "{AB}Prefabs/x.et" {\n components {\n  LF_Act "{01}" {\n   m_eMode 0\n   m_vHalfExtents "5 3 7"\n  }\n }\n}\n');
  r = run();
  check(r.status === 1 && /addons\/LF_X\/Worlds\/W\/W_Layers\/Place_x\.layer:5 /.test(r.stdout), `a quoted m_vHalfExtents in a layer fails (${first(r)})`);
  rmSync(join(layers, "Place_x.layer"));

  writeFileSync(join(prefabs, "p.et"), 'GenericEntity : "{AB}Prefabs/x.et" {\n coords "1 2 3"\n}\n');
  r = run();
  check(r.status === 1 && /Prefabs\/p\.et:2 /.test(r.stdout), `a quoted coords in a prefab fails (${first(r)})`);
  rmSync(join(prefabs, "p.et"));

  writeFileSync(join(confs, "bad.conf"), 'Row {\n m_vHalfExtents "5 3 7"\n}\n');
  r = run();
  check(r.status === 1 && /Configs\/bad\.conf:2 /.test(r.stdout), `a quoted m_v member in a conf fails (${first(r)})`);
  rmSync(join(confs, "bad.conf"));

  writeFileSync(join(gens, "drops.mjs"), 'const l = [`   m_vHalfExtents "${w} ${h} ${d}"`];\n');
  r = run();
  check(r.status === 1 && /tools\/town\/drops\.mjs:1 /.test(r.stdout), `a generator that writes a quoted vector fails (${first(r)})`);
  writeFileSync(join(gens, "drops.mjs"), 'const l = [`   m_vHalfExtents "${w} ${h} ${d}"`]; // lf-quoted-vector-ok: a string field\n');
  r = run();
  check(r.status === 0, `an lf-quoted-vector-ok line passes (${first(r)})`);
  rmSync(join(gens, "drops.mjs"));

  r = run();
  check(r.status === 0, "passes again once the planted lines are gone");
} finally {
  rmSync(dir, { recursive: true, force: true });
}

if (failed) {
  console.log(`quoted-vectors-selftest: ${failed} FAIL`);
  process.exit(1);
}
console.log("quoted-vectors-selftest: OK");

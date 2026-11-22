// Every placed sign names a sign id.
// inputs: addons/**, tools/lib/sign-ids.mjs
//
// A sign prefab whose mesh samples `$rendertarget` (every LF_Sign_* plate, fascia, post sign and pole plate) draws
// solid black until its face is printed, and the face is printed from the sign id. A placed instance with an
// empty id in its whole prefab chain was a black slab that read nothing: the bakery and butcher fascias on the
// island and the taxi stand plate (2026-11-22; "some signs have no titled entity"). The runtime now prints a blank
// painted board for it and logs the error, but the id is still required.
//
// Fails, with file:line: a `.layer` entity (or a `$grp` member) of a prefab whose chain carries LF_SignComponent,
// where neither the instance nor any prefab above it sets a non-empty `m_sSignId`, and `m_bShowFace` is not 0.
// The id's own title row is `validate-dialogue-refs` (sign: a placed id no def has).
//
// LF_SIGNIDS_ROOT points the scan at another tree (the selftest).
//
// red-proof: 2026-11-22, tools/validation/sign-ids-selftest.mjs plants a fascia with no id: `FAIL ...:1 ... has
// no m_sSignId`, exit 1; the real tree held 3 (bakery fascia, butcher fascia, taxi stand) before this change.
import { execFileSync } from "node:child_process";
import { existsSync, readdirSync, readFileSync, statSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { signsWithoutId } from "../lib/sign-ids.mjs";

const root = process.env.LF_SIGNIDS_ROOT || join(dirname(fileURLToPath(import.meta.url)), "..", "..");

function walk(dir, rel, out) {
  for (const name of readdirSync(dir)) {
    if (name === ".git" || name === "node_modules" || name === "log") continue;
    const p = join(dir, name);
    const r = rel ? `${rel}/${name}` : name;
    if (statSync(p).isDirectory()) walk(p, r, out);
    else out.push(r);
  }
}

function listFiles() {
  if (!existsSync(join(root, ".git"))) {
    const out = [];
    if (existsSync(join(root, "addons"))) walk(join(root, "addons"), "addons", out);
    return out;
  }
  const raw = execFileSync("git", ["ls-files", "-co", "--exclude-standard", "--", "addons"], { cwd: root, encoding: "utf8", maxBuffer: 1 << 28 });
  return raw.split("\n").filter(Boolean);
}

const files = new Map();
for (const f of listFiles()) {
  if (/\.(et|layer|meta)$/.test(f)) files.set(f, readFileSync(join(root, f), "utf8"));
}

const bad = signsWithoutId(files);
if (bad.length) {
  for (const b of bad) console.log(`FAIL  ${b.path}:${b.line} ${b.name} (${b.prefab}) has no m_sSignId: its face never prints`);
  console.log(`SUMMARY validate-sign-ids fail ${bad.length}`);
  process.exit(1);
}
console.log("PASS  validate-sign-ids: every placed sign names a sign id");

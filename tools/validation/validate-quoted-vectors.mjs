// No quoted vector in a layer, prefab or config, and no generator that writes one.
// inputs: addons/**, tools/**
//
// `m_vHalfExtents "5 3 7"` is a string where the engine wants `m_vHalfExtents 5 3 7`. The load logs
// `WORLD (E): Unexpected data at offset` and drops the entity at world origin: the Saint-Philippe
// weighbridge prompt vanished and no player could unload grain (#3665 proof, #3671).
//
// Scope, tracked files only:
//   - addons/** and tools/** `.layer` and `.et`: a vector property (m_v*, coords, angles, Offset)
//     holding a quoted list of numbers fails;
//   - `.conf`: only an `m_v*` member (a conf may keep a `Pos "x y z"` string a script parses);
//   - tools/**/*.mjs generators (not selftests or this check's lib): a source line that writes one of
//     those properties with a quoted number list or `${x} ${y}` template fails, so the kit sources,
//     town fit, drops, nightlights, parked cars and homes stay clean. `// lf-quoted-vector-ok: <why>`
//     on the line allows a real one.
//
// LF_QVEC_ROOT points the scan at another tree (the selftest; no .git there, so a directory walk
// lists the files).
//
// red-proof: 2026-11-19, tools/validation/quoted-vectors-selftest.mjs plants a layer with
// `m_vHalfExtents "5 3 7"` and a generator line that writes one and runs this over it:
// `FAIL addons/.../Place_x.layer:5`, exit 1; the real tree held 4 such lines (2 in the Everon pack
// wheatfield layer, 2 in its kit source) before they were unquoted.
import { execFileSync } from "node:child_process";
import { existsSync, readdirSync, readFileSync, statSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { quotedVectorEmitters, quotedVectorLines } from "../lib/quoted-vectors.mjs";

const root = process.env.LF_QVEC_ROOT || join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const SCOPES = ["addons", "tools"];

function walk(dir, rel, out) {
  for (const name of readdirSync(dir)) {
    if (name === ".git" || name === "node_modules") continue;
    const p = join(dir, name);
    const r = rel ? `${rel}/${name}` : name;
    if (statSync(p).isDirectory()) walk(p, r, out);
    else out.push(r);
  }
}

function listFiles() {
  if (existsSync(join(root, ".git"))) {
    const out = execFileSync("git", ["-C", root, "ls-files", "-z", "--", ...SCOPES], { encoding: "utf8", maxBuffer: 1 << 28 });
    return out.split("\0").filter(Boolean);
  }
  const files = [];
  for (const s of SCOPES) if (existsSync(join(root, s))) walk(join(root, s), s, files);
  return files;
}

// A generator source: a tools script that is not a selftest and not the helper that holds the pattern.
const isGenerator = (f) => f.startsWith("tools/") && f.endsWith(".mjs") && !/selftest\.mjs$/.test(f) && !f.endsWith("lib/quoted-vectors.mjs") && !f.endsWith("validate-quoted-vectors.mjs");

const bad = [];
let scanned = 0;
for (const f of listFiles()) {
  const resource = /\.(layer|et)$/.test(f);
  const conf = f.endsWith(".conf");
  const gen = isGenerator(f);
  if (!resource && !conf && !gen) continue;
  let text;
  try {
    text = readFileSync(join(root, f), "utf8");
  } catch {
    continue; // tracked but deleted in this checkout
  }
  scanned++;
  if (resource || conf) {
    for (const h of quotedVectorLines(text, { strict: conf })) bad.push(`FAIL ${f}:${h.line} ${h.name} is a quoted vector: ${h.text}`);
  } else {
    for (const h of quotedVectorEmitters(text)) bad.push(`FAIL ${f}:${h.line} generator writes a quoted ${h.name}: ${h.text}`);
  }
}

if (bad.length) {
  console.log(bad.join("\n"));
  console.log(`validate-quoted-vectors: ${bad.length} quoted vector(s). Write the numbers bare (m_vHalfExtents 5 3 7); a quoted list is a string and the engine drops the entity at world origin.`);
  process.exit(1);
}
console.log(`validate-quoted-vectors: OK (${scanned} file(s))`);

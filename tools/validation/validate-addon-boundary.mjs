// tools/validation/validate-addon-boundary.mjs
// inputs: addons/**, docs/architecture/**
//
// The core (addons/LifeFramework) never references the flagship pack
// (addons/LF_Everon1989); the pack depends on the core, not the other way.
// docs/architecture/core-vs-pack.md has the rule. This check fails when:
//   - a core text file names a pack resource GUID, the pack project GUID, or
//     the pack's addon ID;
//   - a core text file names a pack resource path that has no core file at the
//     same path;
//   - a core script names a class declared in pack script;
//   - the pack holds a .c file that is not on PACK_SCRIPT_ALLOWLIST and is not a pack test.
// Pack tests are the one kind of pack script (docs/architecture/three-worlds.md "Pack test leg"):
// Scripts/Game/Tests/LF_Test_*.c declaring only LF_Test_* classes that extend LF_Test.
// A missing pack folder passes: the core must boot alone.
// red-proof: tools/validation/addon-boundary-selftest.mjs plants each violation
// in a fixture tree and asserts this check reports it.

import { existsSync, readdirSync, readFileSync, statSync } from "node:fs";
import { dirname, join, relative, sep } from "node:path";
import { fileURLToPath } from "node:url";

export const CORE = "LifeFramework";
export const PACK = "LF_Everon1989";

// Pack script that is allowed (paths relative to the pack root, forward
// slashes). Keep it short: script in the pack is a smell; the fix is an
// extension point in the core. Each entry needs a decision entry.
export const PACK_SCRIPT_ALLOWLIST = new Set([]);

const PACK_TEST_FILE = /^Scripts\/Game\/Tests\/LF_Test_\w+\.c$/;
const CLASS_DECL = /^\s*((?:modded\s+|sealed\s+)*)class\s+([A-Za-z_]\w*)(\s*:\s*([A-Za-z_]\w*))?/gm;

// Why a pack script on a pack test path is not a pack test, "" when it is one, null when the path is
// not a pack test path (the allowlist rule decides).
export function packTestProblem(r, text) {
  const decls = [...text.matchAll(CLASS_DECL)].map((m) => ({ modded: /modded/.test(m[1]), name: m[2], base: m[4] }));
  if (!PACK_TEST_FILE.test(r)) return null;
  const bad = decls.filter((d) => d.modded || !/^LF_Test_\w+$/.test(d.name) || d.base !== "LF_Test");
  if (bad.length) return `a pack test file declares ${bad.map((d) => (d.modded ? "modded " : "") + d.name).join(", ")}; it holds only LF_Test_* classes that extend LF_Test`;
  if (!decls.length) return "a pack test file declares no LF_Test_* class";
  return "";
}

// Text formats that can carry a {GUID}path reference.
const TEXT_EXT = new Set([
  ".c", ".et", ".conf", ".layer", ".ent", ".layout", ".st", ".gproj", ".meta",
  ".emat", ".imageset", ".ptc", ".acp", ".sig", ".agr", ".agf", ".ast", ".asi",
  ".aw", ".styles", ".txa", ".terr",
]);
const SKIP_DIRS = new Set(["log", "WorkbenchGame", ".git"]);
const GUID_REF = /\{([0-9A-Fa-f]{16})\}/g;

function ext(name) {
  const i = name.lastIndexOf(".");
  return i < 0 ? "" : name.slice(i).toLowerCase();
}

function walk(dir, out = []) {
  if (!existsSync(dir)) return out;
  for (const name of readdirSync(dir)) {
    if (SKIP_DIRS.has(name)) continue;
    const path = join(dir, name);
    if (statSync(path).isDirectory()) walk(path, out);
    else out.push(path);
  }
  return out;
}

const rel = (base, path) => relative(base, path).split(sep).join("/");

// Finding every pack path in every core line was an indexOf per pack path per line (23 of this check's 30 s).
// A reference starts at the line start or after "}" ({GUID}path), a quote, or whitespace, so Prefabs/X.et does
// not match inside Prefabs/Props/X.et; those are the only places a path can start, and each is looked up by its
// first KEY characters. A path shorter than KEY is matched with startsWith.
const KEY = 20;
const REF_START = /["}\s]/g;
export function indexPaths(paths) {
  const byKey = new Map();
  const short = [];
  for (const p of paths) {
    if (p.length < KEY) {
      short.push(p);
      continue;
    }
    const k = p.slice(0, KEY);
    const list = byKey.get(k);
    if (list) list.push(p);
    else byKey.set(k, [p]);
  }
  return { byKey, short };
}

export function pathRefsIn(line, index) {
  const found = [];
  if (!index.byKey.size && !index.short.length) return found;
  const at = (i) => {
    for (const p of index.short) if (line.startsWith(p, i)) found.push(p);
    const list = index.byKey.get(line.substr(i, KEY));
    if (list) for (const p of list) if (line.startsWith(p, i)) found.push(p);
  };
  at(0);
  REF_START.lastIndex = 0;
  for (let m = REF_START.exec(line); m; m = REF_START.exec(line)) at(m.index + 1);
  return found;
}

// Everything the pack owns that the core must not name.
export function packSurface(packDir) {
  const guids = new Map(); // GUID -> pack file that owns it
  const paths = new Set(); // resource paths relative to the pack root
  const classes = new Map(); // class name -> pack script
  const scripts = [];
  for (const file of walk(packDir)) {
    const r = rel(packDir, file);
    const e = ext(file);
    if (e === ".meta") {
      const m = readFileSync(file, "utf8").match(/Name\s+"\{([0-9A-Fa-f]{16})\}/);
      if (m) guids.set(m[1].toUpperCase(), r);
      continue;
    }
    if (e === ".gproj") {
      const m = readFileSync(file, "utf8").match(/GUID\s+"?([0-9A-Fa-f]{16})"?/);
      if (m) guids.set(m[1].toUpperCase(), r);
      continue;
    }
    // Docs, folder keepers, and Workbench's generated files are not resources.
    if (r.endsWith(".gitkeep") || e === ".md" || /\.(rdb|lock|user)$/.test(r)) continue;
    paths.add(r);
    if (e === ".c") {
      scripts.push(r);
      // A modded class is the core's own class, not one the pack owns.
      for (const m of readFileSync(file, "utf8").matchAll(/^\s*(modded\s+)?(?:sealed\s+)*class\s+([A-Za-z_]\w*)/gm)) {
        if (!m[1]) classes.set(m[2], r);
      }
    }
  }
  return { guids, paths, classes, scripts };
}

// Returns a list of "file:line message" violations; empty means the boundary holds.
export function listPacks(addonsDir) {
  if (!existsSync(addonsDir)) return [];
  return readdirSync(addonsDir).filter((n) => n.startsWith("LF_") && statSync(join(addonsDir, n)).isDirectory()).sort();
}

// Every pack under addons/ (LF_*) is held to the boundary, not only the flagship.
export function checkBoundary(addonsDir) {
  return listPacks(addonsDir).flatMap((p) => checkPackBoundary(addonsDir, p));
}

function checkPackBoundary(addonsDir, PACK) {
  const coreDir = join(addonsDir, CORE);
  const packDir = join(addonsDir, PACK);
  const errors = [];
  if (!existsSync(packDir)) return errors;

  const pack = packSurface(packDir);
  for (const s of pack.scripts) {
    const testProblem = packTestProblem(s, readFileSync(join(packDir, ...s.split("/")), "utf8"));
    if (testProblem) errors.push(`${PACK}/${s}: ${testProblem}`);
    if (testProblem !== null) continue;
    if (!PACK_SCRIPT_ALLOWLIST.has(s)) {
      errors.push(`${PACK}/${s}: script in the pack; add a core extension point (config field, attribute, prefab slot) instead, or allowlist it with a decision entry`);
    }
  }

  const corePaths = new Set(walk(coreDir).map((f) => rel(coreDir, f)));
  const packOnlyPaths = new Set([...pack.paths].filter((p) => !corePaths.has(p)));
  const packPathIndex = indexPaths(packOnlyPaths);
  // Whole word: a pack LF_Test is not named by the core class LF_TestOrgSign.
  const packNameRe = new RegExp(`\\b${PACK}\\b`);
  const classNames = [...pack.classes.keys()];
  const classRe = classNames.length ? new RegExp(`\\b(${classNames.join("|")})\\b`) : null;

  for (const file of walk(coreDir)) {
    if (!TEXT_EXT.has(ext(file))) continue;
    const r = `${CORE}/${rel(coreDir, file)}`;
    const lines = readFileSync(file, "utf8").split(/\r?\n/);
    const isMeta = ext(file) === ".meta";
    lines.forEach((line, i) => {
      const at = `${r}:${i + 1}`;
      if (packNameRe.test(line)) errors.push(`${at} names the pack addon "${PACK}"`);
      for (const m of line.matchAll(GUID_REF)) {
        const owner = pack.guids.get(m[1].toUpperCase());
        if (owner) errors.push(`${at} references pack GUID {${m[1]}} (${PACK}/${owner})`);
      }
      if (ext(file) === ".gproj") {
        for (const m of line.matchAll(/"([0-9A-Fa-f]{16})"/g)) {
          const owner = pack.guids.get(m[1].toUpperCase());
          if (owner) errors.push(`${at} depends on the pack project ${m[1]} (${PACK}/${owner})`);
        }
      }
      if (!isMeta) {
        for (const p of pathRefsIn(line, packPathIndex)) errors.push(`${at} references pack path ${p}`);
      }
      if (classRe && ext(file) === ".c") {
        const m = line.match(classRe);
        if (m) errors.push(`${at} uses pack class ${m[1]} (${PACK}/${pack.classes.get(m[1])})`);
      }
    });
  }
  return errors;
}

if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1]) {
  const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
  const errors = checkBoundary(join(root, "addons"));
  if (errors.length) {
    for (const e of errors) console.log(`FAIL ${e}`);
    console.log(`validate-addon-boundary: ${errors.length} violation(s); the core must not reference ${PACK} (docs/architecture/core-vs-pack.md)`);
    process.exit(1);
  }
  const packs = listPacks(join(root, "addons"));
  const state = packs.length ? `${packs.length} pack(s): ${packs.join(", ")}` : "no pack folder";
  console.log(`validate-addon-boundary: OK (${state}; core references none of them)`);
}

// tools/validation/validate-addon-boundary.mjs
//
// The core (addons/LifeFramework) never references the flagship pack
// (addons/LF_Everon1989); the pack depends on the core, not the other way.
// docs/architecture/core-vs-pack.md has the rule. This check fails when:
//   - a core text file names a pack resource GUID, the pack project GUID, or
//     the pack's addon ID;
//   - a core text file names a pack resource path that has no core file at the
//     same path;
//   - a core script names a class declared in pack script;
//   - the pack holds a .c file that is not on PACK_SCRIPT_ALLOWLIST.
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
      for (const m of readFileSync(file, "utf8").matchAll(/^\s*(?:modded\s+|sealed\s+)*class\s+([A-Za-z_]\w*)/gm)) {
        classes.set(m[1], r);
      }
    }
  }
  return { guids, paths, classes, scripts };
}

// Returns a list of "file:line message" violations; empty means the boundary holds.
export function checkBoundary(addonsDir) {
  const coreDir = join(addonsDir, CORE);
  const packDir = join(addonsDir, PACK);
  const errors = [];
  if (!existsSync(packDir)) return errors;

  const pack = packSurface(packDir);
  for (const s of pack.scripts) {
    if (!PACK_SCRIPT_ALLOWLIST.has(s)) {
      errors.push(`${PACK}/${s}: script in the pack; add a core extension point (config field, attribute, prefab slot) instead, or allowlist it with a decision entry`);
    }
  }

  const corePaths = new Set(walk(coreDir).map((f) => rel(coreDir, f)));
  const packOnlyPaths = new Set([...pack.paths].filter((p) => !corePaths.has(p)));
  const classNames = [...pack.classes.keys()];
  const classRe = classNames.length ? new RegExp(`\\b(${classNames.join("|")})\\b`) : null;

  for (const file of walk(coreDir)) {
    if (!TEXT_EXT.has(ext(file))) continue;
    const r = `${CORE}/${rel(coreDir, file)}`;
    const lines = readFileSync(file, "utf8").split(/\r?\n/);
    const isMeta = ext(file) === ".meta";
    lines.forEach((line, i) => {
      const at = `${r}:${i + 1}`;
      if (line.includes(PACK)) errors.push(`${at} names the pack addon "${PACK}"`);
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
        for (const p of packOnlyPaths) {
          const i = line.indexOf(p);
          // A reference starts after "}" ({GUID}path), a quote, or whitespace,
          // so Prefabs/X.et does not match inside Prefabs/Props/X.et.
          if (i >= 0 && (i === 0 || /["}\s]/.test(line[i - 1]))) errors.push(`${at} references pack path ${p}`);
        }
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
  const packDir = join(root, "addons", PACK);
  const state = existsSync(packDir) ? `${packSurface(packDir).paths.size} pack resource(s)` : "no pack folder";
  console.log(`validate-addon-boundary: OK (${state}; core references none of them)`);
}

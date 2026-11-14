// tools/validation/validate-pack-manifest.mjs
//
// The pack manifest and the public surface (#963, docs/architecture/public-surface.md).
// Fails when:
//   - the pack has no manifest .conf (root class LF_PackManifest), or its range is not
//     x.y.z, is empty (min >= below), or excludes the core version in LF_PackManifest.c;
//   - a pack .conf has a core LF_ class as its root that public-surface.json does not list;
//   - a listed class is not declared in core script;
//   - a name in public-surface-baseline.json is neither listed nor deprecated (a removed
//     public name; retire it through `deprecated` first).
// A missing pack folder passes: the core must boot alone.
// red-proof: tools/validation/pack-manifest-selftest.mjs plants each violation.

import { existsSync, readdirSync, readFileSync, statSync } from "node:fs";
import { dirname, join, relative, sep } from "node:path";
import { fileURLToPath } from "node:url";

const SEMVER = /^\d+\.\d+\.\d+$/;
const cmp = (a, b) => {
  const x = a.split(".").map(Number), y = b.split(".").map(Number);
  for (let i = 0; i < 3; i++) if (x[i] !== y[i]) return x[i] < y[i] ? -1 : 1;
  return 0;
};

function walk(dir, out = []) {
  if (!existsSync(dir)) return out;
  for (const name of readdirSync(dir)) {
    if (name === "log" || name === ".git") continue;
    const p = join(dir, name);
    if (statSync(p).isDirectory()) walk(p, out);
    else out.push(p);
  }
  return out;
}

const field = (text, name) => text.match(new RegExp(`^\\s*${name}\\s+"([^"]*)"`, "m"))?.[1];
const rootClass = (text) => text.match(/^\s*([A-Za-z_]\w*)\s*(?::\s*"[^"]*"\s*)?\{/m)?.[1];

// Returns violation strings; empty means the manifest and surface hold.
export function checkPackManifest(root) {
  const errors = [];
  const addons = join(root, "addons");
  const core = join(addons, "LifeFramework");
  const pack = join(addons, "LF_Everon1989");
  if (!existsSync(pack)) return errors;

  const surfacePath = join(root, "docs", "architecture", "public-surface.json");
  const basePath = join(root, "docs", "architecture", "public-surface-baseline.json");
  const surface = JSON.parse(readFileSync(surfacePath, "utf8"));
  const base = existsSync(basePath) ? JSON.parse(readFileSync(basePath, "utf8")) : { configClasses: [] };
  const listed = new Set(surface.configClasses ?? []);
  const deprecated = new Set((surface.deprecated ?? []).map((d) => d.name));

  // Core script: the running version and the declared classes.
  const declared = new Set();
  let coreVersion = null;
  for (const f of walk(join(core, "Scripts")).filter((p) => p.endsWith(".c"))) {
    const text = readFileSync(f, "utf8");
    for (const m of text.matchAll(/^\s*(?:modded\s+|sealed\s+)*class\s+([A-Za-z_]\w*)/gm)) declared.add(m[1]);
    const v = text.match(/CORE_VERSION\s*=\s*"([^"]*)"/);
    if (v) coreVersion = v[1];
  }
  if (!coreVersion || !SEMVER.test(coreVersion)) errors.push("LF_PackManifest.CORE_VERSION is missing or not x.y.z");

  for (const name of listed) {
    if (!declared.has(name)) errors.push(`public-surface.json lists ${name}, which core script does not declare`);
  }
  for (const name of base.configClasses ?? []) {
    if (!listed.has(name) && !deprecated.has(name)) {
      errors.push(`public name ${name} was removed; move it to "deprecated" in public-surface.json for one minor version first`);
    }
  }

  const manifests = [];
  for (const f of walk(pack).filter((p) => p.endsWith(".conf"))) {
    const text = readFileSync(f, "utf8");
    const r = relative(pack, f).split(sep).join("/");
    const cls = rootClass(text);
    if (!cls) continue;
    if (cls === "LF_PackManifest") manifests.push({ r, text });
    if (cls.startsWith("LF_") && declared.has(cls) && !listed.has(cls)) {
      errors.push(`LF_Everon1989/${r} uses ${cls}, which is not in the core's public surface (docs/architecture/public-surface.md)`);
    }
  }

  if (manifests.length !== 1) {
    errors.push(`LF_Everon1989 needs exactly one LF_PackManifest .conf, found ${manifests.length}`);
  } else {
    const { r, text } = manifests[0];
    const min = field(text, "m_sCoreMin") ?? "";
    const below = field(text, "m_sCoreBelow") ?? "";
    const id = field(text, "m_sPackId") ?? "";
    if (id !== "LF_Everon1989") errors.push(`LF_Everon1989/${r}: m_sPackId is "${id}", expected "LF_Everon1989"`);
    for (const [n, v] of [["m_sCoreMin", min], ["m_sCoreBelow", below]]) {
      if (v && !SEMVER.test(v)) errors.push(`LF_Everon1989/${r}: ${n} "${v}" is not x.y.z`);
    }
    if (SEMVER.test(min) && SEMVER.test(below) && cmp(min, below) >= 0) errors.push(`LF_Everon1989/${r}: the range [${min}, ${below}) is empty`);
    if (coreVersion && SEMVER.test(coreVersion)) {
      const out = (SEMVER.test(min) && cmp(coreVersion, min) < 0) || (SEMVER.test(below) && cmp(coreVersion, below) >= 0);
      if (out) errors.push(`LF_Everon1989/${r}: the range [${min}, ${below}) excludes the running core ${coreVersion}`);
    }
  }
  return errors;
}

if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1]) {
  const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
  const errors = checkPackManifest(root);
  if (errors.length) {
    for (const e of errors) console.log(`FAIL ${e}`);
    console.log(`validate-pack-manifest: ${errors.length} violation(s) (docs/architecture/public-surface.md)`);
    process.exit(1);
  }
  console.log("validate-pack-manifest: OK (manifest range holds the running core; pack uses only the public surface)");
}

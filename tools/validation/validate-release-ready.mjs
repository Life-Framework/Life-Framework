// tools/validation/validate-release-ready.mjs
// inputs: addons/**, docs/marketing/**
//
// A Workshop upload fails or ships junk when addon metadata is missing or the
// tree holds generated files (#368). For each addon (the core LifeFramework and
// the pack LF_Everon1989) this check fails on:
//   - a .gproj without ID, GUID or TITLE, or an ID that is not the folder name;
//   - a .gproj that does not depend on the base game (58D0FB3206B6F859);
//   - a pack .gproj that does not depend on the core's GUID;
//   - a generated file in the tree (*.rdb, *.lock, *.gproj.user, the addon
//     `log` folder), tracked or untracked-not-ignored;
//   - an EL_ identifier in a text file (AGENTS.md rule 5; resource basenames
//     that keep EL_ paths are exempt, as in validate-no-el-prefix.mjs).
// The Workshop kit (version, summary, description, preview image, APL licence
// line) lives under docs/marketing/workshop/ for the core and
// docs/marketing/workshop/everon-1989/ for the pack. A kit folder that exists
// must be complete: a missing field fails and is named. A pack kit folder that
// does not exist yet is a WARN until #375 lands; --strict turns WARN into FAIL.
// The licence follows decision D2: the Workshop build is APL.
// red-proof: tools/validation/release-ready-selftest.mjs builds a fixture repo
// missing each field in turn and asserts each is reported by name.

import { existsSync, readFileSync, readdirSync, statSync } from "node:fs";
import { execFileSync } from "node:child_process";
import { basename, dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

export const CORE = "LifeFramework";
export const PACK = "LF_Everon1989";
export const BASE_GAME_GUID = "58D0FB3206B6F859";
const KIT_ROOT = "docs/marketing/workshop";
const KIT_DIR = { [CORE]: KIT_ROOT, [PACK]: `${KIT_ROOT}/everon-1989` };

const SKIP_DIRS = new Set(["WorkbenchGame", ".git", "node_modules"]);
const BINARY = /\.(png|jpe?g|tif|edds|xob|bin|ico|wav|ogg|mp4|rdb|blob|dds|gif|pdf|lock|fbx|ttf|otf|webp)$/i;
const GENERATED = /(^|\/)([^/]*\.rdb|[^/]*\.lock|[^/]*\.gproj\.user|log)(\/|$)/i;

function walk(dir, base = dir, out = []) {
  if (!existsSync(dir)) return out;
  for (const name of readdirSync(dir)) {
    const p = join(dir, name);
    const rel = p.slice(base.length + 1).split("\\").join("/");
    if (SKIP_DIRS.has(name)) continue;
    if (statSync(p).isDirectory()) walk(p, base, out);
    else out.push(rel);
  }
  return out;
}

// Files to inspect for one addon: git's view when the root is a repo (tracked
// plus untracked-not-ignored), else a plain walk (fixture trees).
function addonFiles(root, addon) {
  const dir = join(root, "addons", addon);
  if (existsSync(join(root, ".git"))) {
    try {
      const out = execFileSync("git", ["ls-files", "-z", "--cached", "--others", "--exclude-standard", `addons/${addon}`], {
        cwd: root, encoding: "utf8", maxBuffer: 1 << 28,
      });
      const prefix = `addons/${addon}/`;
      return out.split("\0").filter(Boolean).map((f) => f.slice(prefix.length)).filter((f) => existsSync(join(dir, f)));
    } catch { /* fall through to the walk */ }
  }
  return walk(dir);
}

function gprojField(text, key) {
  const m = text.match(new RegExp(`^\\s*${key}\\s+"([^"]*)"`, "m"));
  return m ? m[1].trim() : "";
}

function gprojDeps(text) {
  const m = text.match(/Dependencies\s*\{([^}]*)\}/);
  return m ? [...m[1].matchAll(/"([0-9A-Fa-f]{16})"/g)].map((x) => x[1].toUpperCase()) : [];
}

function checkGproj(root, addon, errors, coreGuid) {
  const path = join(root, "addons", addon, `${addon}.gproj`);
  const at = `${addon}/${addon}.gproj`;
  if (!existsSync(path)) {
    errors.push(`${at}: missing; an addon needs its own .gproj`);
    return "";
  }
  const text = readFileSync(path, "utf8");
  const id = gprojField(text, "ID");
  const guid = gprojField(text, "GUID");
  if (!id) errors.push(`${at}: missing ID`);
  else if (id !== addon) errors.push(`${at}: ID "${id}" is not the folder name "${addon}"`);
  if (!/^[0-9A-Fa-f]{16}$/.test(guid)) errors.push(`${at}: missing or malformed GUID`);
  if (!gprojField(text, "TITLE")) errors.push(`${at}: missing TITLE`);
  const deps = gprojDeps(text);
  if (!deps.includes(BASE_GAME_GUID)) errors.push(`${at}: Dependencies lacks the base game ${BASE_GAME_GUID}`);
  if (addon === PACK) {
    if (!coreGuid) errors.push(`${at}: cannot check the core dependency, the core GUID is unknown`);
    else if (!deps.includes(coreGuid.toUpperCase())) errors.push(`${at}: Dependencies lacks the core ${CORE} GUID ${coreGuid}`);
  }
  return guid;
}

function checkTree(root, addon, errors) {
  const files = addonFiles(root, addon);
  const elBases = new Set();
  for (const f of [CORE, PACK].flatMap((a) => addonFiles(root, a))) {
    const b = basename(f).replace(/\.meta$/i, "");
    if (b.startsWith("EL_") && !b.endsWith(".c")) elBases.add(b.replace(/\.[^.]+$/, ""));
  }
  for (const f of files) {
    const at = `${addon}/${f}`;
    if (GENERATED.test(f)) {
      errors.push(`${at}: generated file in the tree (never committed or published)`);
      continue;
    }
    if (BINARY.test(f)) continue;
    const buf = readFileSync(join(root, "addons", addon, f));
    if (buf.includes(0)) continue;
    if (/(^|\/)EL_[^/]*\.c$/.test(f)) errors.push(`${at}: script file keeps the EL_ prefix`);
    buf.toString("utf8").split("\n").forEach((line, i) => {
      for (const m of line.matchAll(/(?<![A-Z0-9])EL_\w*/g)) {
        if (elBases.has(m[0]) && !/\b(class|enum)\s+EL_/.test(line)) continue;
        errors.push(`${at}:${i + 1}: EL_ identifier ${m[0]}`);
      }
    });
  }
}

function checkKit(root, addon, errors, warnings) {
  const rel = KIT_DIR[addon];
  const dir = join(root, rel);
  // The pack's folder sits inside the core's, so it counts as present only
  // when it holds its own README.md.
  if (!existsSync(dir) || (addon === PACK && !existsSync(join(dir, "README.md")))) {
    warnings.push(`${addon}: no Workshop kit at ${rel}/ yet (version, summary, description, preview image, APL line); required once #375 lands`);
    return;
  }
  const need = (file, label, minBytes) => {
    const p = join(dir, file);
    if (!existsSync(p) || statSync(p).size < minBytes) errors.push(`${addon}: Workshop kit lacks ${label} (${rel}/${file})`);
  };
  need("summary.txt", "the summary", 1);
  need("description.bbcode", "the description", 1);
  need("preview.jpg", "the preview image", 1024);
  const readmePath = join(dir, "README.md");
  if (!existsSync(readmePath)) {
    errors.push(`${addon}: Workshop kit lacks README.md (${rel}/README.md), which holds the Version and Licence rows`);
    return;
  }
  const readme = readFileSync(readmePath, "utf8");
  if (!/^\|\s*Version\s*\|\s*`?\d+\.\d+\.\d+`?\s*\|/m.test(readme)) errors.push(`${addon}: Workshop kit README has no semver Version row`);
  const lic = readme.match(/^\|\s*Licen[cs]e\s*\|\s*(.*?)\s*\|/m);
  if (!lic) errors.push(`${addon}: Workshop kit README has no Licence row (APL notice)`);
  else if (!/\bAPL\b|Arma Public License/.test(lic[1]) || /\bMIT\b/.test(lic[1])) {
    errors.push(`${addon}: Workshop kit Licence row "${lic[1]}" is not APL (decision D2)`);
  }
}

// Returns { errors, warnings } for the addons under <root>/addons.
export function checkRelease(root) {
  const errors = [];
  const warnings = [];
  if (!existsSync(join(root, "addons", CORE))) {
    errors.push(`${CORE}: addon folder missing`);
    return { errors, warnings };
  }
  const coreGuid = checkGproj(root, CORE, errors, "");
  checkTree(root, CORE, errors);
  checkKit(root, CORE, errors, warnings);
  if (existsSync(join(root, "addons", PACK))) {
    checkGproj(root, PACK, errors, coreGuid);
    checkTree(root, PACK, errors);
    checkKit(root, PACK, errors, warnings);
  }
  return { errors, warnings };
}

if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1]) {
  const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
  const strict = process.argv.includes("--strict");
  const { errors, warnings } = checkRelease(root);
  const fails = strict ? errors.concat(warnings) : errors;
  if (!strict) for (const w of warnings) console.log(`WARN ${w}`);
  if (fails.length) {
    for (const e of fails) console.log(`FAIL ${e}`);
    console.log(`validate-release-ready: ${fails.length} problem(s); fix before a Workshop upload (#368)`);
    process.exit(1);
  }
  console.log(`validate-release-ready: OK (${warnings.length} warning(s))`);
}

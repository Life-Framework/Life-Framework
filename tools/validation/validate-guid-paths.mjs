// tools/validation/validate-guid-paths.mjs
// inputs: always
// (always: it compares its baseline with origin/main's, so repo files are not its only input)
//
// A resource path in Enforce Script carries its GUID: "{D6DE32D1C0FCC1C7}Prefabs/Editor/Camera/ManualCameraBase.et"
// (AGENTS.md golden rule 3). A bare "Prefabs/...et" resolves to {0000000000000000}, the engine logs
// `Wrong GUID/name for resource` and the client crashed on it (#506, CLEAN_CAMERA_PREFAB).
//
// In .c files under addons/*/Scripts the check fails on a string literal that is a whole resource path:
// it starts with Prefabs/, Configs/, UI/, Worlds/, Assets/, Sounds/, Language/, Missions/, Materials/ ...,
// ends in a resource extension (.et .conf .layout .emat .xob .edds .ent .st .acp ...) and has no
// 16-hex {GUID} in front (a {0000000000000000} or a short/long hex prefix counts as none).
// Not flagged: comments; log text and format strings (a literal with a space or %); path builders
// ("Prefabs/" + name, "{GUID}Prefabs/%1.et"); file-system paths ($profile:, $logs:, $saves:) since
// they do not start with a resource folder; a placeholder prefix such as {SHORT}.
//
// Exempt by folder: Scripts/Game/Tests/ (fixtures that test path helpers and save records; the
// strings are never loaded as resources).
//
// tools/validation/guid-paths-baseline.json lists today's findings, each with the issue that
// removes it. It may only shrink: a finding that is not in it fails, an entry with no finding
// fails (delete it), an entry needs an "issue" ("#123"), and an entry that the main branch copy
// does not have fails (when git can read that copy). --write-baseline refuses to add.
// red-proof: guid-paths-selftest.mjs.

import { execFileSync } from "node:child_process";
import { existsSync, readFileSync, readdirSync, writeFileSync } from "node:fs";
import { dirname, join, relative } from "node:path";
import { fileURLToPath } from "node:url";
import { stripComments } from "./validate-map-independence.mjs";

export const BASELINE = "tools/validation/guid-paths-baseline.json";
export const EXEMPT_DIRS = ["Scripts/Game/Tests"];
const FOLDERS = "Prefabs|Configs|UI|Worlds|Assets|Sounds|Sound|Language|Missions|Materials|Entities|Characters|Items|Props|Textures|Terrain|Animations|Particles|Common|Scenarios";
const EXTS = "et|conf|layout|emat|xob|edds|ent|st|acp|aw|asi|agr|ast|anm|ptc|imageset|fnt|gamemat|physmat|bin|wav|layer|ct";
const PATH_RE = new RegExp(String.raw`^(\{[^}]*\})?((?:${FOLDERS})/[^\s%"{}]*\.(?:${EXTS}))$`, "i");

function* scriptFiles(dir, base) {
  for (const e of readdirSync(dir, { withFileTypes: true })) {
    const full = join(dir, e.name);
    const rel = relative(base, full).replaceAll("\\", "/");
    if (e.isDirectory()) {
      if (e.name === "node_modules" || EXEMPT_DIRS.some((x) => rel.endsWith(x))) continue;
      yield* scriptFiles(full, base);
    } else if (e.name.endsWith(".c")) {
      yield [rel, full];
    }
  }
}

// The literals in one file's text that are a resource path without a good GUID.
export function scanText(raw) {
  const text = stripComments(raw);
  const out = [];
  const seen = new Set();
  for (const m of text.matchAll(/"((?:[^"\\\n]|\\.)*)"/g)) {
    const p = PATH_RE.exec(m[1]);
    if (!p) continue;
    const guid = p[1];
    let bad = !guid;
    if (guid && /^\{[0-9A-Fa-f]*\}$/.test(guid)) bad = !/^\{[0-9A-Fa-f]{16}\}$/.test(guid) || /^\{0{16}\}$/.test(guid);
    if (bad && !seen.has(m[1])) {
      seen.add(m[1]);
      out.push({ rule: "no-guid", token: m[1] });
    }
  }
  return out;
}

// [{ file, rule, token }]; file is "<addon>/Scripts/..." relative to addons/.
export function scanAddons(root) {
  const base = join(root, "addons");
  const out = [];
  if (!existsSync(base)) return out;
  for (const a of readdirSync(base, { withFileTypes: true })) {
    const scripts = join(base, a.name, "Scripts");
    if (!a.isDirectory() || !existsSync(scripts)) continue;
    for (const [rel, full] of scriptFiles(scripts, base)) {
      for (const h of scanText(readFileSync(full, "utf8"))) out.push({ file: rel, ...h });
    }
  }
  return out;
}

export function loadBaseline(root) {
  const p = join(root, BASELINE);
  return existsSync(p) ? JSON.parse(readFileSync(p, "utf8")) : { entries: [] };
}

const key = (e) => `${e.file}|${e.rule}|${e.token}`;
const WHY = "a resource path with no {16 hex GUID} resolves to {0000000000000000} and can crash the client (#506); find the GUID with `tools/cli call asset_search`, a .meta or a layer that references it or the .meta and write \"{GUID}path\" (AGENTS.md rule 3)";

// previous: the entry keys the main branch has, or null when unknown.
export function checkGuidPaths(root, baseline = loadBaseline(root), previous = null) {
  const now = scanAddons(root);
  const entries = baseline.entries || [];
  const errors = [];
  const stale = [];
  const allowed = new Set(entries.map(key));
  const seen = new Set(now.map(key));
  for (const f of now) {
    if (!allowed.has(key(f))) errors.push(`addons/${f.file}: \`${f.token}\`: ${WHY}`);
  }
  for (const e of entries) {
    if (!/^#\d+$/.test(e.issue || "")) errors.push(`baseline entry ${key(e)} has no issue ("#123") naming who removes it`);
    if (!seen.has(key(e))) stale.push(`baseline entry ${key(e)} is gone from the code; delete the entry (the baseline only shrinks)`);
    if (previous && !previous.has(key(e))) errors.push(`baseline entry ${key(e)} is new; the baseline may only shrink`);
  }
  return { errors: [...errors, ...stale], stale, now };
}

function previousKeys(root) {
  try {
    const text = execFileSync("git", ["show", `origin/main:${BASELINE}`], { cwd: root, stdio: ["ignore", "pipe", "ignore"] }).toString();
    return new Set((JSON.parse(text).entries || []).map(key));
  } catch {
    return null;
  }
}

if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1]) {
  const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
  if (process.argv.includes("--write-baseline")) {
    const old = loadBaseline(root);
    const have = new Set((old.entries || []).map(key));
    const now = scanAddons(root);
    const added = now.filter((f) => !have.has(key(f)));
    if (added.length) {
      console.log(`FAIL refusing to add ${added.length} finding(s) to the baseline:\n  ${added.map(key).join("\n  ")}`);
      process.exit(1);
    }
    const seen = new Set(now.map(key));
    writeFileSync(join(root, BASELINE), JSON.stringify({ entries: old.entries.filter((e) => seen.has(key(e))) }, null, 1) + "\n");
    console.log("validate-guid-paths: baseline shrunk");
    process.exit(0);
  }
  const { errors, now } = checkGuidPaths(root, loadBaseline(root), previousKeys(root));
  if (errors.length) {
    for (const e of errors) console.log(`FAIL ${e}`);
    console.log(`validate-guid-paths: ${errors.length} problem(s)`);
    process.exit(1);
  }
  console.log(`validate-guid-paths: OK (${now.length} baselined finding(s) left, see ${BASELINE})`);
}

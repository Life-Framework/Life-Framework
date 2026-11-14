// tools/validation/validate-map-independence.mjs
// inputs: always
// (always: it compares its baseline with origin/main's, so repo files are not its only input)
//
// Core Scripts must not know where anything stands on Everon. The map moves to the
// LF_Everon1989 pack (docs/architecture/core-vs-pack.md "Map independence"; AGENTS.md
// rule 9). In .c files under addons/LifeFramework/Scripts/Game this check fails on:
//   find-by-name  FindEntityByName(...) with any argument: a name only one map has.
//   world-coords  a position literal, "x y z" or Vector(x, y, z), where x and z are both
//                 100 or more in magnitude. Offsets ("0 1.2 0", "8 0 0") are small.
//   place-name    a Roadmap_* or Sign_SafeZone_* name in code (comments are ignored).
// Use LF_Places, an attribute the layer sets, or a component found through its registry.
//
// Exempt by folder (dev tooling, never in a player's play path, decided on #1046):
//   Tests/                    the suite, which has its own annex fixtures
//   Feature/AgentHarness/     the agent harness, placement audit and spike harnesses:
//                             they drive and audit whatever world is loaded and a
//                             dedicated server never needs them.
//
// tools/validation/map-independence-baseline.json lists today's findings, each with the
// issue that removes it. It may only shrink: a finding that is not in it fails, an entry
// with no finding fails (delete it), an entry needs an "issue" ("#123"), and an entry
// that the main branch copy does not have fails (when git can read that copy).
// --write-baseline refuses to add. red-proof: map-independence-selftest.mjs.

import { execFileSync } from "node:child_process";
import { existsSync, readFileSync, readdirSync, writeFileSync } from "node:fs";
import { dirname, join, relative } from "node:path";
import { fileURLToPath } from "node:url";

export const SCRIPTS = "addons/LifeFramework/Scripts/Game";
export const BASELINE = "tools/validation/map-independence-baseline.json";
export const EXEMPT_DIRS = ["Tests", "Feature/AgentHarness"];
export const COORD_MIN = 100;

function* coreFiles(base, dir) {
  for (const e of readdirSync(dir, { withFileTypes: true })) {
    const full = join(dir, e.name);
    const rel = relative(base, full).replaceAll("\\", "/");
    if (e.isDirectory()) {
      if (EXEMPT_DIRS.includes(rel)) continue;
      yield* coreFiles(base, full);
    } else if (e.name.endsWith(".c")) {
      yield [rel, full];
    }
  }
}

// Source with comments blanked (strings kept, line breaks kept).
export function stripComments(text) {
  let out = "";
  for (let i = 0; i < text.length; i++) {
    const c = text[i];
    if (c === '"') {
      let j = i + 1;
      while (j < text.length && text[j] !== '"' && text[j] !== "\n") j += text[j] === "\\" ? 2 : 1;
      out += text.slice(i, j + 1);
      i = j;
    } else if (c === "/" && text[i + 1] === "/") {
      while (i < text.length && text[i] !== "\n") i++;
      out += "\n";
    } else if (c === "/" && text[i + 1] === "*") {
      const end = text.indexOf("*/", i + 2);
      const stop = end < 0 ? text.length : end + 2;
      out += text.slice(i, stop).replace(/[^\n]/g, "");
      i = stop - 1;
    } else {
      out += c;
    }
  }
  return out;
}

const num = String.raw`-?\d+(?:\.\d+)?f?`;
const far = (x, z) => Math.abs(parseFloat(x)) >= COORD_MIN && Math.abs(parseFloat(z)) >= COORD_MIN;

// [{ rule, token }] for one file's text.
export function scanText(raw) {
  const text = stripComments(raw);
  const hits = [];
  for (const m of text.matchAll(/FindEntityByName\s*\(\s*([^;]*?)\s*\)\s*;/g)) hits.push({ rule: "find-by-name", token: m[1].replace(/\s+/g, " ") });
  for (const m of text.matchAll(new RegExp(`"\\s*(${num})\\s+(${num})\\s+(${num})\\s*"`, "g"))) {
    if (far(m[1], m[3])) hits.push({ rule: "world-coords", token: `${m[1]} ${m[2]} ${m[3]}` });
  }
  for (const m of text.matchAll(new RegExp(`Vector\\(\\s*(${num})\\s*,\\s*(${num})\\s*,\\s*(${num})\\s*\\)`, "g"))) {
    if (far(m[1], m[3])) hits.push({ rule: "world-coords", token: `${m[1]} ${m[2]} ${m[3]}` });
  }
  for (const m of text.matchAll(/\b(Roadmap_\w*|Sign_SafeZone_\w*)/g)) hits.push({ rule: "place-name", token: m[1] });
  const seen = new Set();
  return hits.filter((h) => !seen.has(h.rule + "|" + h.token) && seen.add(h.rule + "|" + h.token));
}

// [{ file, rule, token }] for every finding under Scripts/Game.
export function scanCore(root) {
  const base = join(root, SCRIPTS);
  const out = [];
  if (!existsSync(base)) return out;
  for (const [rel, full] of coreFiles(base, base)) {
    for (const h of scanText(readFileSync(full, "utf8"))) out.push({ file: rel, ...h });
  }
  return out;
}

export function loadBaseline(root) {
  const p = join(root, BASELINE);
  return existsSync(p) ? JSON.parse(readFileSync(p, "utf8")) : { entries: [] };
}

const key = (e) => `${e.file}|${e.rule}|${e.token}`;
const WHY = {
  "find-by-name": "FindEntityByName names an entity only one map has; find a placed component through its registry or LF_Places",
  "world-coords": "a world position literal only fits one map; read it from a placed entity or an attribute the layer sets",
  "place-name": "an Everon place name; the layer sets the id on an attribute and the script default stays empty",
};

// previous: the entry keys the main branch has, or null when unknown.
export function checkMapIndependence(root, baseline = loadBaseline(root), previous = null) {
  const now = scanCore(root);
  const entries = baseline.entries || [];
  const errors = [];
  const stale = [];
  const allowed = new Set(entries.map(key));
  const seen = new Set(now.map(key));
  for (const f of now) {
    if (!allowed.has(key(f))) errors.push(`${SCRIPTS}/${f.file}: ${f.rule} \`${f.token}\`: ${WHY[f.rule]} (docs/architecture/core-vs-pack.md "Map independence")`);
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
    const now = scanCore(root);
    const added = now.filter((f) => !have.has(key(f)));
    if (added.length) {
      console.log(`FAIL refusing to add ${added.length} finding(s) to the baseline:\n  ${added.map(key).join("\n  ")}`);
      process.exit(1);
    }
    const seen = new Set(now.map(key));
    writeFileSync(join(root, BASELINE), JSON.stringify({ entries: old.entries.filter((e) => seen.has(key(e))) }, null, 1) + "\n");
    console.log("validate-map-independence: baseline shrunk");
    process.exit(0);
  }
  const { errors, now } = checkMapIndependence(root, loadBaseline(root), previousKeys(root));
  if (errors.length) {
    for (const e of errors) console.log(`FAIL ${e}`);
    console.log(`validate-map-independence: ${errors.length} problem(s)`);
    process.exit(1);
  }
  console.log(`validate-map-independence: OK (${now.length} baselined finding(s) left, see ${BASELINE})`);
}

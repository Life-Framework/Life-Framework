#!/usr/bin/env node
// One-shot, reviewable rename of every EL_ identifier to LF_ (issue #334).
//
//   node tools/rename-el-to-lf.mjs            dry run: map, counts, collisions
//   node tools/rename-el-to-lf.mjs --apply    rewrite files and rename EL_*.c
//   node tools/rename-el-to-lf.mjs --map-out tmp/map.json   write the old->new map
//
// How it works:
//   1. Class map. Every `class|enum|typedef ... EL_X` declaration in a .c file
//      gives EL_X -> LF_X. Those are the renamed classes.
//   2. Every other EL_ identifier token in a tracked text file (method, RPC,
//      constant, script define, input context, config key) gets the same prefix
//      swap, so a name stays one name across .c / .et / .layer / .layout /
//      .conf / tools / docs.
//   3. Resource names stay. A tracked non-script file named EL_*.et|layout|conf|
//      imageset keeps its path (renaming a resource breaks every {GUID}path in
//      a save). A token that is such a file's basename is left alone, except
//      that a class declared under the same name (a script entity class and its
//      prefab share a name) is renamed everywhere but inside the resource path,
//      i.e. when the token is directly followed by the file extension.
//   4. Also swapped: the log prefixes ELDebug -> LFDebug and ELTEST -> LFTEST
//      (and eldebug-lines.txt -> lfdebug-lines.txt).
//   5. Collisions are printed: an LF_X the rename would create that already
//      exists as a token or as a script file.
//
// Skipped files (history, upstream credit, generated): see KEEP_FILES.
// The persistence alias map is not produced here; see docs/persistence.md
// "Renamed types".

import { execFileSync } from "node:child_process";
import { existsSync, readFileSync, writeFileSync } from "node:fs";
import { basename, dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..");
const args = process.argv.slice(2);
const APPLY = args.includes("--apply");
const mapOutIdx = args.indexOf("--map-out");
const MAP_OUT = mapOutIdx >= 0 ? args[mapOutIdx + 1] : "";

// Files whose EL_ names are history or upstream credit, or are generated.
const KEEP_FILES = new Set([
  "docs/decisions.md",
  "docs/everonlife-forks.md",
  "THIRD_PARTY.md",
  "website/content/github-snapshot.json",
  "tools/rename-el-to-lf.mjs",
]);
const BINARY_EXT = /\.(png|jpg|jpeg|tif|tiff|edds|xob|bin|ico|woff2?|ttf|otf|wav|ogg|mp4|zip|pak|rdb|blob|dds|gif|webp|pdf|lock|npy)$/i;
const SKIP_PREFIX = ["website/node_modules/", "node_modules/", "server/", "tmp/"];
const RESOURCE_EXT = /\.(et|layout|conf|imageset|emat|xob|ent|layer|st|meta)$/i;

// EL_ not glued to a preceding capital or digit (FUEL_, MODEL_ stay).
const EL_TOKEN = /(?<![A-Z0-9])EL_\w*/g;
const RES_PATH_EXT = /^\.(et|layout|conf|imageset)\b/;
const LF_TOKEN = /(?<![A-Z0-9])LF_\w*/g;

function git(...a) {
  return execFileSync("git", a, { cwd: ROOT, encoding: "utf8", maxBuffer: 1 << 28 });
}

const tracked = git("ls-files", "-z").split("\0").filter(Boolean);
const textFiles = tracked.filter(
  (f) => !BINARY_EXT.test(f) && !KEEP_FILES.has(f) && !SKIP_PREFIX.some((p) => f.startsWith(p)) && existsSync(join(ROOT, f)),
);

const contents = new Map();
for (const f of textFiles) {
  const buf = readFileSync(join(ROOT, f));
  if (buf.includes(0)) continue;
  contents.set(f, buf.toString("utf8"));
}

// 1. declared symbols
const declared = new Set();
const declKinds = { class: 0, enum: 0, typedef: 0 };
for (const [f, text] of contents) {
  if (!f.endsWith(".c")) continue;
  for (const m of text.matchAll(/^[ \t]*(?:modded[ \t]+|sealed[ \t]+)?(class|enum)[ \t]+(EL_\w+)/gm)) {
    if (!declared.has(m[2])) declKinds[m[1]]++;
    declared.add(m[2]);
  }
  for (const m of text.matchAll(/^[ \t]*typedef\b[^;\n]*?\b(EL_\w+)[ \t]*;/gm)) {
    if (!declared.has(m[1])) declKinds.typedef++;
    declared.add(m[1]);
  }
}

// 3. resource basenames that stay
const resourceBase = new Set();
for (const f of tracked) {
  if (f.endsWith(".c") || !RESOURCE_EXT.test(f)) continue;
  const base = basename(f).replace(/\.meta$/i, "");
  if (!base.startsWith("EL_")) continue;
  resourceBase.add(base.replace(/\.[^.]+$/, ""));
}

// 2. every token
const tokenCount = new Map();
const lfTokens = new Set();
for (const text of contents.values()) {
  for (const m of text.matchAll(EL_TOKEN)) tokenCount.set(m[0], (tokenCount.get(m[0]) ?? 0) + 1);
  for (const m of text.matchAll(LF_TOKEN)) lfTokens.add(m[0]);
}
const keptResource = [];
const map = new Map();
for (const t of tokenCount.keys()) {
  if (resourceBase.has(t) && !declared.has(t)) {
    keptResource.push(t);
    continue;
  }
  map.set(t, "LF_" + t.slice(3));
}

// 5. collisions
const collisions = [];
for (const [from, to] of map) {
  if (lfTokens.has(to)) collisions.push(`${from} -> ${to} (token ${to} already exists)`);
}
const scriptRenames = [];
const trackedSet = new Set(tracked);
for (const f of tracked) {
  const b = basename(f);
  if (!f.endsWith(".c") || !b.startsWith("EL_") || f.startsWith("server/")) continue;
  const to = join(dirname(f), "LF_" + b.slice(3)).replace(/\\/g, "/");
  scriptRenames.push([f, to]);
  if (trackedSet.has(to)) collisions.push(`file ${f} -> ${to} (target exists)`);
}

function rewrite(text) {
  return text
    .replace(EL_TOKEN, (t, off, whole) => {
      if (resourceBase.has(t) && RES_PATH_EXT.test(whole.slice(off + t.length, off + t.length + 12))) return t;
      return map.get(t) ?? t;
    })
    .replace(/\bELDebug/g, "LFDebug")
    .replace(/\bELTEST\b/g, "LFTEST")
    .replace(/\beldebug-lines\b/g, "lfdebug-lines");
}

let changedFiles = 0;
for (const [f, text] of contents) {
  const next = rewrite(text);
  if (next === text) continue;
  changedFiles++;
  if (APPLY) writeFileSync(join(ROOT, f), next);
}

if (APPLY) {
  for (const [from, to] of scriptRenames) git("mv", from, to);
}

console.log(`mode: ${APPLY ? "APPLY" : "dry run"}`);
console.log(`declared symbols: ${declared.size} (class ${declKinds.class}, enum ${declKinds.enum}, typedef ${declKinds.typedef})`);
console.log(`distinct EL_ tokens: ${tokenCount.size}; renamed: ${map.size}; kept as resource names: ${keptResource.length}`);
console.log(`files rewritten: ${changedFiles}; script files renamed: ${scriptRenames.length}`);
console.log(`resource names kept (paths are {GUID}path references): ${keptResource.sort().join(" ")}`);
const shared = [...resourceBase].filter((t) => declared.has(t)).sort();
console.log(`class and resource share a name (class renamed, resource path kept): ${shared.join(" ")}`);
console.log(`collisions: ${collisions.length}`);
for (const c of collisions) console.log(`  COLLISION ${c}`);
const undeclared = [...map.keys()].filter((t) => !declared.has(t) && t !== "EL_");
console.log(`non-class tokens renamed (methods, constants, defines, keys): ${undeclared.length}`);

if (MAP_OUT) {
  const out = { classes: {}, other: {}, files: Object.fromEntries(scriptRenames) };
  for (const [f, t] of map) (declared.has(f) ? out.classes : out.other)[f] = t;
  writeFileSync(resolve(ROOT, MAP_OUT), JSON.stringify(out, null, 2) + "\n");
  console.log(`map written: ${MAP_OUT}`);
}

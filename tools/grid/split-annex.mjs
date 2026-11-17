#!/usr/bin/env node
// tools/grid/split-annex.mjs: split the annex out of DebugWorld (docs/architecture/three-worlds.md).
//
//   node tools/grid/split-annex.mjs --copy  [--src <layers>] [--dst <layers>] [--dry-run]
//   node tools/grid/split-annex.mjs --strip [--src <layers>] [--dry-run]
//
// An entity is grid when its own coords stand inside the annex rectangle (x 18..114, z 18..114) and
// it is on a legacy annex layer or a Grid_* layer. --copy writes those entities into GridWorld's
// layer folder, one file per source layer, each entity's text byte for byte (LF). --strip removes
// them from DebugWorld: a layer with nothing left outside is deleted, any other is rewritten with the
// outside entities only (slice W6). default, Gamemode and Factions are never touched: GridWorld has
// its own.
import { existsSync, mkdirSync, readFileSync, readdirSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { pathToFileURL } from "node:url";
import { LEGACY_ANNEX, SYSTEM_LAYERS } from "../lib/world-zones.mjs";
import { gridLayersDir, playtestLayerDirs } from "../lib/worlds.mjs";

// The annex fence runs x 20..112, z 20..112; 2 m of slack takes in posts and signs on the line.
export const ANNEX_RECT = { x0: 18, z0: 18, x1: 114, z1: 114 };

// Layer files whose entities may be grid: the legacy annex layers and every Grid_* layer.
export function isGridCandidate(file) {
  const name = file.replace(/\.layer$/, "");
  if (!file.endsWith(".layer") || SYSTEM_LAYERS.has(name)) return false;
  return LEGACY_ANNEX.has(name) || /^Grid_/.test(name);
}

// A layer's text as top-level entity blocks. A block starts on a line with no indent and runs to the
// first line that is exactly "}" (nested blocks close indented). Text between blocks (blank lines)
// rides with the next block as `lead`; whatever follows the last block is `tail`. Joining every
// lead + text + tail gives an LF-ended file back byte for byte.
export function parseLayer(text) {
  const lines = text.split("\n");
  const blocks = [];
  let lead = [];
  let i = 0;
  // split() leaves "" after a final "\n"; that is not a line.
  const n = text.endsWith("\n") ? lines.length - 1 : lines.length;
  while (i < n) {
    const line = lines[i];
    if (!/^[A-Za-z_]/.test(line)) {
      lead.push(line);
      i++;
      continue;
    }
    const start = i;
    const opens = (line.match(/{/g) || []).length;
    const closes = (line.match(/}/g) || []).length;
    if (opens > 0 && opens === closes) i++;
    else {
      i++;
      while (i < n && lines[i] !== "}") i++;
      if (i >= n) throw new Error(`unclosed block from line ${start + 1}: ${line.slice(0, 60)}`);
      i++;
    }
    const body = lines.slice(start, i);
    const head = /^(\w+)(?:\s+([A-Za-z_]\w*))?/.exec(line);
    const c = body.map((l) => /^ coords (\S+) (\S+) (\S+)\s*$/.exec(l)).find(Boolean);
    blocks.push({
      lead: lead.length ? lead.join("\n") + "\n" : "",
      text: body.join("\n") + "\n",
      cls: head[1],
      name: head[2] && head[2] !== "components" ? head[2] : "",
      coords: c ? [Number(c[1]), Number(c[2]), Number(c[3])] : null,
    });
    lead = [];
  }
  return { blocks, tail: lead.length ? lead.join("\n") + "\n" : "" };
}

// No coords means the origin, which is outside the annex.
export function isInside(block, rect = ANNEX_RECT) {
  if (!block.coords) return false;
  const [x, , z] = block.coords;
  return x >= rect.x0 && x <= rect.x1 && z >= rect.z0 && z <= rect.z1;
}

const join_ = (blocks, tail) => blocks.map((b) => b.lead + b.text).join("") + tail;

// { inside: text|null, outside: text|null, counts } for one layer's text. A side with no entities is
// null; the tail stays with a side that keeps every block.
export function splitLayer(text, rect = ANNEX_RECT) {
  const { blocks, tail } = parseLayer(text);
  const ins = blocks.filter((b) => isInside(b, rect));
  const outs = blocks.filter((b) => !isInside(b, rect));
  const side = (list) => (list.length ? join_(list, list.length === blocks.length ? tail : "") : null);
  return { inside: side(ins), outside: side(outs), counts: { inside: ins.length, outside: outs.length } };
}

const layerFiles = (dir) => (existsSync(dir) ? readdirSync(dir).filter(isGridCandidate).sort() : []);

// Writes the inside entities of every candidate layer in src to dst. Returns [{ file, inside, outside }].
export function copyAnnex(src, dst, { dryRun = false } = {}) {
  const plan = [];
  for (const file of layerFiles(src)) {
    const s = splitLayer(readFileSync(join(src, file), "utf8"));
    plan.push({ file, ...s.counts });
    if (dryRun || !s.inside) continue;
    mkdirSync(dst, { recursive: true });
    writeFileSync(join(dst, file), s.inside);
  }
  return plan;
}

// Removes the inside entities from every candidate layer in src. Returns [{ file, inside, outside, action }].
export function stripAnnex(src, { dryRun = false } = {}) {
  const plan = [];
  for (const file of layerFiles(src)) {
    const s = splitLayer(readFileSync(join(src, file), "utf8"));
    let action = "keep";
    if (s.counts.inside && !s.outside) action = "delete";
    else if (s.counts.inside) action = "rewrite";
    plan.push({ file, ...s.counts, action });
    if (dryRun) continue;
    if (action === "delete") rmSync(join(src, file));
    else if (action === "rewrite") writeFileSync(join(src, file), s.outside);
  }
  return plan;
}

function main(argv) {
  const arg = (k) => {
    const i = argv.indexOf(k);
    return i >= 0 ? argv[i + 1] : undefined;
  };
  const dryRun = argv.includes("--dry-run");
  const src = arg("--src") || playtestLayerDirs()[0];
  if (argv.includes("--copy")) {
    const dst = arg("--dst") || gridLayersDir();
    for (const p of copyAnnex(src, dst, { dryRun })) console.log(`${p.inside ? "copy" : "skip"}  ${p.file}  inside ${p.inside}, outside ${p.outside}`);
    console.log(`SUMMARY split-annex copy ok ${dryRun ? "(dry run) " : ""}dst=${dst}`);
    return 0;
  }
  if (argv.includes("--strip")) {
    for (const p of stripAnnex(src, { dryRun })) console.log(`${p.action.padEnd(7)} ${p.file}  inside ${p.inside}, outside ${p.outside}`);
    console.log(`SUMMARY split-annex strip ok ${dryRun ? "(dry run) " : ""}src=${src}`);
    return 0;
  }
  console.log("usage: node tools/grid/split-annex.mjs --copy|--strip [--src <layers>] [--dst <layers>] [--dry-run]");
  return 2;
}

if (process.argv[1] && pathToFileURL(process.argv[1]).href === import.meta.url) process.exit(main(process.argv.slice(2)));
export { main };

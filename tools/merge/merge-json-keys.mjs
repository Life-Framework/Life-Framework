// Git merge driver for key-addressed JSON data (website/content/gallery.json).
//
// A line merge fails on JSON because appending after the last entry edits that
// entry's trailing comma. This parses the three sides and merges by key:
//   - top-level objects (captions, issueTitles, ...): union of keys; a key that
//     both sides changed to different values is a conflict on that key only.
//   - top-level arrays (sections, skip): union of entries, object entries keyed
//     by `id` (else `name`); a deletion on either side is honoured.
// Output is re-serialized (BOM stripped, never written) in the file's house layout (one member or element per
// line, nested values inline). A conflicting key is written as a marker block
// and the driver exits 1.
//
//   node tools/merge/merge-json-keys.mjs %O %A %B %P

import { readFileSync, writeFileSync } from "node:fs";

const isObj = (v) => v !== null && typeof v === "object" && !Array.isArray(v);
const same = (a, b) => JSON.stringify(a) === JSON.stringify(b);
const has = (o, k) => Object.prototype.hasOwnProperty.call(o, k);
const ABSENT = Symbol("absent");

// Inline JSON with spaces inside braces: { "a": 1, "b": 2 }.
function inline(v) {
  if (Array.isArray(v)) return `[${v.map(inline).join(", ")}]`;
  if (isObj(v)) {
    const parts = Object.keys(v).map((k) => `${JSON.stringify(k)}: ${inline(v[k])}`);
    return parts.length ? `{ ${parts.join(", ")} }` : "{}";
  }
  return JSON.stringify(v);
}

function entryKey(item) {
  if (isObj(item) && has(item, "id")) return `id:${item.id}`;
  if (isObj(item) && has(item, "name") && typeof item.name === "string") return `name:${item.name}`;
  return `v:${JSON.stringify(item)}`;
}

// Three-way pick of one value. ABSENT means missing on that side.
function pick(base, ours, theirs) {
  if (same(ours, theirs)) return { value: ours };
  if (same(ours, base)) return { value: theirs };
  if (same(theirs, base)) return { value: ours };
  return { conflict: [ours, theirs] };
}

const side = (obj, k) => (has(obj, k) ? obj[k] : ABSENT);

function mergeMembers(base, ours, theirs) {
  const keys = [...Object.keys(ours)];
  for (const k of Object.keys(theirs)) if (!keys.includes(k)) keys.push(k);
  const members = [];
  for (const k of keys) {
    const r = pick(side(base, k), side(ours, k), side(theirs, k));
    if (r.conflict) members.push({ key: k, conflict: r.conflict });
    else if (r.value !== ABSENT) members.push({ key: k, value: r.value });
  }
  return members;
}

function mergeElements(base, ours, theirs) {
  const index = (arr) => new Map(arr.map((item) => [entryKey(item), item]));
  const b = index(base);
  const o = index(ours);
  const t = index(theirs);
  const keys = [...o.keys()];
  for (const k of t.keys()) if (!keys.includes(k)) keys.push(k);
  const members = [];
  for (const k of keys) {
    const r = pick(b.has(k) ? b.get(k) : ABSENT, o.has(k) ? o.get(k) : ABSENT, t.has(k) ? t.get(k) : ABSENT);
    if (r.conflict) members.push({ conflict: r.conflict });
    else if (r.value !== ABSENT) members.push({ value: r.value });
  }
  return members;
}

export function mergeJsonKeys(baseRaw, oursRaw, theirsRaw) {
  const strip = (text) => text.replace(/^﻿/, "");
  const baseText = strip(baseRaw);
  const oursText = strip(oursRaw);
  const theirsText = strip(theirsRaw);
  let base;
  let ours;
  let theirs;
  try {
    base = baseText.trim() ? JSON.parse(baseText) : {};
    ours = JSON.parse(oursText);
    theirs = JSON.parse(theirsText);
  } catch (error) {
    return { ok: false, conflicts: 1, text: oursText, error: `unparseable JSON: ${error.message}` };
  }
  if (!isObj(ours) || !isObj(theirs) || !isObj(base)) {
    return { ok: false, conflicts: 1, text: oursText, error: "top level is not an object" };
  }
  const eol = oursText.includes("\r\n") ? "\r\n" : "\n";
  let conflicts = 0;

  const block = (indent, a, b) => {
    conflicts++;
    return ["<<<<<<< ours", `${indent}${a}`, "=======", `${indent}${b}`, ">>>>>>> theirs"];
  };

  const container = (open, close, members, kind, comma) => {
    const lines = [`  ${open}`];
    const last = members.length - 1;
    members.forEach((m, i) => {
      const c = i < last ? "," : "";
      const name = kind === "object" ? `${JSON.stringify(m.key)}: ` : "";
      if (m.conflict) lines.push(...block("    ", `${name}${inline(m.conflict[0])}`, `${name}${inline(m.conflict[1])}`));
      else lines.push(`    ${name}${inline(m.value)}${c}`);
    });
    lines.push(`  ${close}${comma}`);
    return lines;
  };

  const out = ["{"];
  const keys = [...Object.keys(ours)];
  for (const k of Object.keys(theirs)) if (!keys.includes(k)) keys.push(k);
  const kept = [];
  for (const k of keys) {
    const o = side(ours, k);
    const t = side(theirs, k);
    const b = side(base, k);
    const r = pick(b, o, t);
    if (r.value === ABSENT) continue;
    kept.push({ key: k, r, o, t, b });
  }
  kept.forEach(({ key, r, o, t, b }, index) => {
    const comma = index < kept.length - 1 ? "," : "";
    const name = JSON.stringify(key);
    if (isObj(o) && isObj(t)) {
      out.push(...container(name + ": {", "}", mergeMembers(isObj(b) ? b : {}, o, t), "object", comma));
    } else if (Array.isArray(o) && Array.isArray(t)) {
      out.push(...container(name + ": [", "]", mergeElements(Array.isArray(b) ? b : [], o, t), "array", comma));
    } else if (!r.conflict) {
      const v = r.value;
      if (isObj(v)) out.push(...container(name + ": {", "}", mergeMembers({}, v, v), "object", comma));
      else if (Array.isArray(v)) out.push(...container(name + ": [", "]", mergeElements([], v, v), "array", comma));
      else out.push(`  ${name}: ${inline(v)}${comma}`);
    } else {
      out.push(...block("  ", `${name}: ${inline(o)}`, `${name}: ${inline(t)}`));
    }
  });
  out.push("}");
  return { ok: conflicts === 0, conflicts, text: out.join(eol) + eol };
}

function runDriver() {
  const [basePath, oursPath, theirsPath, path] = process.argv.slice(2);
  if (!basePath || !oursPath || !theirsPath) {
    console.error("usage: merge-json-keys.mjs <base> <ours> <theirs> [path]");
    process.exit(2);
  }
  const merged = mergeJsonKeys(
    readFileSync(basePath, "utf8"),
    readFileSync(oursPath, "utf8"),
    readFileSync(theirsPath, "utf8"),
  );
  if (merged.error) {
    console.error(`lf-json-keys: ${path || oursPath}: ${merged.error}`);
    process.exit(1);
  }
  writeFileSync(oursPath, merged.text);
  if (!merged.ok) {
    console.error(`lf-json-keys: ${merged.conflicts} conflict(s) in ${path || oursPath}; the same key changed on both sides`);
    process.exit(1);
  }
}

const invoked = process.argv[1] && process.argv[1].replace(/\\/g, "/").endsWith("tools/merge/merge-json-keys.mjs");
if (invoked) runDriver();

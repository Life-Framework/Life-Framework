// Owner config schema (#280): parses the `[Attribute(...)]` declarations of the
// config classes offline and writes docs/config/config.schema.json plus
// docs/config/config.example.json, the shape LF_ConfigService reads.
//
//   node tools/config/schema.mjs --write    regenerate both files
//   node tools/config/schema.mjs --check    exit 1 when either is stale
//
// A config class is a `[BaseContainerProps(configRoot: true)]` class with
// `static const string SECTION = "<name>"`; its section is the owner JSON key.
// JSON keys are the member name without its Hungarian prefix, lower camel case
// (`m_iDefaultBuyPrice` is `defaultBuyPrice`; Core/Config/Feature.md).
// A list of config objects is a record object keyed by the record's stable key.
// tools/config/schema-overrides.json holds what no attribute can say: which
// record fields form the key, and the example records.
import { readFileSync, writeFileSync, readdirSync, statSync, existsSync, mkdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
export const REPO = join(here, "..", "..");
export const SCRIPTS = join(REPO, "addons", "LifeFramework", "Scripts", "Game");
export const SERVICE = join(SCRIPTS, "Core", "Config", "LF_ConfigService.c");
export const OUT_DIR = join(REPO, "docs", "config");
export const SCHEMA_PATH = join(OUT_DIR, "config.schema.json");
export const EXAMPLE_PATH = join(OUT_DIR, "config.example.json");
const OVERRIDES = join(here, "schema-overrides.json");

function walk(dir, out = []) {
  for (const e of readdirSync(dir).sort()) {
    const p = join(dir, e);
    if (statSync(p).isDirectory()) walk(p, out);
    else if (p.endsWith(".c")) out.push(p);
  }
  return out;
}

export function readScripts(dir = SCRIPTS) {
  return walk(dir).map((path) => ({ path, text: readFileSync(path, "utf8") }));
}

/** Split an attribute argument list at top-level commas, respecting quotes. */
function splitArgs(s) {
  const out = [];
  let cur = "", q = false, depth = 0;
  for (let i = 0; i < s.length; i++) {
    const c = s[i];
    if (q) {
      cur += c;
      if (c === "\\") cur += s[++i] ?? "";
      else if (c === '"') q = false;
    } else if (c === '"') { q = true; cur += c; }
    else if (c === "(" || c === "[") { depth++; cur += c; }
    else if (c === ")" || c === "]") { depth--; cur += c; }
    else if (c === "," && depth === 0) { out.push(cur.trim()); cur = ""; }
    else cur += c;
  }
  if (cur.trim()) out.push(cur.trim());
  return out;
}

const unquote = (s) => {
  const m = /^"((?:[^"\\]|\\.)*)"$/.exec(s.trim());
  return m ? m[1].replace(/\\(.)/g, "$1") : null;
};

/** `m_iDefaultBuyPrice` -> `defaultBuyPrice`. */
export function jsonKey(member) {
  const m = /^m_[a-z]{1,3}([A-Z]\w*)$/.exec(member);
  const base = m ? m[1] : member.replace(/^m_/, "");
  return base[0].toLowerCase() + base.slice(1);
}

/** Classes with their attribute-bearing members, in declaration order. */
export function parseClasses(files) {
  const classes = new Map();
  for (const { path, text } of files) {
    const heads = [...text.matchAll(/^[ \t]*class\s+(\w+)(?:\s*:\s*(\w+))?/gm)];
    heads.forEach((h, i) => {
      const start = h.index, end = i + 1 < heads.length ? heads[i + 1].index : text.length;
      const before = text.slice(Math.max(0, start - 240), start).split(/\n\s*\n/).pop();
      const body = text.slice(start, end);
      const sec = /static\s+const\s+string\s+SECTION\s*=\s*"(\w+)"/.exec(body);
      const cls = {
        name: h[1], base: h[2] ?? null, path,
        configRoot: /configRoot\s*:\s*true/.test(before),
        section: sec ? sec[1] : null,
        doc: docBefore(text, start), members: [],
      };
      let pos = 0;
      for (;;) {
        const a = body.indexOf("[Attribute(", pos);
        if (a < 0) break;
        const argStart = a + "[Attribute(".length;
        let i2 = argStart, depth = 1, q = false;
        for (; i2 < body.length && depth > 0; i2++) {
          const c = body[i2];
          if (q) { if (c === "\\") i2++; else if (c === '"') q = false; }
          else if (c === '"') q = true;
          else if (c === "(") depth++;
          else if (c === ")") depth--;
        }
        const args = splitArgs(body.slice(argStart, i2 - 1));
        pos = i2;
        const rest = body.slice(pos).replace(/^\s*\]/, "");
        const mem = /^(?:\s|\/\/[^\n]*\n|\[[^\]]*\])*(?:ref\s+)?(?:array<\s*(?:ref\s+)?(\w+)\s*>|(\w+))\s+(m_\w+)\s*(?:=[^;]*)?;/.exec(rest);
        if (!mem) continue;
        cls.members.push({ member: mem[3], list: mem[1] ?? null, type: mem[1] ?? mem[2], ...attrInfo(args) });
      }
      if (!classes.has(cls.name)) classes.set(cls.name, cls);
    });
  }
  return classes;
}

function docBefore(text, idx) {
  const lines = text.slice(0, idx).split("\n").slice(0, -1);
  const out = [];
  while (lines.length) {
    const l = lines.pop().trim();
    if (l.startsWith("[") || (!out.length && l.startsWith("//---"))) continue;
    if (l.startsWith("//!")) out.unshift(l.replace(/^\/\/!\s?/, ""));
    else break;
  }
  const first = /^.*?[.!?](?=\s|$)/.exec(out.join(" ").trim());
  return first ? first[0] : out.join(" ").trim();
}

function attrInfo(args) {
  const info = { def: unquote(args[0] ?? '""') ?? "", desc: "", min: null, max: null };
  const widget = args[1] ?? "";
  let params = null;
  args.slice(2).forEach((a, i) => {
    const named = /^(\w+)\s*:\s*(.*)$/.exec(a);
    if (named) {
      if (named[1] === "desc") info.desc = unquote(named[2]) ?? "";
      else if (named[1] === "params") params = unquote(named[2]);
    } else if (i === 1 && params === null) params = unquote(a);
  });
  // "min max [step]" on a numeric widget; a resource picker's params ("et") are not a range.
  if (params && /Slider|Spin|Edit|Auto/.test(widget)) {
    const nums = params.trim().split(/\s+/);
    if (nums.length >= 2 && nums.slice(0, 2).every((n) => /^-?\d+(\.\d+)?$/.test(n))) {
      info.min = Number(nums[0]);
      info.max = Number(nums[1]);
    }
  }
  return info;
}

const SCALAR = { int: "integer", float: "number", bool: "boolean", string: "string", ResourceName: "string" };

function scalarDefault(type, def) {
  if (type === "bool") return def === "true" ? true : def === "false" ? false : undefined;
  if (type === "int" || type === "float") return /^-?\d+(\.\d+)?$/.test(def) ? Number(def) : undefined;
  return def;
}

function allMembers(classes, cls) {
  const base = cls.base && classes.get(cls.base);
  return [...(base ? allMembers(classes, base) : []), ...cls.members];
}

function fieldSchema(classes, m, overrides, owner) {
  const keyed = classes.get(m.type);
  if (m.list && keyed && keyed.members.length) {
    const ov = overrides.records?.[keyed.name] ?? {};
    const skip = new Set(ov.keyFields ?? []);
    const props = {};
    for (const rm of allMembers(classes, keyed)) {
      if (skip.has(rm.member)) continue;
      props[jsonKey(rm.member)] = fieldSchema(classes, rm, overrides, keyed.name);
    }
    const desc = [m.desc.replace(/\.?$/, "."), ov.key ? `Records are an object keyed by a stable key. ${ov.key}` : ""].filter((x) => x !== ".").join(" ");
    return {
      type: "object", description: desc,
      additionalProperties: { type: "object", properties: props, additionalProperties: false },
    };
  }
  if (m.list) {
    const t = SCALAR[m.type];
    if (!t) throw new Error(`${owner}.${m.member}: a list of ${m.type} has no JSON mapping (make it a config class or a scalar)`);
    return { type: "array", description: m.desc, items: { type: t }, default: [] };
  }
  if (m.type === "vector") throw new Error(`${owner}.${m.member}: vector has no JSON mapping yet`);
  const known = SCALAR[m.type];
  const s = { type: known ?? "integer" }; // any other type is an enum, which is an int in JSON
  s.description = m.desc || (known ? "" : `${m.type} value, as an integer`);
  const d = scalarDefault(known ? m.type : "int", m.def);
  if (d !== undefined) s.default = d;
  if (m.min !== null && s.type !== "string" && s.type !== "boolean") { s.minimum = m.min; s.maximum = m.max; }
  if (!s.description) delete s.description;
  return s;
}

/** The runtime schema version, read from the config service so the two cannot drift. */
export function schemaVersion(serviceText) {
  const m = /SCHEMA_VERSION\s*=\s*(\d+)/.exec(serviceText);
  if (!m) throw new Error("LF_ConfigService.SCHEMA_VERSION not found");
  return Number(m[1]);
}

/** { schema, example } from the script files, the service text and the overrides object. */
export function generate(files, serviceText, overrides) {
  const classes = parseClasses(files);
  const sections = [...classes.values()].filter((c) => c.configRoot && c.section)
    .sort((a, b) => a.section.localeCompare(b.section));
  const version = schemaVersion(serviceText);
  const props = {
    schemaVersion: {
      type: "integer", minimum: 1, maximum: version, default: version,
      description: "Owner file schema version. A file without it is version 1.",
    },
  };
  const example = { schemaVersion: version };
  for (const c of sections) {
    const sp = {}, ex = {};
    for (const m of allMembers(classes, c)) {
      const key = jsonKey(m.member);
      const fs = fieldSchema(classes, m, overrides, c.name);
      sp[key] = fs;
      const given = overrides.examples?.[c.section]?.[key];
      if (given !== undefined) ex[key] = given;
      else if (fs.type === "object") ex[key] = {};
      else if (fs.default !== undefined) ex[key] = fs.default;
    }
    props[c.section] = {
      type: "object", description: c.doc || `${c.name} owner section.`,
      properties: sp, additionalProperties: false, "x-class": c.name,
    };
    example[c.section] = ex;
  }
  const schema = {
    $schema: "https://json-schema.org/draft/2020-12/schema",
    title: "Life Framework owner config",
    description: "The owner's $profile:LifeFramework/config.json. Generated by tools/config/schema.mjs from the config classes' [Attribute] declarations; do not edit by hand.",
    type: "object", properties: props, additionalProperties: false,
  };
  return { schema, example };
}

export const render = (obj) => JSON.stringify(obj, null, 2) + "\n";

/**
 * Validates a value against the subset of JSON Schema this generator emits
 * (type, properties, additionalProperties, items, minimum, maximum). Returns messages.
 */
export function validateJson(value, schema, path = "$") {
  const errs = [];
  const t = schema.type;
  const kind = value === null ? "null" : Array.isArray(value) ? "array" : typeof value;
  const ok = t === "integer" ? Number.isInteger(value) : t === "number" ? kind === "number" : t === kind;
  if (t && !ok) return [`${path}: expected ${t}, got ${kind === "number" ? "a non-integer number" : kind}`];
  if (typeof value === "number") {
    if (schema.minimum !== undefined && value < schema.minimum) errs.push(`${path}: ${value} is below the minimum ${schema.minimum}`);
    if (schema.maximum !== undefined && value > schema.maximum) errs.push(`${path}: ${value} is above the maximum ${schema.maximum}`);
  }
  if (kind === "array" && schema.items) value.forEach((v, i) => errs.push(...validateJson(v, schema.items, `${path}[${i}]`)));
  if (kind === "object") {
    for (const [k, v] of Object.entries(value)) {
      const sub = schema.properties?.[k];
      if (sub) errs.push(...validateJson(v, sub, `${path}.${k}`));
      else if (schema.additionalProperties === false) errs.push(`${path}.${k}: unknown key`);
      else if (typeof schema.additionalProperties === "object") errs.push(...validateJson(v, schema.additionalProperties, `${path}.${k}`));
    }
  }
  return errs;
}

/** The example the service writes at runtime (WriteExample), rebuilt from its source; null when unreadable. */
export function runtimeExample(serviceText) {
  const body = /WriteExample\([^)]*\)\s*\{([\s\S]*?)\n\t\}\n/.exec(serviceText)?.[1];
  if (!body) return null;
  const version = schemaVersion(serviceText);
  const lines = [];
  for (const m of body.matchAll(/handle\.WriteLine\(\s*(?:string\.Format\(\s*)?"((?:[^"\\]|\\.)*)"/g)) {
    lines.push(m[1].replace(/\\(.)/g, "$1").replace("%1", String(version)));
  }
  try { return JSON.parse(lines.join("\n")); } catch { return null; }
}

export function build() {
  const overrides = JSON.parse(readFileSync(OVERRIDES, "utf8"));
  return generate(readScripts(), readFileSync(SERVICE, "utf8"), overrides);
}

if (process.argv[1] && /[\\/]schema\.mjs$/.test(process.argv[1])) {
  const { schema, example } = build();
  const out = [[SCHEMA_PATH, render(schema)], [EXAMPLE_PATH, render(example)]];
  if (process.argv.includes("--write")) {
    mkdirSync(OUT_DIR, { recursive: true });
    for (const [p, t] of out) writeFileSync(p, t);
    console.log(`config-schema: wrote ${out.length} files in docs/config/`);
  } else if (process.argv.includes("--check")) {
    const stale = out.filter(([p, t]) => !existsSync(p) || readFileSync(p, "utf8") !== t);
    for (const [p] of stale) console.log(`stale: ${p}`);
    process.exit(stale.length ? 1 : 0);
  } else {
    const names = Object.keys(schema.properties).slice(1);
    console.log(`${names.length} section(s): ${names.join(", ")}`);
  }
}

/**
 * Range agreement (#279): the limits an attribute's `params: "min max"` declares and the
 * constants the apply step passes to `reader.ReadInt/ReadFloat("key", target, MIN, MAX)` are
 * written twice by hand. Returns messages for a mismatch, a checked read whose field has no
 * range, and a ranged field no read checks. `files` are { path, text } as readScripts gives.
 */
export function rangeProblems(files) {
  const classes = parseClasses(files);
  const text = new Map(files.map((f) => [f.path, f.text]));
  const msgs = [];
  for (const root of [...classes.values()].filter((c) => c.configRoot && c.section)) {
    const fields = new Map(); // json key -> [{ cls, min, max }]
    const paths = new Set([root.path]);
    const add = (cls) => {
      for (const m of allMembers(classes, cls)) {
        const k = jsonKey(m.member);
        if (!fields.has(k)) fields.set(k, []);
        fields.get(k).push({ cls: cls.name, min: m.min, max: m.max, numeric: m.type === "int" || m.type === "float" });
      }
      paths.add(cls.path);
    };
    add(root);
    for (const m of root.members) if (m.list && classes.get(m.type)?.members.length) add(classes.get(m.type));
    const checked = new Set();
    for (const p of paths) {
      const src = text.get(p);
      const num = (expr) => {
        const e = expr.trim();
        if (/^-?\d+(\.\d+)?$/.test(e)) return Number(e);
        const name = e.split(".").pop();
        const c = new RegExp(String.raw`const\s+(?:int|float)\s+${name}\s*=\s*(-?\d+(?:\.\d+)?)\s*;`).exec(src);
        return c ? Number(c[1]) : null;
      };
      for (const m of src.matchAll(/\.Read(?:Int|Float)\(\s*"(\w+)"\s*,\s*[^,()]+,\s*([^,()]+?)\s*,\s*([^,()]+?)\s*\)/g)) {
        const [, key, lo, hi] = m;
        checked.add(`${root.section}.${key}`);
        const decl = fields.get(key);
        const min = num(lo), max = num(hi);
        if (!decl) continue;
        for (const d of decl) {
          if (d.min === null) msgs.push(`${root.section}.${key}: ReadInt passes ${lo}..${hi} but ${d.cls} has no params range on the attribute`);
          else if (min === null || max === null) msgs.push(`${root.section}.${key}: cannot resolve ${lo}..${hi} to numbers (use literals or a const int in ${p.split(/[\/]/).pop()})`);
          else if (min !== d.min || max !== d.max) msgs.push(`${root.section}.${key}: ReadInt checks ${min}..${max} but the ${d.cls} attribute params say ${d.min}..${d.max}`);
        }
      }
    }
    for (const [key, decl] of fields) {
      for (const d of decl) {
        if (d.min !== null && d.numeric && !checked.has(`${root.section}.${key}`)) {
          msgs.push(`${root.section}.${key}: the ${d.cls} attribute declares ${d.min}..${d.max} but no ReadInt/ReadFloat passes a range for it`);
        }
      }
    }
  }
  return msgs;
}

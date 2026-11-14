// tools/validation/validate-record-shapes.mjs
// inputs: addons/**, tools/validation/record-shapes.json
//
// The append-only rule for save records (#761, docs/persistence.md "Record
// shapes"). LF_PersistenceComponentSerializer writes each payload as an array
// of record objects by reflection, so a record's saved shape is its serialized
// fields in declaration order (base class first, nested LF_ records included).
// tools/validation/record-shapes.json locks that shape per record class at the
// current save version. This check fails when:
//   - a locked field is removed, renamed, reordered or retyped (never legal:
//     every older save is positional);
//   - a locked record class is no longer written, or a payload writes a
//     different record class;
//   - a serialized field is appended to a locked record, or a new record class
//     is written, without raising the save version;
//   - the save version moved and the lock was not refreshed (`--update`);
//   - the lock says "launched": a shipped record takes no new serialized field
//     at all; new data is a sidecar payload ([NonSerialized()] on the live
//     record, its own row class appended to RegisterPersistencePayloads).
// [NonSerialized()], static and const members are not part of the shape.
//
//   node tools/validation/validate-record-shapes.mjs           check
//   node tools/validation/validate-record-shapes.mjs --update  refresh the lock
//     after a legal change (refuses while a hard error stands)
//   node tools/validation/validate-record-shapes.mjs --report  print the audit table
//
// red-proof: tools/validation/record-shapes-selftest.mjs plants each violation
// in fixture sources and asserts this check reports it.

import { existsSync, readdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, relative } from "node:path";
import { fileURLToPath } from "node:url";

export const SERIALIZER = "addons/LifeFramework/Scripts/Game/Persistence/Serializers/LF_PersistenceComponentSerializer.c";
export const LOCK = "tools/validation/record-shapes.json";
const PREFIX_PAYLOAD = "prefix";

// ---------------------------------------------------------------- parsing

// Comments and string contents go; attributes and code stay. Offsets do not
// matter to callers, so strings collapse to "".
export function stripComments(text) {
  let out = "";
  for (let i = 0; i < text.length; i++) {
    const c = text[i];
    const n = text[i + 1];
    if (c === "/" && n === "/") {
      while (i < text.length && text[i] !== "\n") i++;
      out += "\n";
    } else if (c === "/" && n === "*") {
      i += 2;
      while (i < text.length && !(text[i] === "*" && text[i + 1] === "/")) i++;
      i++;
    } else if (c === '"') {
      i++;
      while (i < text.length && text[i] !== '"') {
        if (text[i] === "\\") i++;
        i++;
      }
      out += '""';
    } else {
      out += c;
    }
  }
  return out;
}

function matchBrace(text, open) {
  let depth = 0;
  for (let i = open; i < text.length; i++) {
    if (text[i] === "{") depth++;
    else if (text[i] === "}") {
      depth--;
      if (depth === 0) return i;
    }
  }
  return -1;
}

// name -> { base, body, file }. Only plain declarations; a modded class is
// noted so the audit can flag it.
export function indexClasses(files) {
  const classes = new Map();
  const modded = new Set();
  for (const [file, raw] of files) {
    const text = stripComments(raw);
    const re = /(^|[\s;}])(modded\s+)?class\s+(LF_\w+)\s*(?::\s*(\w+))?\s*\{/g;
    let m;
    while ((m = re.exec(text))) {
      const open = text.indexOf("{", m.index + m[0].length - 1);
      const close = matchBrace(text, open);
      if (close < 0) continue;
      if (m[2]) {
        modded.add(m[3]);
        continue;
      }
      classes.set(m[3], { base: m[4] ?? null, body: text.slice(open + 1, close), file });
    }
  }
  return { classes, modded };
}

function normType(type) {
  return type.replace(/\s+/g, " ").replace(/\s*([<>,])\s*/g, "$1").trim();
}

// Split on commas outside <> and ().
function splitTop(text) {
  const parts = [];
  let depth = 0;
  let cur = "";
  for (const c of text) {
    if (c === "<" || c === "(") depth++;
    else if (c === ">" || c === ")") depth--;
    if (c === "," && depth === 0) {
      parts.push(cur);
      cur = "";
    } else cur += c;
  }
  parts.push(cur);
  return parts;
}

const MODIFIERS = new Set(["protected", "private", "public", "ref", "autoptr", "owned", "notnull", "volatile", "local"]);
const SKIP_MODIFIERS = new Set(["static", "const", "proto", "native", "external", "event", "override", "sealed"]);

// The serialized fields of one class body, in declaration order:
// [{ type, name, nonSerialized }]. Methods and nested blocks are skipped.
export function parseFields(body) {
  const fields = [];
  let depth = 0;
  let stmt = "";
  let head = null; // text before the first { of a statement at depth 0
  for (let i = 0; i < body.length; i++) {
    const c = body[i];
    if (c === "{") {
      if (depth === 0) head = stmt;
      depth++;
      stmt += c;
      continue;
    }
    if (c === "}") {
      depth--;
      stmt += c;
      if (depth === 0 && head !== null && !head.includes("=")) {
        // A method, enum or nested block ended.
        stmt = "";
        head = null;
      }
      continue;
    }
    if (c === ";" && depth === 0) {
      addStatement(stmt, fields);
      stmt = "";
      head = null;
      continue;
    }
    stmt += c;
  }
  return fields;
}

function addStatement(stmt, fields) {
  let text = stmt;
  let nonSerialized = false;
  text = text.replace(/\[([^\]]*)\]/g, (all, inner) => {
    if (/\bNonSerialized\b/.test(inner)) nonSerialized = true;
    return " ";
  });
  const eq = text.indexOf("=");
  const decl = (eq >= 0 ? text.slice(0, eq) : text).trim();
  if (!decl || decl.includes("(")) return;
  const tokens = decl.split(/\s+/);
  // Re-join generic types split by spaces: work on the string instead.
  let rest = decl;
  for (;;) {
    const word = rest.match(/^(\w+)\s+/);
    if (!word) break;
    if (SKIP_MODIFIERS.has(word[1])) return;
    if (!MODIFIERS.has(word[1])) break;
    rest = rest.slice(word[0].length);
  }
  if (tokens.length < 2) return;
  const parts = splitTop(rest);
  const first = parts[0].trim().match(/^(.*?)\s*\b(\w+)\s*(\[\s*\d*\s*\])?$/);
  if (!first || !first[1]) return;
  const type = normType(first[1] + (first[3] ?? ""));
  fields.push({ type, name: first[2], nonSerialized });
  for (const extra of parts.slice(1)) {
    const name = extra.trim().match(/^(\w+)/);
    if (name) fields.push({ type, name: name[1], nonSerialized });
  }
}

// The payloads the serializer writes: payload name -> root record class, plus
// the prefix (accounts and bank) under "prefix", and the written version.
export function parseSerializer(raw) {
  const text = stripComments(raw);
  const versionMatch = text.match(/WriteValue\(\s*""\s*,\s*(\d+)\s*\)/) ?? raw.match(/WriteValue\("version",\s*(\d+)\)/);
  const version = versionMatch ? Number(versionMatch[1]) : null;
  const payloads = [];
  const order = [];
  const regStart = raw.indexOf("void RegisterPersistencePayloads(");
  if (regStart >= 0) {
    const open = raw.indexOf("{", regStart);
    for (const m of raw.slice(open, matchBrace(raw, open)).matchAll(/order\.Insert\("(\w+)"\)/g)) order.push(m[1]);
  }
  const typesIn = (segment) => {
    const vars = new Map();
    for (const m of segment.matchAll(/array<\s*ref\s+(LF_\w+)\s*>\s+(\w+)\s*=/g)) vars.set(m[2], m[1]);
    const roots = [];
    for (const m of segment.matchAll(/context\.Write\(\s*(\w+)\s*\)/g)) {
      if (vars.has(m[1])) roots.push(vars.get(m[1]));
    }
    return roots;
  };
  const prefixStart = raw.indexOf("void WritePrefix(");
  if (prefixStart >= 0) {
    const open = raw.indexOf("{", prefixStart);
    const body = stripComments(raw.slice(open, matchBrace(raw, open) + 1));
    for (const root of typesIn(body)) payloads.push({ payload: PREFIX_PAYLOAD, root });
  }
  const writeStart = raw.indexOf("void WritePersistencePayload(");
  if (writeStart >= 0) {
    const open = raw.indexOf("{", writeStart);
    const body = raw.slice(open, matchBrace(raw, open) + 1);
    const marks = [...body.matchAll(/payload == "(\w+)"/g)];
    marks.forEach((m, i) => {
      const end = i + 1 < marks.length ? marks[i + 1].index : body.length;
      for (const root of typesIn(stripComments(body.slice(m.index, end)))) payloads.push({ payload: m[1], root });
    });
  }
  return { version, payloads, order };
}

// Every record class reachable from the roots: name -> ["type name", ...] of
// serialized fields, base fields first. Missing classes are reported.
export function collectShapes(payloads, classes) {
  const shapes = new Map();
  const missing = [];
  const unserialized = new Map();
  const visit = (name) => {
    if (shapes.has(name)) return;
    const cls = classes.get(name);
    if (!cls) {
      missing.push(name);
      return;
    }
    shapes.set(name, []); // cycle guard
    const fields = [];
    const skipped = [];
    let base = cls.base;
    const chain = [];
    while (base && classes.has(base)) {
      chain.unshift(base);
      base = classes.get(base).base;
    }
    for (const owner of [...chain, name]) {
      for (const f of parseFields(classes.get(owner).body)) {
        if (f.nonSerialized) skipped.push(f.name);
        else fields.push(f);
      }
    }
    shapes.set(name, fields.map((f) => `${f.type} ${f.name}`));
    if (skipped.length) unserialized.set(name, skipped);
    for (const f of fields) {
      for (const ref of f.type.matchAll(/\bLF_\w+/g)) {
        if (classes.has(ref[0])) visit(ref[0]);
      }
    }
  };
  for (const p of payloads) visit(p.root);
  return { shapes, missing, unserialized };
}

// ---------------------------------------------------------------- the rule

// errors: [{ hard, msg }]. hard = never legal; soft = legal with --update.
export function compare(lock, current) {
  const errors = [];
  const hard = (msg) => errors.push({ hard: true, msg });
  const soft = (msg) => errors.push({ hard: false, msg });
  const bumped = current.version > lock.saveVersion;
  if (current.version < lock.saveVersion) {
    hard(`save version went down: the lock is at ${lock.saveVersion}, the serializer writes ${current.version}`);
  }
  for (const [payload, root] of Object.entries(lock.payloads ?? {})) {
    const now = current.payloads.filter((p) => p.payload === payload).map((p) => p.root).join(",");
    if (!now) hard(`payload "${payload}" no longer writes ${root}: a shipped payload stays in every save`);
    else if (now !== root) hard(`payload "${payload}" writes ${now}, locked as ${root}: retyping a payload breaks every older save`);
  }
  for (const [name, locked] of Object.entries(lock.records)) {
    const now = current.shapes.get(name);
    if (!now) {
      hard(`record ${name} is no longer written: a locked record keeps its class and fields`);
      continue;
    }
    for (let i = 0; i < locked.fields.length; i++) {
      const want = locked.fields[i];
      const got = now[i];
      if (got === want) continue;
      const wantName = want.split(" ").pop();
      const gotName = got ? got.split(" ").pop() : null;
      const at = now.indexOf(want);
      const sameName = now.find((f) => f.split(" ").pop() === wantName);
      if (!got) hard(`${name}: field ${i + 1} "${want}" was removed (fields are append-only)`);
      else if (gotName === wantName) hard(`${name}: field ${i + 1} "${want}" was retyped to "${got}" (fields are append-only)`);
      else if (at >= 0) hard(`${name}: field "${want}" moved from position ${i + 1} to ${at + 1} (fields are append-only)`);
      else if (sameName) hard(`${name}: field "${want}" was retyped to "${sameName}" and moved (fields are append-only)`);
      else hard(`${name}: field ${i + 1} "${want}" was removed or renamed to "${got}" (fields are append-only)`);
      break;
    }
    if (now.length > locked.fields.length && now.slice(0, locked.fields.length).join("|") === locked.fields.join("|")) {
      const added = now.slice(locked.fields.length).join(", ");
      if (lock.launched) {
        hard(`${name}: serialized field(s) ${added} added to a launched record; after the persistent launch new data is a sidecar payload: mark the field [NonSerialized()] and save it in a row payload appended to RegisterPersistencePayloads (docs/persistence.md "Record shapes")`);
      } else if (!bumped) {
        hard(`${name}: serialized field(s) ${added} appended without a save version bump (still ${current.version}); raise the version and read the older shape, or mark it [NonSerialized()] and add a sidecar payload`);
      } else {
        soft(`${name}: field(s) ${added} appended at save version ${current.version}; run node tools/validation/validate-record-shapes.mjs --update`);
      }
    }
  }
  for (const [name] of current.shapes) {
    if (lock.records[name]) continue;
    if (!bumped) hard(`new record class ${name} is written without a save version bump (still ${current.version})`);
    else soft(`new record class ${name} at save version ${current.version}; run node tools/validation/validate-record-shapes.mjs --update`);
  }
  for (const p of current.payloads) {
    if (lock.payloads?.[p.payload]) continue;
    if (!bumped) hard(`new payload "${p.payload}" is written without a save version bump`);
  }
  if (bumped && !errors.length) {
    soft(`save version moved ${lock.saveVersion} -> ${current.version}; run node tools/validation/validate-record-shapes.mjs --update to lock the shapes at ${current.version}`);
  }
  return errors;
}

// The save version a record first appeared in: its payload's position in
// RegisterPersistencePayloads (the prefix is version 1); a nested record takes
// its parent's. A locked record keeps the "since" it was locked with.
function firstVersions(current) {
  const since = new Map();
  const mark = (name, version) => {
    if (since.has(name) && since.get(name) <= version) return;
    since.set(name, version);
    for (const f of current.shapes.get(name) ?? []) {
      for (const ref of f.matchAll(/\bLF_\w+/g)) if (current.shapes.has(ref[0]) && ref[0] !== name) mark(ref[0], version);
    }
  };
  for (const p of current.payloads) {
    const index = (current.order ?? []).indexOf(p.payload);
    let version = current.version;
    if (p.payload === PREFIX_PAYLOAD) version = 1;
    else if (index >= 0) version = index + 1;
    mark(p.root, version);
  }
  return since;
}

export function buildLock(current, previous) {
  const records = {};
  const first = firstVersions(current);
  for (const [name, fields] of current.shapes) {
    const since = previous?.records?.[name]?.since ?? first.get(name) ?? current.version;
    records[name] = { since, fields };
  }
  const payloads = {};
  for (const p of current.payloads) {
    payloads[p.payload] = payloads[p.payload] ? `${payloads[p.payload]},${p.root}` : p.root;
  }
  return { saveVersion: current.version, launched: previous?.launched ?? false, payloads, records };
}

// files: Map(relPath -> text); serializerText: the serializer source.
export function readCurrent(files, serializerText) {
  const { classes, modded } = indexClasses(files);
  const { version, payloads, order } = parseSerializer(serializerText);
  const { shapes, missing, unserialized } = collectShapes(payloads, classes);
  return { version, payloads, order, shapes, missing, unserialized, modded, classes };
}

// ---------------------------------------------------------------- CLI

function walk(dir, out) {
  if (!existsSync(dir)) return out;
  for (const entry of readdirSync(dir, { withFileTypes: true })) {
    const full = join(dir, entry.name);
    if (entry.isDirectory()) {
      if (entry.name === "Tests") continue;
      walk(full, out);
    } else if (entry.name.endsWith(".c")) out.push(full);
  }
  return out;
}

export function loadRepo(root) {
  const files = new Map();
  for (const addon of readdirSync(join(root, "addons"), { withFileTypes: true })) {
    if (!addon.isDirectory()) continue;
    for (const file of walk(join(root, "addons", addon.name, "Scripts"), [])) {
      files.set(relative(root, file).replaceAll("\\", "/"), readFileSync(file, "utf8"));
    }
  }
  return readCurrent(files, readFileSync(join(root, SERIALIZER), "utf8"));
}

function report(current, lock) {
  const lines = ["| Payload | Record | Since | Serialized fields |", "|---|---|---|---|"];
  const seen = new Set();
  const row = (payload, name) => {
    if (seen.has(name)) return;
    seen.add(name);
    const since = lock?.records?.[name]?.since ?? "";
    const fields = current.shapes.get(name) ?? [];
    lines.push(`| ${payload} | \`${name}\` | ${since} | ${fields.map((f) => `\`${f}\``).join(", ")} |`);
    for (const f of fields) for (const ref of f.matchAll(/\bLF_\w+/g)) if (current.shapes.has(ref[0])) row(`(in ${name})`, ref[0]);
  };
  for (const p of current.payloads) row(p.payload, p.root);
  return lines.join("\n");
}

function main() {
  const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
  const args = process.argv.slice(2);
  const current = loadRepo(root);
  const lockPath = join(root, LOCK);
  const lock = existsSync(lockPath) ? JSON.parse(readFileSync(lockPath, "utf8")) : null;
  if (current.version === null || !current.payloads.length) {
    console.log(`FAIL record shapes: could not read the payloads or the written version from ${SERIALIZER}`);
    process.exit(1);
  }
  if (current.missing.length) {
    console.log(`FAIL record shapes: record class(es) not found in addons/*/Scripts: ${current.missing.join(", ")}`);
    process.exit(1);
  }
  const moddedRecords = [...current.shapes.keys()].filter((name) => current.modded.has(name));
  if (moddedRecords.length) {
    console.log(`FAIL record shapes: a modded class extends saved record(s) ${moddedRecords.join(", ")}; a record's fields live in its one declaration`);
    process.exit(1);
  }
  if (args.includes("--report")) {
    console.log(report(current, lock));
    return;
  }
  if (args.includes("--update")) {
    const errors = lock ? compare(lock, current) : [];
    const hardErrors = errors.filter((e) => e.hard);
    if (hardErrors.length) {
      for (const e of hardErrors) console.log(`FAIL ${e.msg}`);
      console.log("record shapes: lock not updated; fix the hard errors first");
      process.exit(1);
    }
    writeFileSync(lockPath, JSON.stringify(buildLock(current, lock), null, 2) + "\n");
    console.log(`record shapes: locked ${current.shapes.size} record class(es) at save version ${current.version}`);
    return;
  }
  if (!lock) {
    console.log(`FAIL record shapes: ${LOCK} is missing; run node tools/validation/validate-record-shapes.mjs --update`);
    process.exit(1);
  }
  const errors = compare(lock, current);
  for (const e of errors) console.log(`FAIL ${e.msg}`);
  if (errors.length) process.exit(1);
  console.log(`record shapes: ${current.shapes.size} record class(es) match the lock at save version ${current.version}`);
}

if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1]) main();

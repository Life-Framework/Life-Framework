// tools/validation/validate-record-var-names.mjs
// inputs: addons/LifeFramework/Scripts/Game/Persistence/Serializers/LF_PersistenceComponentSerializer.c, tools/validation/validate-record-var-names.mjs
//
// A save record is keyed by the variable name passed to context.Write / context.Read in
// LF_PersistenceComponentSerializer.c. A name reused by a second payload silently made every
// bought garage slot load as 0 (#2656 first used 'slotRows', already the characters payload's
// key; found in the #2795 review). This check fails when a variable name given to Write or Read:
//   - appears in more than one payload (the account/bank prefix counts as one: "prefix");
//   - is written other than exactly once, or read other than exactly once.
// So every payload owns its names, one write and one read each. Pick a fresh name for a new
// payload (docs/persistence.md "Record keys").
//
//   node tools/validation/validate-record-var-names.mjs
//
// red-proof: tools/validation/record-var-names-selftest.mjs plants each violation in a fixture
// serializer and asserts this check reports it.

import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { stripComments } from "./validate-record-shapes.mjs";

export const SERIALIZER = "addons/LifeFramework/Scripts/Game/Persistence/Serializers/LF_PersistenceComponentSerializer.c";
const PREFIX = "prefix";

// Every context.Write(name) / context.Read(name) site as { op, name, payload, line }. A site inside
// WritePersistencePayload / ReadPersistencePayload belongs to the nearest `payload == "x"` above it;
// any other site (WritePrefix, DeserializeStore's account and bank reads) belongs to the prefix.
export function collectSites(raw) {
  const text = stripComments(raw);
  const sites = [];
  const marks = [...text.matchAll(/payload == ""/g)];
  // stripComments blanks string contents, so find the payload names in the raw text at the same
  // ordinal: the n-th `payload == "x"` in raw is the n-th `payload == ""` in text.
  const names = [...raw.matchAll(/payload == "(\w+)"/g)].map((m) => m[1]);
  const fnRanges = [];
  for (const fn of ["WritePersistencePayload", "ReadPersistencePayload"]) {
    const start = text.search(new RegExp(String.raw`void\s+${fn}\s*\(`));
    if (start < 0) continue;
    const open = text.indexOf("{", start);
    let depth = 0;
    let end = text.length;
    for (let i = open; i < text.length; i++) {
      if (text[i] === "{") depth++;
      else if (text[i] === "}" && --depth === 0) {
        end = i;
        break;
      }
    }
    fnRanges.push([open, end]);
  }
  for (const m of text.matchAll(/\bcontext\.(Write|Read)\(\s*(\w+)\s*\)/g)) {
    let payload = PREFIX;
    if (fnRanges.some(([a, b]) => m.index > a && m.index < b)) {
      let mark = -1;
      marks.forEach((mk, i) => {
        if (mk.index < m.index) mark = i;
      });
      payload = mark >= 0 ? (names[mark] ?? PREFIX) : PREFIX;
    }
    const line = text.slice(0, m.index).split("\n").length;
    sites.push({ op: m[1], name: m[2], payload, line });
  }
  return sites;
}

// The error messages for a list of sites; empty when every name is owned by one payload with one
// write and one read.
export function check(sites) {
  const errors = [];
  const byName = new Map();
  for (const s of sites) {
    if (!byName.has(s.name)) byName.set(s.name, []);
    byName.get(s.name).push(s);
  }
  for (const [name, list] of byName) {
    const payloads = [...new Set(list.map((s) => s.payload))];
    if (payloads.length > 1) {
      errors.push(`record variable '${name}' is used by ${payloads.length} payloads (${payloads.join(", ")}); each payload needs its own name or one load overwrites the other (lines ${list.map((s) => s.line).join(", ")})`);
      continue;
    }
    const writes = list.filter((s) => s.op === "Write");
    const reads = list.filter((s) => s.op === "Read");
    if (writes.length !== 1 || reads.length !== 1) {
      errors.push(`record variable '${name}' in payload '${payloads[0]}' has ${writes.length} write(s) and ${reads.length} read(s); it needs exactly one of each (lines ${list.map((s) => s.line).join(", ")})`);
    }
  }
  return errors;
}

function main() {
  const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
  const sites = collectSites(readFileSync(join(root, SERIALIZER), "utf8"));
  if (!sites.length) {
    console.log(`FAIL record var names: no context.Write/Read sites found in ${SERIALIZER}`);
    process.exit(1);
  }
  const errors = check(sites);
  for (const e of errors) console.log(`FAIL ${SERIALIZER}: ${e}`);
  if (errors.length) process.exit(1);
  console.log(`record var names: ${new Set(sites.map((s) => s.name)).size} record variable(s), each in one payload with one write and one read`);
}

if (process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1]) main();

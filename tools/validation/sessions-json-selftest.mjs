// Proves website/content/sessions.schema.json is the contract `host announce` writes and the servers page reads.
// inputs: tools/**, website/content/**
// Fixtures: fixtures/sessions/{scheduled,none}.json. When website/content/sessions.json exists it is checked too.
//
// red-proof: run before the schema existed it printed "FAIL schema: website/content/sessions.schema.json
// is missing" and exited 1.
// A website/content/sessions.json with no next.at and wipe: "no" printed "FAIL website/content/sessions.json:
// $.next.at: required" and "$.next.wipe: expected boolean, got string" and exited 1; removed again.
import { existsSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const root = join(here, "..", "..");
const schemaPath = join(root, "website", "content", "sessions.schema.json");
const livePath = join(root, "website", "content", "sessions.json");
const fx = (n) => JSON.parse(readFileSync(join(here, "fixtures", "sessions", `${n}.json`), "utf8"));

const fails = [];
const expect = (ok, msg) => { if (!ok) fails.push(msg); };

/** The JSON Schema subset the contract uses: type (or a list), required, properties, additionalProperties false, enum, minLength, format date-time (with offset). */
function check(value, schema, path = "$") {
  const kind = value === null ? "null" : Array.isArray(value) ? "array" : typeof value;
  const types = schema.type === undefined ? null : [].concat(schema.type);
  if (types && !types.includes(kind)) return [`${path}: expected ${types.join("|")}, got ${kind}`];
  const errs = [];
  if (schema.enum && !schema.enum.includes(value)) errs.push(`${path}: not one of ${JSON.stringify(schema.enum)}`);
  if (kind === "string") {
    if (schema.minLength !== undefined && value.length < schema.minLength) errs.push(`${path}: shorter than ${schema.minLength}`);
    if (schema.format === "date-time") {
      const m = /^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}(\.\d+)?(Z|[+-]\d{2}:\d{2})$/.test(value);
      if (!m || Number.isNaN(Date.parse(value))) errs.push(`${path}: "${value}" is not an ISO 8601 date-time with an offset`);
    }
  }
  if (kind === "object") {
    for (const k of schema.required || []) if (!(k in value)) errs.push(`${path}.${k}: required`);
    for (const [k, v] of Object.entries(value)) {
      if (schema.properties?.[k]) errs.push(...check(v, schema.properties[k], `${path}.${k}`));
      else if (schema.additionalProperties === false) errs.push(`${path}.${k}: unknown key`);
    }
  }
  return errs;
}

if (!existsSync(schemaPath)) {
  fails.push("schema: website/content/sessions.schema.json is missing");
} else {
  const schema = JSON.parse(readFileSync(schemaPath, "utf8"));
  const bad = (doc) => check(doc, schema).length > 0;
  const scheduled = fx("scheduled");
  const none = fx("none");

  expect(check(scheduled, schema).length === 0, `scheduled fixture must validate: ${check(scheduled, schema)}`);
  expect(check(none, schema).length === 0, `none fixture must validate: ${check(none, schema)}`);
  expect(scheduled.next && typeof scheduled.next.at === "string", "scheduled fixture carries a next.at");
  expect(none.next === null, "none fixture has next: null");

  const mut = (f) => { const d = structuredClone(scheduled); f(d.next); return d; };
  expect(bad(mut((n) => { delete n.at; })), "a next without at must fail");
  expect(bad(mut((n) => { delete n.build; })), "a next without build must fail");
  expect(bad(mut((n) => { delete n.wipe; })), "a next without wipe must fail");
  // #1537: `announced` is the ISO post time and is absent on a draft that was not posted.
  expect(!bad(mut((n) => { delete n.announced; })), "a draft next without announced must pass");
  expect(check(fx("draft"), schema).length === 0, `draft fixture must validate: ${check(fx("draft"), schema)}`);
  expect(!("announced" in fx("draft").next), "draft fixture has no announced key");
  expect(bad(mut((n) => { n.announced = false; })), "announced: false must fail (a draft omits the key)");
  expect(bad(mut((n) => { n.announced = true; })), "announced: true must fail (it is the post time)");
  expect(bad(mut((n) => { n.announced = "yesterday"; })), "an announced that is not a date must fail");
  expect(bad(mut((n) => { n.announced = "2026-11-15T12:30:00"; })), "an announced without an offset must fail");
  expect(bad(mut((n) => { n.wipe = "true"; })), "a wipe that is a string must fail");
  expect(bad(mut((n) => { n.wipe = 1; })), "a wipe that is a number must fail");
  expect(bad(mut((n) => { n.at = "2026-11-20T20:00:00"; })), "an at without an offset must fail");
  expect(bad(mut((n) => { n.at = "soon"; })), "an at that is not a date must fail");
  expect(bad(mut((n) => { n.build = 12; })), "a build that is not a string must fail");
  expect(bad(mut((n) => { n.players = 12; })), "a player count must fail: the site shows the next session time only");
  expect(bad(mut((n) => { n.online = true; })), "an up/down field must fail: the site shows the next session time only");
  expect(bad({}), "a document without next must fail");
  expect(bad({ next: "tomorrow" }), "a next that is neither null nor an object must fail");
  expect(bad({ next: null, players: 3 }), "an unknown top-level key must fail");

  if (existsSync(livePath)) {
    let live = null;
    try { live = JSON.parse(readFileSync(livePath, "utf8")); } catch (e) { fails.push(`website/content/sessions.json is not JSON: ${e.message}`); }
    if (live !== null) for (const e of check(live, schema)) fails.push(`website/content/sessions.json: ${e}`);
  }
}

for (const f of fails) console.log(`FAIL ${f}`);
console.log(`SUMMARY sessions-json-selftest ${fails.length ? "fail" : "ok"}${fails.length ? ` ${fails.length} failing` : ""}`);
process.exit(fails.length ? 1 : 0);

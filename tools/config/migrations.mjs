// Dry-run migration report for `config-check` (#304). Given an owner's config.json, says which
// config migrations (#303) LF_ConfigMigrator would run at boot to bring it to the build's schema,
// in order, with each step's description and every field it renames, drops or defaults. Nothing is
// ever written. The steps are Enforce Script (LF_ConfigMigration subclasses listed in
// LF_ConfigMigrator.BuildShipped); this file reads their human-facing record from
// docs/config/config.migrations.json, which the selftest keeps in step with BuildShipped.
//
// Manifest: { "migrations": [ { "from": 1, "name": "shop-to-shops", "description": "...",
//   "changes": [ { "op": "rename", "path": "shop", "to": "shops" }, { "op": "drop", "path": "a.b" },
//                { "op": "default", "path": "a.c", "value": 5 } ] } ] }   (to = from + 1)
import { readFileSync, existsSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
export const MIGRATIONS_PATH = join(here, "..", "..", "docs", "config", "config.migrations.json");

export function loadManifest(path = MIGRATIONS_PATH) {
  if (!existsSync(path)) return { migrations: [] };
  return JSON.parse(readFileSync(path, "utf8"));
}

/** The node at a dotted path in the parsed owner file (rules.mjs shape), or null. */
function nodeAt(root, path) {
  let n = root;
  for (const key of path.split(".")) {
    if (!n || n.t !== "obj") return null;
    const p = n.props.find((q) => q.key === key);
    if (!p) return null;
    n = p.val;
  }
  return n;
}

function describeChange(c, root) {
  const has = !!nodeAt(root, c.path);
  if (c.op === "rename") return { op: c.op, path: c.path, present: has, text: `rename ${c.path} -> ${c.to} (${has ? "in your file" : "not in your file; nothing to do"})` };
  if (c.op === "drop") return { op: c.op, path: c.path, present: has, text: `drop ${c.path} (${has ? "in your file" : "not in your file; nothing to do"})` };
  return { op: c.op, path: c.path, present: has, text: `default ${c.path} = ${JSON.stringify(c.value)} (${has ? "already set; kept" : "missing from your file; added"})` };
}

/**
 * The plan for an owner file. `root` is the parsed file (or null when it did not parse),
 * `build` the schema the build reads. Returns
 * { fileVersion, build, status: "current"|"older"|"newer"|"invalid"|"unreadable", steps, missing }.
 */
export function planMigrations(root, build, manifest) {
  const plan = { fileVersion: null, build, status: "current", steps: [], missing: null };
  if (!root || root.t !== "obj") { plan.status = "unreadable"; return plan; }
  const p = root.props.find((q) => q.key === "schemaVersion");
  if (!p) return plan; // no schemaVersion reads as 1 and is not migrated
  const v = p.val;
  if (v.t !== "num" || !Number.isInteger(v.value) || v.value < 1) { plan.status = "invalid"; return plan; }
  plan.fileVersion = v.value;
  if (v.value > build) { plan.status = "newer"; return plan; }
  if (v.value === build) return plan;
  plan.status = "older";
  const steps = new Map((manifest.migrations ?? []).map((m) => [m.from, m]));
  for (let at = v.value; at < build; at++) {
    const m = steps.get(at);
    if (!m) { plan.missing = at; break; }
    plan.steps.push({ from: at, to: at + 1, name: m.name, description: m.description, changes: (m.changes ?? []).map((c) => describeChange(c, root)) });
  }
  return plan;
}

/** Report lines for the plan; the same text for the terminal and a release page. */
export function renderPlan(plan, source = "config.json") {
  const head = `Migrations: ${source}`;
  if (plan.status === "unreadable") return [`${head}: the file does not parse, so no migration can be planned`];
  if (plan.status === "invalid") return [`${head}: schemaVersion is not a whole number of 1 or more; the server reads it as version 1 and runs no migration`];
  if (plan.status === "newer") return [`${head}: schemaVersion ${plan.fileVersion} is newer than this build's ${plan.build}; refused, no migration can run (update the server, never edit the number down)`];
  if (plan.fileVersion === null) return [`${head}: no schemaVersion, read as version 1; none needed`];
  if (plan.status === "current") return [`${head}: schemaVersion ${plan.fileVersion} is this build's; none needed`];
  const out = [`${head}: schemaVersion ${plan.fileVersion}, this build reads ${plan.build}; ${plan.steps.length} would run${plan.missing === null ? "" : ", then the chain breaks"}, in order:`];
  plan.steps.forEach((s, i) => {
    out.push(`  ${i + 1}. v${s.from} -> v${s.to}  ${s.name}: ${s.description}`);
    for (const c of s.changes) out.push(`       ${c.text}`);
    if (!s.changes.length) out.push("       (no field renamed, dropped or defaulted)");
  });
  if (plan.missing !== null) out.push(`  no migration reads version ${plan.missing}; the server would log an error, leave the file as it is and boot on shipped values`);
  else out.push(`  Dry run: nothing was written. At boot the server first copies the file to config.json.bak-v${plan.fileVersion}-<yyyymmdd-hhmmss UTC>, then writes the migrated file.`);
  return out;
}

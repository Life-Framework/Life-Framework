#!/usr/bin/env node
// tools/codemods/static-reset-registry.mjs - convert `modded class LF_StaticResetHooks` layers to
// registry hook classes (#2034).
//
// Every layer called super.Run, so the chain nested one script frame per feature and overflowed the
// stack at about 123 layers. This rewrites each layer in place into an LF_StaticResetHook subclass that
// registers itself with LF_StaticReset.Register from a static initializer (template and helpers in
// tools/lib/static-reset-hooks.mjs), and rewrites Core/LF_StaticReset.c to the flat registry.
//
//   node tools/codemods/static-reset-registry.mjs [--dry-run] [--root <dir>] [--no-central]
//
// Idempotent: a second run finds no layer and changes nothing. `tools\cli heal-hotspots` runs the same
// conversion on a branch that still adds a layer.

import { existsSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { CENTRAL, centralFile, migrateText, registryHooks, scriptFiles } from "../lib/static-reset-hooks.mjs";

// Convert every layer under root. Returns { files: [{ file, calls }], central: bool, dupes: [names] }.
export function migrateRepo(root, { dryRun = false, central = true } = {}) {
  const files = [];
  const names = new Map();
  for (const rel of scriptFiles(root)) {
    const p = join(root, rel);
    const text = readFileSync(p, "utf8");
    let next = text;
    if (/^modded\s+class\s+LF_StaticResetHooks\b/m.test(text)) {
      const r = migrateText(text);
      next = r.text;
      files.push({ file: rel, calls: r.calls });
      if (!dryRun && next !== text) writeFileSync(p, next);
    }
    for (const h of registryHooks(next)) names.set(h.name, [...(names.get(h.name) || []), rel]);
  }
  let wroteCentral = false;
  const cp = join(root, CENTRAL);
  if (central && existsSync(cp) && readFileSync(cp, "utf8") !== centralFile()) {
    if (!dryRun) writeFileSync(cp, centralFile());
    wroteCentral = true;
  }
  const dupes = [...names].filter(([, fs]) => fs.length > 1).map(([n, fs]) => `${n} (${fs.join(", ")})`);
  return { files, central: wroteCentral, dupes };
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  const args = process.argv.slice(2);
  const at = args.indexOf("--root");
  const root = at >= 0 ? resolve(args[at + 1]) : join(dirname(fileURLToPath(import.meta.url)), "..", "..");
  const dryRun = args.includes("--dry-run");
  const r = migrateRepo(root, { dryRun, central: !args.includes("--no-central") });
  const calls = r.files.reduce((n, f) => n + f.calls.length, 0);
  for (const f of r.files) console.log(`${dryRun ? "would convert" : "converted"} ${f.file}: ${f.calls.map((c) => `${c.cls}.${c.method}`).join(", ")}`);
  if (r.central) console.log(`${dryRun ? "would rewrite" : "rewrote"} ${CENTRAL} to the flat registry`);
  for (const d of r.dupes) console.log(`FAIL duplicate hook class ${d}`);
  console.log(`SUMMARY static-reset-registry ${r.dupes.length ? "fail" : "ok"} files=${r.files.length} hooks=${calls}${dryRun ? " dry-run" : ""}`);
  process.exit(r.dupes.length ? 1 : 0);
}

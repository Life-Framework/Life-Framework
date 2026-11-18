// tools/validation/validate-input-listeners.mjs
// inputs: addons/**/*.c
//
// InputManager is process-global: a listener added with AddActionListener stays until someone
// removes it, and it outlives the menu or screen that added it. The inventory menu added its
// move-quantity listeners on open AND on close (a copy of the open lines), so each open and close
// left four more on a dead menu (docs/reviews/2026-11-17-ui.md).
//
// Fails when a .c calls AddActionListener(action, trigger, callback) and the same file has no
// RemoveActionListener with the same three arguments. There is no allowlist: a once-per-controller
// listener (give up, clip stamp, placement, vehicle lock) is removed in the controller's destructor,
// or a reconnect in the same process leaves a second one on a deleted controller (#2545).
// Comments are ignored; tests are skipped.
// red-proof: tools/validation/input-listeners-selftest.mjs plants an add with no remove, and the
// inventory menu's add-on-close copy.

import { existsSync, readFileSync, readdirSync, statSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const strip = (text) => text.replace(/\/\*[\s\S]*?\*\//g, "").replace(/\/\/.*$/gm, "");
const norm = (s) => s.replace(/\s+/g, "");

// files: [{ path, text }]. Returns the failure lines.
export function problems(files) {
  const out = [];
  for (const f of files) {
    if (f.path.includes("/Tests/")) continue;
    const code = strip(f.text);
    const removes = new Set([...code.matchAll(/RemoveActionListener\(([^;]*)\);/g)].map((m) => norm(m[1])));
    const seen = new Set();
    for (const m of code.matchAll(/AddActionListener\(([^;]*)\);/g)) {
      const args = norm(m[1]);
      if (removes.has(args) || seen.has(args)) continue;
      seen.add(args);
      out.push(`${f.path}: AddActionListener(${args}) has no RemoveActionListener with the same arguments; InputManager keeps it after the menu or screen is gone. Remove it in OnMenuClose / Close, or for a per-controller listener in the controller's destructor`);
    }
  }
  return out;
}

function walk(dir, out) {
  for (const name of readdirSync(dir)) {
    const p = join(dir, name);
    if (statSync(p).isDirectory()) walk(p, out);
    else if (name.endsWith(".c")) out.push(p);
  }
  return out;
}

export function scan(root) {
  const out = [];
  const addons = join(root, "addons");
  if (!existsSync(addons)) return out;
  for (const addon of readdirSync(addons)) {
    const dir = join(addons, addon, "Scripts");
    if (!existsSync(dir)) continue;
    for (const p of walk(dir, [])) out.push({ path: p.slice(root.length + 1).replaceAll("\\", "/"), text: readFileSync(p, "utf8") });
  }
  return out;
}

const isMain = process.argv[1] && fileURLToPath(import.meta.url) === process.argv[1];
if (isMain) {
  const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
  const files = scan(root);
  const bad = problems(files);
  if (bad.length) {
    console.log("validate-input-listeners: FAIL");
    for (const b of bad) console.log(`  ${b}`);
    process.exit(1);
  }
  console.log(`validate-input-listeners: OK (${files.length} scripts)`);
}

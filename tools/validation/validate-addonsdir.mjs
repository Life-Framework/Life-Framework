// One `-addonsDir` per launch: Workbench keeps only the last repeated flag, so
// the pack addon cannot find the core (docs/architecture/pack-move-spike.md).
// Pass the folders through tools/lib/addons.mjs addonsDirArgs() (comma-joined).
// red-proof: restoring the two separate "-addonsDir" entries in cmdPack printed
// `tools/cli.mjs:<line>: repeated -addonsDir` and exited 1.
import { readdirSync, readFileSync, statSync } from "node:fs";
import { join } from "node:path";
import { repeatedAddonsDirLines, selftestAddons } from "../lib/addons.mjs";

const failures = selftestAddons().map((m) => `selftest: ${m}`);
const root = process.cwd();
function walk(dir, out = []) {
  for (const e of readdirSync(dir, { withFileTypes: true })) {
    if (e.name === "node_modules" || e.name === ".git" || e.name === "tmp") continue;
    const p = join(dir, e.name);
    if (e.isDirectory()) walk(p, out);
    else if (/\.(mjs|js)$/.test(e.name) && !p.endsWith("addons.mjs")) out.push(p);
  }
  return out;
}
for (const base of ["tools", "agent-box"]) {
  let files = [];
  try { files = walk(join(root, base)); } catch {}
  for (const f of files) {
    if (statSync(f).size > 2e6) continue;
    for (const n of repeatedAddonsDirLines(readFileSync(f, "utf8"))) failures.push(`${f.slice(root.length + 1).split("\\").join("/")}:${n}: repeated -addonsDir (use addonsDirArgs)`);
  }
}
if (failures.length) {
  console.log("validate-addonsdir: FAIL");
  for (const m of failures) console.log(`  ${m}`);
  process.exit(1);
}
console.log("validate-addonsdir: OK");

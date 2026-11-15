// The shared git config holds at most one merge-drivers include, and it is the main checkout's.
// inputs: always
//
// Worktrees share .git/config; the pre-commit hook once appended one include.path per worktree
// (640 lines, 79 KB, "could not write config file" races). tools/merge/register-drivers.mjs keeps
// exactly one. This fails on a second line or on a line that is not the main checkout's file, and
// prints the one-line fix. It heals first (register-drivers.mjs), so it only fails when the config
// cannot be written. A repo without the include (CI, a fresh clone) passes.
//
// red-proof (2026-11-14): with the heal call removed and the shared .git/config still holding three
// worktree lines from old hooks this printed `FAIL .git/config has 3 merge-drivers include.path lines` and
// exited 1.
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { driversIncludes, mainCheckout, registerDrivers } from "../merge/register-drivers.mjs";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
// A worktree still on the old hook re-adds its own line on every commit; heal first so that never blocks a ship.
const healed = registerDrivers(root);
if (healed.removed) console.log(`validate-drivers-include: healed ${healed.removed} stale include.path line(s)`);
const lines = driversIncludes(root);
const main = mainCheckout(root);
const canonical = main ? `${main}/tools/merge/drivers.gitconfig` : null;
const fix = "fix: node tools/merge/register-drivers.mjs (the pre-commit hook runs it too)";

if (lines.length > 1) {
  console.log(`FAIL .git/config has ${lines.length} merge-drivers include.path lines, expected 1 (${canonical}). ${fix}`);
  for (const l of lines) console.log(`  ${l}`);
  process.exit(1);
}
if (lines.length === 1 && canonical && lines[0] !== canonical) {
  console.log(`FAIL .git/config merge-drivers include.path is ${lines[0]}, expected ${canonical}. ${fix}`);
  process.exit(1);
}
console.log(`validate-drivers-include: OK (${lines.length} include)`);

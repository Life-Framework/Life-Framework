// How a selection of test names reaches tools/test/test-e2e.ps1 (#4842).
// A short list rides the command line as `-Only a,b` (what `--only` always did). A long one (the core leg of an
// unsharded `--tier all` names 1367+ classes, about 40 KB) would pass the 32,767-char Windows command-line limit
// and spawn fails with ENAMETOOLONG before any test starts, so it travels as a file: `-OnlyList <path>`, one
// line of comma-separated names, which the harness reads into the same $profile:LF_TestOnly.txt it always wrote.
import { mkdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";

// Well under the 32,767 limit, which also counts the harness path, the other flags and the quoting powershell.exe adds.
export const MAX_INLINE_ONLY = 4000;

// The harness arguments that carry `only` (an array of names). Empty when nothing is selected.
// root: the worktree the run belongs to; the list file lands in its tmp/ folder (gitignored, per-process name).
export function onlyArgs(root, only) {
  if (!only || !only.length) return [];
  const joined = only.join(",");
  if (joined.length <= MAX_INLINE_ONLY) return ["-Only", joined];
  const dir = join(root, "tmp", "test-only");
  mkdirSync(dir, { recursive: true });
  const file = join(dir, `only-${process.pid}-${Date.now()}.txt`);
  writeFileSync(file, joined, "utf8");
  return ["-OnlyList", file];
}

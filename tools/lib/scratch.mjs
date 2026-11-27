// Scratch folders for node selftests that cannot leak. `scratchDir(prefix)` is mkdtempSync(join(tmpdir(), prefix))
// plus a process 'exit' hook that removes the folder, so it is gone however the selftest ends: a failed check, a
// throw, process.exit(1). Before this, selftests that created a folder and then failed or exited early left it in
// the OS temp dir; on lf-testbox about 400,000 `lf-*` folders (18 GB) piled up in a week (#2589).
// Removal never follows links (safeRemoveTree) and a folder that is already gone, or a holder that will not let go,
// is not an error here: the exit hook must never change the selftest's exit code.
import { mkdtempSync } from "node:fs";
import { tmpdir } from "node:os";
import { join, resolve, sep } from "node:path";
import { safeRemoveTree } from "../link-safety.mjs";

const live = new Set();
let hooked = false;

function removeAll() {
  for (const dir of live) {
    // A selftest that chdir'ed into its scratch folder would hold it open on Windows.
    try {
      const cwd = resolve(process.cwd());
      if (cwd === dir || cwd.startsWith(dir + sep)) process.chdir(tmpdir());
    } catch {}
    try {
      safeRemoveTree(dir);
    } catch {}
  }
  live.clear();
}

// A new empty folder under the OS temp dir; removed when this process exits (or sooner, by `releaseScratch`).
export function scratchDir(prefix = "lf-scratch-") {
  const dir = mkdtempSync(join(tmpdir(), prefix));
  live.add(resolve(dir));
  if (!hooked) {
    hooked = true;
    process.on("exit", removeAll);
  }
  return dir;
}

// Removes one folder now (a selftest that wants it gone before the next case). Same safety as the exit hook.
export function releaseScratch(dir) {
  const abs = resolve(dir);
  live.delete(abs);
  try {
    safeRemoveTree(abs);
    return true;
  } catch {
    return false;
  }
}

// Usage: node tools/safe-rm.mjs <dir> [<dir> ...]
// Deletes each directory without following junctions or symlinks. Use it from
// PowerShell or shell scripts instead of Remove-Item -Recurse / rm -rf on any
// tree that may hold server/profile game-addons links.
import { safeRemoveTree } from "./link-safety.mjs";

const targets = process.argv.slice(2);
if (!targets.length) {
  console.log("usage: node tools/safe-rm.mjs <dir> [<dir> ...]");
  process.exit(2);
}
let failed = 0;
for (const target of targets) {
  try {
    safeRemoveTree(target);
  } catch (e) {
    console.log(`safe-rm: FAIL ${target}: ${e.message}`);
    failed = 1;
  }
}
process.exit(failed);

// tools/validation/town-stamp-selftest.mjs
// inputs: tools/town/**, tools/validation/town-stamp-selftest.mjs
//
// Runs tools/town/stamp.selftest.mjs and tools/town/author.selftest.mjs (card #3881) so `tools\cli validate` covers the grid-to-island
// stamp path on tools/town/fixtures/stamp/: stamping is idempotent (byte equal), a moved kit row moves in both stamped copies, a
// building of another prefab is refused, and a floating or blocks-door finding refuses the stamp and restores the previous bytes.
// red-proof: see the two selftests' headers (2026-11-21).
import { spawnSync } from "node:child_process";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
let failed = 0;
for (const f of ["stamp.selftest.mjs", "author.selftest.mjs"]) {
  const r = spawnSync(process.execPath, [join(ROOT, "tools", "town", f)], { cwd: ROOT, encoding: "utf8" });
  process.stdout.write(r.stdout ?? "");
  process.stderr.write(r.stderr ?? "");
  if (r.status !== 0) failed++;
}
console.log(`SUMMARY town-stamp-selftest ${failed ? "fail" : "ok"} ${failed}`);
process.exit(failed ? 1 : 0);

// Warns (never fails) when this checkout's gitignored resourceDatabase.rdb does
// not register every .meta in the addon: the server would refuse those
// resources with `Wrong GUID/name`. In a worktree, test/dev/serve/session/e2e
// refresh it on their own (`tools\cli rdb` does it now). Fails only when the
// detector's selftest fails.
// red-proof: with missingFrom() matching paths case-sensitively the selftest
// printed "folder casing (Data/ vs data/) counted missing" and exited 1.
import { join } from "node:path";
import { describeMissing, rdbFreshness, selftestRdbFresh } from "../lib/rdb-fresh.mjs";

const failures = selftestRdbFresh();
if (failures.length) {
  console.log("validate-rdb-fresh: FAIL (selftest)");
  for (const f of failures) console.log(`  ${f}`);
  process.exit(1);
}

const addon = join(process.cwd(), "addons", "LifeFramework");
const r = rdbFreshness(addon);
if (!r.exists) {
  console.log("validate-rdb-fresh: OK (selftest; no resourceDatabase.rdb in this checkout, e.g. CI)");
} else if (r.missing.length) {
  console.log(`WARN  resource database stale: ${r.missing.length} unregistered resource(s): ${describeMissing(r.missing)}`);
  console.log("      a world test would see `Wrong GUID/name` for these; run `tools\\cli rdb` in a worktree (test/dev refresh it anyway),");
  console.log("      or open Workbench once on the main checkout");
} else {
  console.log(`validate-rdb-fresh: OK (${r.total} resources registered)`);
}

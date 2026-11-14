// Warns (never fails) when this checkout's gitignored resourceDatabase.rdb does
// inputs: always
// not register every .meta in the addon: the server would refuse those
// resources with `Wrong GUID/name`. In a worktree, test/dev/serve/session/e2e
// refresh it on their own (`tools\\cli rdb` does it now). Fails only when the
// detector's selftest fails. Covers the core and any other addon that declares
// resources (tools/lib/addons.mjs rdbAddons).
// red-proof: with missingFrom() matching paths case-sensitively the selftest
// printed "folder casing (Data/ vs data/) counted missing" and exited 1.
import { describeMissing, rdbFreshness, selftestRdbFresh } from "../lib/rdb-fresh.mjs";
import { addonDir, rdbAddons, selftestAddons } from "../lib/addons.mjs";

const failures = [...selftestRdbFresh(), ...selftestAddons()];
if (failures.length) {
  console.log("validate-rdb-fresh: FAIL (selftest)");
  for (const f of failures) console.log(`  ${f}`);
  process.exit(1);
}

const names = rdbAddons(process.cwd());
let stale = false;
let none = 0;
const ok = [];
for (const name of names) {
  const r = rdbFreshness(addonDir(process.cwd(), name));
  const tag = names.length > 1 ? ` [${name}]` : "";
  if (!r.exists) none++;
  else if (r.missing.length) {
    stale = true;
    console.log(`WARN  resource database stale${tag}: ${r.missing.length} unregistered resource(s): ${describeMissing(r.missing)}`);
  } else ok.push(`${r.total} resources registered${tag}`);
}
if (none === names.length) {
  console.log("validate-rdb-fresh: OK (selftest; no resourceDatabase.rdb in this checkout, e.g. CI)");
} else if (stale) {
  console.log("      a world test would see `Wrong GUID/name` for these; run `tools\\cli rdb` in a worktree (test/dev refresh it anyway),");
  console.log("      or open Workbench once on the main checkout");
} else {
  console.log(`validate-rdb-fresh: OK (${ok.join("; ")})`);
}

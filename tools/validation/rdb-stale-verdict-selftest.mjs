// tools/validation/rdb-stale-verdict-selftest.mjs
// inputs: tools/lib/rdb-stale-verdict.mjs, tools/cli.mjs
//
// Proves a test run on a stale resourceDatabase.rdb never passes silently (tools/lib/rdb-stale-verdict.mjs):
// a whole-suite pass fails with the reason, a fast or --only run warns, LF_ALLOW_STALE_RDB=1 downgrades the
// failure, and cli.mjs wires the verdict into the local run and keeps such a run out of the pass cache.
//
// red-proof: 2026-11-17, with staleRdbVerdict returning { fail: false, warn: false, line: "" } for every
// input (the old silent behaviour), the run printed `FAIL whole-suite pass on a stale rdb must fail`,
// `FAIL --only run on a stale rdb must warn`, `FAIL fast run on a stale rdb must warn` (and the two
// override/already-failing cases) and exited 1; restored, it passes.
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { staleRdbVerdict } from "../lib/rdb-stale-verdict.mjs";

const failures = [];
const check = (ok, msg) => {
  if (!ok) failures.push(msg);
};

const env = {};
const fresh = staleRdbVerdict({ stale: [], code: 0, tier: "all", only: [], env });
check(!fresh.fail && !fresh.warn && fresh.line === "", "a fresh rdb must change nothing");

const whole = staleRdbVerdict({ stale: ["LF_Everon1989"], code: 0, tier: "all", only: [], env });
check(whole.fail && /SUMMARY test fail rdb-stale=LF_Everon1989/.test(whole.line), "whole-suite pass on a stale rdb must fail");

const only = staleRdbVerdict({ stale: ["LifeFramework"], code: 0, tier: "all", only: ["LF_Test_X"], env });
check(!only.fail && only.warn && /SUMMARY test warn rdb-stale=LifeFramework/.test(only.line), "--only run on a stale rdb must warn");

const fast = staleRdbVerdict({ stale: ["LifeFramework"], code: 0, tier: "fast", only: [], env });
check(!fast.fail && fast.warn, "fast run on a stale rdb must warn");

const red = staleRdbVerdict({ stale: ["LifeFramework"], code: 1, tier: "all", only: [], env });
check(!red.fail && red.warn, "an already-failing run keeps its own failure and gets the warning");

const allowed = staleRdbVerdict({ stale: ["LifeFramework"], code: 0, tier: "all", only: [], env: { LF_ALLOW_STALE_RDB: "1" } });
check(!allowed.fail && allowed.warn, "LF_ALLOW_STALE_RDB=1 must downgrade the failure to a warning");

// Wiring: the local run asks for the verdict and a stale run is never cached.
const cli = readFileSync(join(dirname(fileURLToPath(import.meta.url)), "..", "cli.mjs"), "utf8");
check(/staleRdbVerdict\(\{ stale: testbox\.staleRdbAddons\(root\)/.test(cli), "cli.mjs local test run must call staleRdbVerdict with the stale addons");
check(/r\.code === 0 && !r\.rdbStale\) passCacheStore/.test(cli), "cli.mjs must not store a stale-rdb run in the pass cache");

if (failures.length) {
  for (const f of failures) console.log(`FAIL ${f}`);
  process.exit(1);
}
console.log("rdb-stale-verdict-selftest: OK (whole suite fails, fast/--only warn, override, wiring, no cache)");

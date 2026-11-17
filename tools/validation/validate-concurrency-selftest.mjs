// validate-concurrency-selftest: validate's fan-out shrinks when free RAM is short (tools/lib/validate-concurrency.mjs).
// inputs: tools/lib/validate-concurrency.mjs, tools/cli.mjs, tools/validation/validate-concurrency-selftest.mjs
//
// red-proof 2026-11-17: with areaConcurrency returning the old max(8, min(12, cpus)) the "short of RAM" checks fail.

import { readFileSync } from "node:fs";
import { areaConcurrency, MIN } from "../lib/validate-concurrency.mjs";

const failures = [];
const check = (ok, msg) => {
  if (!ok) failures.push(msg);
};

check(areaConcurrency(16, 8000) === 12 && areaConcurrency(4, 8000) === 8 && areaConcurrency(10, 8000) === 10, "with room: 8-12 by CPU count, as before");
check(areaConcurrency(16, undefined) === 12, "unknown free RAM: as before");
check(areaConcurrency(16, 2300) === 10, `2.3 GB free: as many as fit over the 1.5 GB floor at 80 MB each (got ${areaConcurrency(16, 2300)})`);
check(areaConcurrency(16, 1900) === 5, `1.9 GB free: 5 (got ${areaConcurrency(16, 1900)})`);
check(areaConcurrency(16, 900) === MIN && MIN >= 1, "under the floor: never fewer than MIN");
check(/AREA_CONCURRENCY = areaConcurrency\(/.test(readFileSync(new URL("../cli.mjs", import.meta.url), "utf8")), "validate in tools/cli.mjs uses areaConcurrency");

if (failures.length) {
  console.log("validate-concurrency-selftest: FAIL");
  for (const f of failures) console.log(`  ${f}`);
  process.exit(1);
}
console.log("validate-concurrency-selftest: OK (fan-out as before with room, shrinks to fit over the floor, never under MIN)");

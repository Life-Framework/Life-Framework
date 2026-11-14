// Proves validate-prefab-compare fails on planted path compares and passes on the
// GUID-safe and marked forms. Fixtures: tools/validation/fixtures/prefab-compare/{bad,good}.
//
// red-proof: with the pattern list emptied the bad fixture printed
// "bad: expected 3 path compares, got 0" and exited 1; restored, it prints
// "prefab-compare-selftest: OK".
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { scanPrefabCompares, compare, toBaseline } from "./validate-prefab-compare.mjs";

const here = dirname(fileURLToPath(import.meta.url));
const fx = (n) => join(here, "fixtures", "prefab-compare", n);
const fails = [];
const expect = (ok, msg) => { if (!ok) fails.push(msg); };

const bad = scanPrefabCompares(fx("bad"));
expect(bad.length === 3, `bad: expected 3 path compares, got ${bad.length}`);
expect(compare(bad, []).fresh.length === 3, "bad: all three must be new against an empty baseline");

const good = scanPrefabCompares(fx("good"));
expect(good.length === 0, `good: expected no path compares, got ${good.map((h) => h.text)}`);

const base = toBaseline(bad);
expect(compare(bad, base).fresh.length === 0, "ratchet: baselined lines must pass");
expect(compare(bad, base.slice(1)).fresh.length === 1, "ratchet: a line missing from the baseline must fail");
expect(compare(bad.slice(1), base).stale.length === 1, "ratchet: a fixed line must show as stale");

if (fails.length) {
  for (const f of fails) console.log(`FAIL ${f}`);
  process.exit(1);
}
console.log("prefab-compare-selftest: OK (planted compares red, SamePrefab and marked forms green, ratchet holds)");

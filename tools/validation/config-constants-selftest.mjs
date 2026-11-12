// Proves validate-config-constants fails on a planted tunable and passes on the
// exempt forms (lf-fixed mark, config-class default, structural name, baselined).
// Fixtures: tools/validation/fixtures/config-constants/{bad,good}/<Area>/*.c.
//
// red-proof: with TUNABLE_TOKENS emptied the bad fixture printed
// "bad: expected 3 new tunables, got 0" and exited 1; restored, it prints
// "config-constants-selftest: OK".
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { scanConstants } from "../config/inventory.mjs";
import { compare, toBaseline } from "./validate-config-constants.mjs";

const here = dirname(fileURLToPath(import.meta.url));
const fx = (n) => join(here, "fixtures", "config-constants", n);
const fails = [];
const expect = (ok, msg) => { if (!ok) fails.push(msg); };

const bad = scanConstants(fx("bad"));
const rBad = compare(bad, []);
expect(rBad.fresh.length === 3, `bad: expected 3 new tunables, got ${rBad.fresh.length}`);

const good = scanConstants(fx("good"));
expect(good.length >= 5, `good: expected the fixture to declare 5+ constants, saw ${good.length}`);
expect(compare(good, []).fresh.length === 0, `good: expected no new tunables, got ${compare(good, []).fresh.map((c) => c.name)}`);

// Ratchet: baselining the bad fixture passes it; one extra constant fails; a removed one is stale.
const base = toBaseline(bad);
expect(compare(bad, base).fresh.length === 0, "ratchet: baselined constants must pass");
expect(compare(bad, base.slice(1)).fresh.length === 1, "ratchet: a constant missing from the baseline must fail");
expect(compare(bad.slice(1), base).stale.length === 1, "ratchet: a fixed constant must show as stale");

if (fails.length) {
  for (const f of fails) console.log(`FAIL ${f}`);
  process.exit(1);
}
console.log("config-constants-selftest: OK (planted tunables red, exempt forms green, ratchet holds)");

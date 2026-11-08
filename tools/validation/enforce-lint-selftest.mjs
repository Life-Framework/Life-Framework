// Proves every enforce-lint rule fires on its known-bad fixture, stays quiet on
// its known-good fixture, and that the baseline ratchet fails only new hits.
// Fixtures: tools/validation/fixtures/enforce-lint/<rule>.{bad,good}.c. Each runs
// in a throwaway addon tree (the L6 .meta lives there, not in the repo, so the
// repo GUID checks never see fixture GUIDs).
//
// red-proof: with the L1 spawn regex changed to never match, this printed
// "L1.bad.c: expected 1 L1 finding(s), got 0" and exited 1; with the L4 cleanup
// check removed it printed "L4.good.c: expected no findings, got L4@3"; restored,
// it prints "enforce-lint-selftest: OK (8 rules red on bad, green on good, ratchet holds)".
import { mkdtempSync, mkdirSync, copyFileSync, writeFileSync, readdirSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { lint, compare, toBaseline, RULES } from "../enforce-lint.mjs";
import { safeRemoveTree } from "../link-safety.mjs";

const here = dirname(fileURLToPath(import.meta.url));
const fixtures = join(here, "fixtures", "enforce-lint");

// Findings each bad fixture must produce for its own rule.
const EXPECT = { L1: 1, L2: 7, L3: 2, L4: 1, L5: 1, L6: 3, L7: 1, L8: 1 };

function runFixture(name) {
  const tmp = mkdtempSync(join(tmpdir(), "lf-enforce-lint-"));
  try {
    const addon = join(tmp, "LifeFramework");
    const scripts = join(addon, "Scripts", "Game");
    mkdirSync(scripts, { recursive: true });
    mkdirSync(join(addon, "Prefabs"), { recursive: true });
    writeFileSync(join(addon, "Prefabs", "LF_FixtureProp.et"), "GenericEntity {\n}\n");
    writeFileSync(
      join(addon, "Prefabs", "LF_FixtureProp.et.meta"),
      'MetaFileClass {\n Name "{2222222222222222}Prefabs/LF_FixtureProp.et"\n}\n',
    );
    copyFileSync(join(fixtures, name), join(scripts, name));
    return lint({ addonDir: addon, scriptsDir: scripts });
  } finally {
    safeRemoveTree(tmp);
  }
}

const failures = [];
const verbose = process.argv.includes("--verbose");
const rules = Object.keys(RULES);
const present = new Set(readdirSync(fixtures));

for (const rule of rules) {
  for (const kind of ["bad", "good"]) {
    const name = `${rule}.${kind}.c`;
    if (!present.has(name)) {
      failures.push(`${name}: fixture missing`);
      continue;
    }
    const found = runFixture(name);
    if (verbose) for (const f of found) console.log(`  ${name}: ${f.rule}@${f.line} ${f.message}`);
    if (kind === "bad") {
      const own = found.filter((f) => f.rule === rule);
      if (own.length !== EXPECT[rule]) {
        failures.push(`${name}: expected ${EXPECT[rule]} ${rule} finding(s), got ${own.length}`);
        for (const f of own) failures.push(`    ${f.rule}@${f.line} ${f.message}`);
      }
    } else if (found.length) {
      failures.push(`${name}: expected no findings, got ${found.map((f) => `${f.rule}@${f.line}`).join(", ")}`);
    }
  }
}

// Ratchet: a baseline of the current hits passes; one extra hit fails; a fixed
// hit is counted as stale.
const bad = runFixture("L2.bad.c");
const base = toBaseline(bad);
const same = compare(bad, base);
if (same.fresh.length !== 0) failures.push(`ratchet: baseline of its own hits left ${same.fresh.length} new`);
const grown = compare([...bad, { ...bad[0] }], base);
if (grown.fresh.length !== 1) failures.push(`ratchet: a duplicated hit should be 1 new, got ${grown.fresh.length}`);
const shrunk = compare(bad.slice(1), base);
if (shrunk.fresh.length !== 0 || shrunk.stale !== 1) failures.push(`ratchet: a fixed hit should be 0 new / 1 stale, got ${shrunk.fresh.length} / ${shrunk.stale}`);

if (failures.length) {
  console.log("enforce-lint-selftest: FAIL");
  for (const f of failures) console.log(`  ${f}`);
  process.exit(1);
}
console.log(`enforce-lint-selftest: OK (${rules.length} rules red on bad, green on good, ratchet holds)`);

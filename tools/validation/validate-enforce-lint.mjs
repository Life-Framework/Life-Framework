// Ratcheting structural lint over addons/LifeFramework (rules in tools/enforce-lint.mjs).
// inputs: addons/**, tools/enforce-lint.mjs, tools/link-safety.mjs
//
// Every finding prints as `WARN <rule> <file>:<line> <snippet>`. Findings listed
// in enforce-lint-baseline.json are known debt and pass; a finding not in the
// baseline fails the run. Fixing debt leaves stale baseline entries, which are
// reported so the next `--update-baseline` shrinks the file.
//
//   node tools/validation/validate-enforce-lint.mjs                   check
//   node tools/validation/validate-enforce-lint.mjs --update-baseline rewrite the baseline
//   node tools/validation/validate-enforce-lint.mjs --quiet           only new findings
//
// red-proof: see enforce-lint-selftest.mjs.
import { readFileSync, writeFileSync, existsSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { lint, compare, toBaseline, RULES } from "../enforce-lint.mjs";

const here = dirname(fileURLToPath(import.meta.url));
const root = join(here, "..", "..");
const addonDir = join(root, "addons", "LifeFramework");
const scriptsDir = join(addonDir, "Scripts", "Game");
const baselinePath = join(here, "enforce-lint-baseline.json");
const args = new Set(process.argv.slice(2));

const findings = lint({ addonDir, scriptsDir });

if (args.has("--update-baseline")) {
  writeFileSync(baselinePath, JSON.stringify(toBaseline(findings), null, 1) + "\n");
  console.log(`enforce-lint: baseline rewritten with ${findings.length} finding(s)`);
  process.exit(0);
}

const baseline = existsSync(baselinePath) ? JSON.parse(readFileSync(baselinePath, "utf8")) : [];
const { fresh, stale } = compare(findings, baseline);
const freshSet = new Set(fresh);

const counts = {};
for (const f of findings) counts[f.rule] = (counts[f.rule] ?? 0) + 1;

if (!args.has("--quiet")) {
  for (const f of findings) {
    if (freshSet.has(f)) continue;
    console.log(`WARN ${f.rule} ${f.file}:${f.line} ${f.text.slice(0, 140)}`);
  }
}
for (const f of fresh) {
  console.log(`FAIL ${f.rule} ${f.file}:${f.line} ${f.text.slice(0, 140)}`);
  console.log(`     ${RULES[f.rule]}: ${f.message}`);
}

const summary = Object.keys(RULES).map((r) => `${r}=${counts[r] ?? 0}`).join(" ");
console.log(`enforce-lint: ${findings.length} finding(s) [${summary}], ${fresh.length} new, ${stale} fixed since baseline`);
if (stale > 0) console.log("enforce-lint: baseline has fixed entries; shrink it with --update-baseline");
if (fresh.length > 0) {
  console.log("enforce-lint: new findings fail. Fix them (preferred), or if one is a false positive, say why in the PR and run --update-baseline.");
  process.exit(1);
}

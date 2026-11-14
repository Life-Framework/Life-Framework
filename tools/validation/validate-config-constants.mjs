// Rejects new hard-coded tunables in Feature/ (#277). A numeric constant whose
// inputs: addons/**, docs/config-inventory.md, tools/config/**
// name is a time, range, price, cap or rate belongs on a config class
// (tools/config/inventory.mjs holds the rules and docs/config-inventory.md the table).
//
// A tunable passes when it is
//   - a DEFAULT_* in a *Config.c file (it is the config class's own default),
//   - marked `// lf-fixed: <why>` on its line or the line above, or
//   - listed in config-constants-baseline.json (existing debt, burned down by #284).
//
//   node tools/validation/validate-config-constants.mjs                   check
//   node tools/validation/validate-config-constants.mjs --update-baseline rewrite the baseline
//
// red-proof: see config-constants-selftest.mjs.
import { readFileSync, writeFileSync, existsSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { scanConstants, needsEntry, constKey } from "../config/inventory.mjs";

const here = dirname(fileURLToPath(import.meta.url));
export const baselinePath = join(here, "config-constants-baseline.json");

/** Returns { fresh, stale } for found constants against a baseline key list. */
export function compare(consts, baseline) {
  const held = consts.filter(needsEntry);
  const known = new Set(baseline);
  const present = new Set(held.map(constKey));
  return {
    fresh: held.filter((c) => !known.has(constKey(c))),
    stale: baseline.filter((k) => !present.has(k)),
  };
}

export const toBaseline = (consts) => [...new Set(consts.filter(needsEntry).map(constKey))].sort();

if (process.argv[1] && process.argv[1].endsWith("validate-config-constants.mjs")) {
  const consts = scanConstants();
  if (process.argv.includes("--update-baseline")) {
    const b = toBaseline(consts);
    writeFileSync(baselinePath, JSON.stringify(b, null, 1) + "\n");
    console.log(`config-constants: baseline rewritten with ${b.length} constant(s)`);
    process.exit(0);
  }
  const baseline = existsSync(baselinePath) ? JSON.parse(readFileSync(baselinePath, "utf8")) : [];
  const { fresh, stale } = compare(consts, baseline);
  for (const c of fresh) {
    console.log(`FAIL ${c.file}:${c.line} ${c.type} ${c.name} = ${c.value}`);
  }
  console.log(`config-constants: ${consts.filter(needsEntry).length} baselined tunable(s), ${fresh.length} new, ${stale.length} gone since baseline`);
  if (stale.length > 0) console.log("config-constants: baseline has moved constants; shrink it with --update-baseline");
  if (fresh.length > 0) {
    console.log("config-constants: a new tunable (time, range, price, cap, rate) belongs on a config class with this value as its default.");
    console.log("  If it must stay in script, mark it `// lf-fixed: <why>`. See docs/config-inventory.md.");
    process.exit(1);
  }
}

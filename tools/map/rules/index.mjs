// tools/map/rules/index.mjs - the rule loader and the case-table format.
//
// A map rule (visibility, cluster, areas, discovery) is a pure function in tools/map/rules/<rule>.mjs,
// written by the card that writes its Enforce twin (M2, M4, M5, M13). This file never defines a rule: it
// imports whichever of those modules exist, so the emulator draws with them as they land.
//
// Each rule has ONE shared case table, addons/LifeFramework/Configs/Map/Rules/<rule>.cases.json:
//   { "rule": "visibility", "version": 1, "cases": [ { "name": "...", "input": { ... }, "expect": <json> } ] }
// The node module exports `evaluate(input)` returning a JSON-comparable value; the node selftest runs
// `runCases(rule, fn)`, and the rule's LOGIC test reads the same file and runs the Enforce twin, so the two
// implementations cannot drift: a mismatch fails one of them.
import { existsSync, readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

export const RULE_NAMES = ["visibility", "cluster", "areas", "discovery"];
const HERE = dirname(fileURLToPath(import.meta.url));
export const CASES_DIR = resolve(HERE, "..", "..", "..", "addons", "LifeFramework", "Configs", "Map", "Rules");

// { visibility: <module>, ... } for the rule files that exist.
export async function loadRules(dir = HERE) {
  const out = {};
  for (const name of RULE_NAMES) {
    const f = join(dir, `${name}.mjs`);
    if (!existsSync(f)) continue;
    try {
      out[name] = await import(pathToFileURL(f).href);
    } catch (e) {
      console.error(`map rules: ${name}.mjs failed to load: ${e.message}`);
    }
  }
  return out;
}

export function loadCases(rule, { dir = CASES_DIR } = {}) {
  const f = join(dir, `${rule}.cases.json`);
  if (!existsSync(f)) return null;
  const t = JSON.parse(readFileSync(f, "utf8"));
  if (t.rule !== rule || !Array.isArray(t.cases)) throw new Error(`${f}: expected { rule: "${rule}", cases: [...] }`);
  return t;
}

const same = (a, b) => JSON.stringify(a) === JSON.stringify(b);

// Runs a rule function over its case table. Returns { pass, fail, failures: [{ name, expect, got }] }.
export function runCases(rule, fn, { dir = CASES_DIR } = {}) {
  const t = loadCases(rule, { dir });
  if (!t) return { pass: 0, fail: 0, failures: [], missing: true };
  const failures = [];
  let pass = 0;
  for (const c of t.cases) {
    let got;
    try {
      got = fn(c.input);
    } catch (e) {
      got = `threw: ${e.message}`;
    }
    if (same(got, c.expect)) pass++;
    else failures.push({ name: c.name, expect: c.expect, got });
  }
  return { pass, fail: failures.length, failures };
}

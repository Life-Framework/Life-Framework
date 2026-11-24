// tools/validation/level-set-harness-selftest.mjs
// inputs: addons/LifeFramework/Scripts/Game/Feature/Level/LF_LevelSetHarness.c
//
// Proves the `levelset` harness verb (#4773) exists only in a LF_HARNESS build: the verb handler and its
// server RPC sit inside `#ifdef LF_HARNESS ... #endif`, the statics outside it refuse without a harness
// build, and the checker goes red when the guard is removed.
// red-proof: with the `#ifdef LF_HARNESS` line deleted from LF_LevelSetHarness.c this printed
// "FAIL the verb handler is outside #ifdef LF_HARNESS" and exited 1 (2026-11-24); restored, it prints OK.

import { readFileSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const FILE = "addons/LifeFramework/Scripts/Game/Feature/Level/LF_LevelSetHarness.c";

// Returns the list of failures for one source text.
export function check(text) {
  const fails = [];
  const lines = text.split("\n");
  let depth = 0;
  const guarded = new Set();
  lines.forEach((line, i) => {
    const t = line.trim();
    if (t === "#ifdef LF_HARNESS") depth++;
    else if (t.startsWith("#if")) depth += 0; // any other conditional is not the guard
    else if (t === "#endif" && depth > 0) depth--;
    else if (depth > 0) guarded.add(i);
  });
  const lineOf = (re) => lines.findIndex((l) => re.test(l));
  for (const [what, re] of [
    ["the verb handler", /cmd\.Verb != "levelset"/],
    ["the server RPC", /RpcAsk_LF_LevelSet\(string job/],
    ["the client ask", /void LF_AskLevelSet\(/],
  ]) {
    const at = lineOf(re);
    if (at < 0) fails.push(`${what} is missing`);
    else if (!guarded.has(at)) fails.push(`${what} is outside #ifdef LF_HARNESS`);
  }
  for (const [what, re] of [
    ["HarnessSetLevel", /static int HarnessSetLevel\(/],
    ["HarnessSetXp", /static int HarnessSetXp\(/],
  ]) {
    const at = lineOf(re);
    if (at < 0) { fails.push(`${what} is missing`); continue; }
    const body = lines.slice(at, at + 8).join("\n");
    if (!/!LF_DevGate\.HarnessBuild\(\)/.test(body)) fails.push(`${what} does not refuse outside a harness build`);
  }
  if (/LF_DevGate\.IsAllowedForPlayer|CheckPlayerRequest/.test(text)) fails.push("the RPC falls back to the admin gate: a harness build only");
  return fails;
}

const text = readFileSync(join(ROOT, FILE), "utf8");
let failed = 0;
const expect = (ok, msg) => { if (!ok) { console.log(`FAIL ${msg}`); failed++; } };

const real = check(text);
expect(real.length === 0, `shipped file: ${real.join("; ")}`);

// Red path: each break must be caught.
expect(check(text.replace("#ifdef LF_HARNESS", "#ifdef LF_SOMETHING_ELSE")).some((f) => /outside #ifdef LF_HARNESS/.test(f)),
  "the verb handler is outside #ifdef LF_HARNESS (guard removed must fail)");
expect(check(text.replace(/static int HarnessSetLevel\([^)]*\)\n\t\{\n\t\tif \(!LF_DevGate\.HarnessBuild\(\)\)/, "static int HarnessSetLevel(string a, string b, int c)\n\t{\n\t\tif (false)"))
  .some((f) => /HarnessSetLevel does not refuse/.test(f)), "a set that skips the harness check must fail");
expect(check(text + "\nLF_DevGate.IsAllowedForPlayer(1);").some((f) => /admin gate/.test(f)), "an admin fallback must fail");

if (failed) {
  console.log(`level-set-harness-selftest: ${failed} check(s) failed`);
  process.exit(1);
}
console.log("level-set-harness-selftest: OK (verb and RPC guarded, statics refuse outside a harness build, 3 breaks caught)");

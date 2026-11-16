// The pre-move e2e loads a save written by the ref build (502ebf39), which predates the folded piles (#1899),
// so the verify test's pile assertions must sit inside `#ifndef LF_E2E_PRE_MOVE` (the main guard went red
// on them at every head until they did).
// inputs: addons/LifeFramework/Scripts/Game/Tests/LF_Test_E2E_SaveState.c
// red-proof: with the `#ifndef LF_E2E_PRE_MOVE` line above the pile block removed this printed "the pile assertion is not behind #ifndef LF_E2E_PRE_MOVE" and exited 1.
import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const file = "addons/LifeFramework/Scripts/Game/Tests/LF_Test_E2E_SaveState.c";
const lines = readFileSync(join(root, file), "utf8").split("\n");

// Assertions about content the ref build never saved: each must be skipped under --pre-move.
const POST_REF = ["piled trousers", "restored copy, not a respawned one", "none doubled", "pile holds three"];

const stack = []; // true = this level excludes the pre-move build
const fails = [];
lines.forEach((line, i) => {
  const t = line.trim();
  if (/^#ifndef\s+LF_E2E_PRE_MOVE\b/.test(t)) stack.push(true);
  else if (/^#if(def|ndef)?\b/.test(t)) stack.push(false);
  else if (/^#else\b/.test(t) && stack.length) stack[stack.length - 1] = false;
  else if (/^#endif\b/.test(t)) stack.pop();
  else if (POST_REF.some((s) => line.includes(s)) && !stack.includes(true))
    fails.push(`${file}:${i + 1} the pile assertion is not behind #ifndef LF_E2E_PRE_MOVE`);
});

if (fails.length) {
  console.log("e2e-pre-move-pile-selftest: FAIL");
  for (const f of fails) console.log("  " + f);
  process.exit(1);
}
console.log("e2e-pre-move-pile-selftest: OK (pile assertions skipped under --pre-move)");

// tools/validation/static-reset-hooks-selftest.mjs - the world-end hooks stay spread over short chains.
// inputs: tools/lib/static-reset-hooks.mjs, tools/validation/validate-registries.mjs, tools/validation/static-reset-hooks-selftest.mjs, addons/**
//
// One `modded class LF_StaticResetHooks` of 122 layers overflowed the script VM stack ("Stack overflow",
// Class LF_StaticResetHooks) at world teardown, and the main guard reverted the PR that happened to add
// the 120th layer. Every layer is one nested super.Run frame, so the fix is short chains: eight sibling
// bucket classes, a feature picking its bucket from its class name. This proves the pieces:
//   1. bucketOf is stable, in range, and spreads real class names (no bucket over MAX_PER_BUCKET);
//   2. the shipped addons stay within the cap in every bucket (the real tree, not a fixture);
//   3. centralFile() declares and runs every bucket, and addHooks writes the bucket bucketOf names;
//   4. validate-registries' cap check (bucketCapProblems) rejects a bucket past the cap.
//
// red-proof: 2026-11-16 the bug itself: main at 742471ab (122 layers in one class) crashed every run with
// "Stack overflow, Class LF_StaticResetHooks" (`test --only LF_Test_StaticResetHooks`, crash.log); the
// same run on the bucketed tree is the green. For this file: MAX_PER_BUCKET raised to 1000 printed
// "FAIL  122 layers in one class is rejected"; BUCKETS = 1 printed "FAIL  bucketOf spreads real class names
// over several buckets".
import { readFileSync, readdirSync, statSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { BUCKETS, MAX_PER_BUCKET, SCRIPTS, addHooks, bucketCapProblems, bucketOf, centralFile, hookBlocks, hookClass, hookedCalls } from "../lib/static-reset-hooks.mjs";

let failed = 0;
const check = (ok, msg) => {
  console.log(`${ok ? "PASS" : "FAIL"}  ${msg}`);
  if (!ok) failed++;
};

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");

// 1. bucketOf
const names = ["LF_Impairment", "LF_Breach", "LF_Places", "LF_Jail", "LF_Payphone", "LF_Session", "LF_WorldReset", "LF_Hotwire"];
check(names.every((n) => bucketOf(n) === bucketOf(n) && bucketOf(n) >= 0 && bucketOf(n) < BUCKETS), "bucketOf is stable and in range");
check(new Set(names.map(bucketOf)).size >= 3, "bucketOf spreads real class names over several buckets");
check(hookClass(0) === "LF_StaticResetHooks" && hookClass(3) === "LF_StaticResetHooks3", "bucket 0 keeps the old class name");

// 2. the real tree
function walk(dir, out = []) {
  for (const n of readdirSync(dir)) {
    const p = join(dir, n);
    if (statSync(p).isDirectory()) walk(p, out);
    else if (n.endsWith(".c")) out.push(p);
  }
  return out;
}
const layers = {};
for (const dir of [join(root, SCRIPTS), join(root, "addons/LF_Everon1989")]) {
  try {
    for (const f of walk(dir)) {
      const text = readFileSync(f, "utf8");
      if (!text.includes("modded class LF_StaticResetHooks")) continue;
      for (const b of hookBlocks(text)) layers[b.cls] = (layers[b.cls] || 0) + 1;
    }
  } catch {
    // a pack folder that is absent is not an error
  }
}
const worst = Object.entries(layers).sort((a, b) => b[1] - a[1])[0] || ["none", 0];
check(worst[1] <= MAX_PER_BUCKET, `no bucket holds more than ${MAX_PER_BUCKET} layers (${worst[0]}: ${worst[1]})`);
check(Object.values(layers).reduce((a, b) => a + b, 0) >= 100, "the tree's hooks were found (a regex that matches nothing would pass step 2)");

// 3. the generated central file and the scaffold's block
const central = centralFile();
let all = true;
for (let b = 0; b < BUCKETS; b++) {
  const cls = hookClass(b);
  if (!new RegExp(`^class ${cls} : Managed`, "m").test(central) || !central.includes(`new ${cls}()`)) all = false;
}
check(all, "centralFile declares and runs every bucket class");
check(!/\bLF_[A-Z][A-Za-z0-9]*\.ResetStatics/.test(central), "centralFile lists no feature");
const added = addHooks("class LF_Zeta\n{\n}\n", [{ cls: "LF_Zeta", method: "ResetStatics" }]);
const blocks = hookBlocks(added);
check(blocks.length === 1 && blocks[0].cls === hookClass(bucketOf("LF_Zeta")), "addHooks writes the bucket bucketOf names");
check(hookedCalls(added).length === 1 && addHooks(added, [{ cls: "LF_Zeta", method: "ResetStatics" }]) === added, "addHooks stays idempotent across buckets");

// 4. the cap check validate-registries runs
check(bucketCapProblems({ LF_StaticResetHooks5: MAX_PER_BUCKET }).length === 0, "a bucket at the cap is accepted");
check(bucketCapProblems({ LF_StaticResetHooks5: MAX_PER_BUCKET + 1 }).length === 1, "a bucket over the cap is rejected (the 122-layer class that overflowed would be)");
check(bucketCapProblems({ LF_StaticResetHooks: 122 }).length === 1, "122 layers in one class is rejected");

if (failed) {
  console.log(`static-reset-hooks-selftest: ${failed} check(s) failed`);
  process.exit(1);
}
console.log("static-reset-hooks-selftest: OK");

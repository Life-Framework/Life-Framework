// No test calls LF_StaticReset.ResetAll() (AGENTS.md "Tests", issue #2093).
// inputs: addons/**/Scripts/Game/Tests/*.c, tools/lib/test-resetall.mjs
//
// ResetAll wipes every feature's statics. Called mid-suite it left 38 later tests without their
// placed-object registries and the main guard blamed an innocent PR (2026-11-16, fixed in #2092).
// A test applies only its own feature's LF_StaticResetHook (`<Owner>.ResetStatics()` or the hook
// class), never the whole registry. Core and pack test folders are both scanned; comments that
// explain why ResetAll is avoided are fine.
//
// LF_RESETALL_ROOT points the scan at another tree (the selftest uses a temp one).
//
// red-proof: 2026-11-16, tools/validation/no-test-resetall-selftest.mjs runs this over a tree
// holding tools/validation/fixtures/no-test-resetall/LF_Test_BadResetAll.c; it fails with
// `FAIL ...LF_Test_BadResetAll.c:12` and passes once the file is gone.
import { existsSync, readdirSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { resetAllLines } from "../lib/test-resetall.mjs";

const root = process.env.LF_RESETALL_ROOT || join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const files = [];
const addons = join(root, "addons");
for (const addon of existsSync(addons) ? readdirSync(addons) : []) {
  const dir = join(addons, addon, "Scripts", "Game", "Tests");
  if (!existsSync(dir)) continue;
  for (const f of readdirSync(dir)) if (f.endsWith(".c")) files.push(`addons/${addon}/Scripts/Game/Tests/${f}`);
}

const bad = [];
for (const f of files) {
  for (const line of resetAllLines(readFileSync(join(root, f), "utf8"))) bad.push(`FAIL ${f}:${line} LF_StaticReset.ResetAll()`);
}

if (bad.length) {
  console.log(bad.join("\n"));
  console.log(`validate-no-test-resetall: ${bad.length} call(s). A test applies only its own feature's static reset hook, never ResetAll (#2093).`);
  process.exit(1);
}
console.log(`validate-no-test-resetall: OK (${files.length} test file(s))`);

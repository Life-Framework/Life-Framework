// tools/validation/static-init-budget-selftest.mjs - validate-static-init-budget (#2756).
// inputs: tools/validation/validate-static-init-budget.mjs, tools/validation/static-init-budget-selftest.mjs
//
// Builds scratch addon trees and runs the validator on them (LF_STATIC_INIT_ROOT):
//   - under the cap: exit 0, no WARN, the headroom is printed;
//   - 200 initializers: exit 0 with a WARN line;
//   - 250 pass, 251 fail with the lazy-init advice and the biggest file named;
//   - `static const`, comments and strings are not counted.
// Then the real tree is scanned and its count and headroom are printed.
//
// red-proof: 2026-11-18 before validate-static-init-budget.mjs existed the import failed and the selftest exited 1;
// with it, "251 initializers fail" and every other check print PASS.
import { spawnSync } from "node:child_process";
import { mkdirSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { scratchDir } from "../lib/scratch.mjs";
import { CAP, WARN, countStaticInits, scanTree } from "./validate-static-init-budget.mjs";

let failed = 0;
const check = (ok, msg) => {
  console.log(`${ok ? "PASS" : "FAIL"}  ${msg}`);
  if (!ok) failed++;
};

const here = dirname(fileURLToPath(import.meta.url));
const script = join(here, "validate-static-init-budget.mjs");

// A scratch repo-shaped tree with `n` initializers spread over two files (a big one and a small one).
function fixture(n) {
  const root = scratchDir("lf-static-init-");
  const dir = join(root, "addons", "LifeFramework", "Scripts", "Game", "Core");
  mkdirSync(dir, { recursive: true });
  const big = Math.max(0, n - 3);
  const lines = (from, count) => Array.from({ length: count }, (_, i) => `  protected static ref map<string, ref LF_Foo> s_m${from + i} = new map<string, ref LF_Foo>();`).join("\n");
  writeFileSync(join(dir, "LF_Big.c"), `class LF_Big\n{\n${lines(0, big)}\n}\n`);
  writeFileSync(join(dir, "LF_Small.c"), `class LF_Small\n{\n${lines(1000, n - big)}\n}\n`);
  return root;
}
const run = (n) => {
  const r = spawnSync(process.execPath, [script], { env: { ...process.env, LF_STATIC_INIT_ROOT: fixture(n) }, encoding: "utf8" });
  return { code: r.status, out: (r.stdout || "") + (r.stderr || "") };
};

// Counting.
check(countStaticInits("class A { static int s_a = 0; }".replace("{ ", "{\n")) === 1, "a static field initializer counts");
check(countStaticInits("class A\n{\n  static const int K = 3;\n  static const string S = \"x\";\n}") === 0, "static const does not count");
check(countStaticInits("class A\n{\n  // static int s_a = 0;\n  /* private static int s_b = 1;\n  static int s_c = 2; */\n  string t = \"static int s_d = 3;\";\n}") === 0, "comments and strings do not count");
check(countStaticInits("class A\n{\n  private static ref array<ref LF_X> s_a = {};\n  protected static bool s_b = false;\n  static int s_c;\n  static int Count() { return 1; }\n}") === 2, "private and protected static fields count; a field with no initializer and a static method do not");

// Thresholds.
const under = run(40);
check(under.code === 0 && !/WARN|FAIL/.test(under.out) && /40 static initializer/.test(under.out) && /headroom 210/.test(under.out), `40 initializers pass quietly with headroom 210: ${under.out.trim()}`);
const warn = run(WARN);
check(warn.code === 0 && /WARN static-init-budget/.test(warn.out), `${WARN} initializers pass with a WARN: ${warn.out.trim()}`);
const edge = run(CAP);
check(edge.code === 0 && /headroom 0/.test(edge.out), `${CAP} initializers still pass`);
const over = run(CAP + 1);
check(over.code === 1 && /FAIL static-init-budget/.test(over.out), `${CAP + 1} initializers fail with exit 1`);
check(/LF_Big\.c/.test(over.out) && /if \(!s_x\) s_x = new/.test(over.out), "the failure names the biggest file and says to initialize lazily in the accessor");

// The real tree: print the count and headroom.
const real = scanTree(join(here, "..", ".."));
console.log(`INFO  real tree: ${real.total} static initializers, cap ${CAP}, headroom ${CAP - real.total}`);
check(real.total <= CAP, `the real tree is within the cap (${real.total} <= ${CAP})`);

if (failed > 0) {
  console.log(`static-init-budget-selftest: ${failed} check(s) FAILED`);
  process.exit(1);
}
console.log("static-init-budget-selftest: OK");

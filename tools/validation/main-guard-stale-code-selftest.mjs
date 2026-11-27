// Proves the main guard never acts on stale code. 2026-11-18 the worker started at 23:13:12Z, 14 s before the main
// checkout pulled #2763 (a held lock is INFRA, #2759); a running worker never reloads its modules, so it kept judging
// `FAIL lock 'build' is held` by exit code and reverted #2881 (#2884) and #2887 (#2899). Now the worker compares the
// blob ids it loaded with origin/main every tick: drift refuses every revert, stops checking, and restarts the worker
// once the main checkout fast-forwards; `start` refuses a checkout it cannot bring to origin/main.
// inputs: tools/lib/main-guard.mjs, tools/main-guard.mjs
// red-proof 2026-11-18: on main before the fix this printed "FAIL  cannot load: codeDrift missing"; with the drift
//   line removed from revertRefusal it printed "FAIL  a stale guard may not revert: got null".
import { existsSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import * as g from "../lib/main-guard.mjs";

let failed = 0;
const check = (c, m) => { if (!c) { console.log(`FAIL  ${m}`); failed++; } };
if (typeof g.codeDrift !== "function" || typeof g.staleAction !== "function" || typeof g.parseLsTree !== "function") {
  console.log("FAIL  cannot load: codeDrift missing");
  process.exit(1);
}
const repo = dirname(dirname(dirname(fileURLToPath(import.meta.url))));

// 1. The guard's own files are what it fingerprints.
for (const p of g.GUARD_CODE) check(existsSync(join(repo, p)), `GUARD_CODE names a missing file: ${p}`);

// 2. ls-tree parsing.
const A = "a".repeat(40), B = "b".repeat(40), C = "c".repeat(40), D = "d".repeat(40), E = "e".repeat(40);
const IDS = [A, B, C, E]; // one id per GUARD_CODE file
const lsTree = g.GUARD_CODE.map((p, i) => `100644 blob ${IDS[i]}\t${p}`).join("\n") + "\n";
const cur = g.parseLsTree(lsTree);
check(g.GUARD_CODE.every((p, i) => cur[p] === IDS[i]), `parseLsTree: got ${JSON.stringify(cur)}`);

// 3. Drift: the 2026-11-18 case is the classifier file (tools/lib/main-guard.mjs) changing under a running worker.
check(g.codeDrift({ ...cur }, cur).length === 0, "identical ids: no drift");
const before2763 = { ...cur, "tools/lib/main-guard.mjs": D };
check(JSON.stringify(g.codeDrift(before2763, cur)) === JSON.stringify(["tools/lib/main-guard.mjs"]), `the pre-#2763 classifier: drift, got ${JSON.stringify(g.codeDrift(before2763, cur))}`);
check(g.codeDrift({}, cur).length === g.GUARD_CODE.length, "unknown loaded ids: all drift (cannot prove current code)");
check(g.codeDrift(cur, {}).length === g.GUARD_CODE.length, "origin/main unreadable: all drift");

// 4. A stale guard refuses the revert; a current one does not.
const culprit = { sha: "1b02865242c07a03958c5516c902d0af663343ba", subject: "Merge pull request #2887 from Life-Framework/ws/slice-2847", beforeStart: false };
const stale = g.revertRefusal(culprit, { reverted: [] }, { step: "build", codeDrift: g.codeDrift(before2763, cur) });
check(stale && /stale/.test(stale), `a stale guard may not revert: got ${JSON.stringify(stale)}`);
check(g.revertRefusal(culprit, { reverted: [] }, { step: "build", codeDrift: [] }) === null, "a current guard may revert a real red");

// 5. What a tick does about drift.
check(g.staleAction({ drift: [] }) === "none", "no drift: none");
check(g.staleAction({ drift: ["x"], busy: true, checkoutFresh: true }) === "wait", "drift during a stage-2 run: wait");
check(g.staleAction({ drift: ["x"], checkoutFresh: true }) === "restart", "drift, checkout current: restart");
check(g.staleAction({ drift: ["x"], checkoutFresh: false }) === "stuck", "drift, checkout cannot fast-forward: stuck");

// 6. The worker wiring: drift is checked every tick and before a revert, and `start` runs the main checkout's file.
const src = readFileSync(join(repo, "tools", "main-guard.mjs"), "utf8");
const gate = src.indexOf("mg.staleRefusal(drift)");
check(gate > 0 && src.indexOf("codeDriftNow(dir)") < gate && gate < src.indexOf("mg.revertRefusal(b.culprit"), "checkHead stops on drift before the revert decision");
check(/mg\.staleAction\(/.test(src), "the worker loop decides on drift every tick");
check(!/\[SELF, "worker"/.test(src), "start spawns the main checkout's main-guard.mjs, not the caller's copy");
check(/freshenMainCheckout\(root\)/.test(src), "start fast-forwards the main checkout first");

if (failed) process.exit(1);
console.log("main-guard-stale-code-selftest: OK");

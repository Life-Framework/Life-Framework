// Proves the main guard's compile-first stage and its revert rules (tools/lib/main-guard.mjs, docs/worktrees.md
// "Main guard") offline: a green compile marks compiled-ok; a red compile bisects at once with no recheck and names
// the culprit; a pool failure is no verdict; the maintainer's town PR (#1867 on ws/town-maintainer-edits) is
// reverted for a compile break only, a placement audit red is never reverted (a fix card instead, #1872), and the
// worker wires compile before stage 2.
// inputs: tools/lib/main-guard.mjs, tools/main-guard.mjs
// red-proof 2026-11-16 (4b): before driverNames existed "FAIL driverNames"; the real revert of #1856 failed with
// "lf-regen: cannot identify the merge sides" and applies with the overrides (10 files).
// red-proof 2026-11-16: before compileGate existed this printed "FAIL compileGate exported"; with fixCardInstead
// returning null it printed "FAIL #1867 placement audit: no revert" and "FAIL #1867 suite: fix card, no revert".

import { readFileSync } from "node:fs";
import * as mg from "../lib/main-guard.mjs";

let failed = 0;
const check = (c, m) => { if (!c) { console.log(`FAIL  ${m}`); failed++; } };

check(typeof mg.compileGate === "function", "compileGate exported");
if (typeof mg.compileGate !== "function") process.exit(1);

const merges = [
  { sha: "a".repeat(40), subject: "Merge pull request #11 from Life-Framework/ws/one", order: 0 },
  { sha: "b".repeat(40), subject: "Merge pull request #12 from Life-Framework/ws/two", order: 1 },
  { sha: "c".repeat(40), subject: "Merge pull request #13 from Life-Framework/ws/three", order: 2 },
  { sha: "d".repeat(40), subject: "Merge pull request #14 from Life-Framework/ws/four", order: 3 },
];
const HEAD = "d".repeat(40);

// 1. Green compile.
{
  const seen = [];
  const g = await mg.compileGate({ head: HEAD, merges, state: {}, compile: async (s) => (seen.push(s), "ok") });
  check(g.verdict === "compiled-ok" && seen.length === 1, "green compile: compiled-ok after one run");
}
// 2. Red compile: #12 broke it. One run at the head, then the bisect, no recheck at the head.
{
  const seen = [];
  const broken = new Set(["b", "c", "d"].map((x) => x.repeat(40)));
  const g = await mg.compileGate({ head: HEAD, merges, state: {}, compile: async (s) => (seen.push(s), broken.has(s) ? "fail" : "ok") });
  check(g.verdict === "red", "red compile: verdict red");
  check(mg.prOfMerge(g.culprit?.subject) === 12, `red compile: culprit #12, got ${g.culprit?.subject}`);
  check(seen.filter((s) => s === HEAD).length === 1, "red compile: no recheck at the head (deterministic)");
  check(g.refusal === null, `red compile: revert allowed, got ${g.refusal}`);
}
// 2b. The base was never compile-checked and is red too: nobody is blamed.
{
  const g = await mg.compileGate({ head: HEAD, merges, state: {}, compile: async () => "fail", base: "0".repeat(40), baseChecked: false });
  check(g.verdict === "red" && g.culprit === null && /already red/.test(g.refusal ?? ""), "unchecked red base: no culprit, no revert");
  const ok = await mg.compileGate({ head: HEAD, merges, state: {}, compile: async (s) => (s === "0".repeat(40) || s === "a".repeat(40) ? "ok" : "fail"), base: "0".repeat(40), baseChecked: false });
  check(mg.prOfMerge(ok.culprit?.subject) === 12, "unchecked green base: bisect still runs");
}
// 3. Pool failure: no verdict, no bisect.
{
  const g = await mg.compileGate({ head: HEAD, merges, state: {}, compile: async () => "env" });
  check(g.verdict === "env", "pool failure: env");
  check(mg.compileVerdict(3) === "env" && mg.compileVerdict(4) === "env" && mg.compileVerdict(1) === "fail" && mg.compileVerdict(0) === "ok", "compileVerdict codes");
}
// 4. The maintainer's town edit: compile break reverts it, anything else is a fix card.
{
  const town = { sha: "e".repeat(40), subject: "Merge pull request #1867 from Life-Framework/ws/town-maintainer-edits" };
  check(mg.revertRefusal(town, {}, { step: "compile" }) === null, "#1867 compile: revert allowed");
  check(!!mg.revertRefusal(town, {}, { step: "placement audit" }), "#1867 placement audit: no revert");
  check(!!mg.revertRefusal(town, {}, { step: "suite" }) && !!mg.fixCardInstead(town, "suite"), "#1867 suite: fix card, no revert");
  const other = { sha: "f".repeat(40), subject: "Merge pull request #1900 from Life-Framework/ws/shop" };
  check(!!mg.fixCardInstead(other, "placement audit"), "any PR: placement audit red is a fix card");
  check(mg.revertRefusal(other, {}, { step: "suite" }) === null, "ordinary PR: suite red reverts");
  const card = mg.fixCard(town, "suite", "LF_Test_X", "log.txt");
  check(/#1867/.test(card.title) && /maintainer/.test(card.body), "fix card names the PR and why");
}
// 3b. Exit 1 without the compiler's words (2026-11-15: 11 s on lf-cloud, a false red that bisected to an already
// fixed PR) is no verdict; with "does not compile" it is red.
check(mg.compileVerdict(1, "main guard compile: lf-cloud finished (exit 1) in 11 s") === "env", "exit 1 without a compile error: env");
check(mg.compileVerdict(1, "validate-scripts: FAILED - Enforce Script does not compile [LifeFramework]") === "fail", "exit 1 with a compile error: fail");
// 4b. A revert runs with every merge driver set to keep ours (they fail under git revert; #1847 and #1856 were
// left unreverted that way), then regenerates the generated files.
{
  const names = mg.driverNames('[merge "lf-regen-tests"]\n\tdriver = node x\n[merge "lf-markdown"]\n');
  check(names.join(",") === "lf-regen-tests,lf-markdown", `driverNames: ${names}`);
  check(mg.driverOverrides(["lf-regen-tests"]).join(" ") === "-c merge.lf-regen-tests.driver=true", "driverOverrides keep ours");
  const w = readFileSync(new URL("../main-guard.mjs", import.meta.url), "utf8");
  check(/driverOverrides\(readDriverNames\(dir\)\), "revert"/.test(w) && /REGEN_AFTER_REVERT/.test(w), "revert uses the overrides, then regenerates");
}
// 5. Batching and wiring.
{
  check(mg.shouldCompile({ lastCompileAt: new Date(1000).toISOString() }, HEAD, 1000 + 60000).run === false, "compile batched within 3 min");
  check(mg.shouldCompile({ lastCompileAt: new Date(1000).toISOString() }, HEAD, 1000 + mg.COMPILE_BATCH_MS + 1).run === true, "compile runs after 3 min");
  const w = readFileSync(new URL("../main-guard.mjs", import.meta.url), "utf8");
  const loop = w.slice(w.indexOf("export async function runWorker"));
  check(loop.indexOf("compileHead(") > 0 && loop.indexOf("compileHead(") < loop.indexOf("checkHead(root, dir, s.lastCompiled)"), "worker: compile before stage 2");
  check(/shouldRun\(s, s\.lastCompiled/.test(loop), "worker: stage 2 only on a compiled-ok head");
  check(loop.indexOf("readState(root).request") > 0 && loop.indexOf("readState(root).request") < loop.indexOf("compileHead("), "worker: a requested check (main-guard check <sha>) runs before the head's compile");
  check(loop.includes("checkHead(root, dir, req)"), "worker: the requested commit gets stage 2 on exactly that sha");
  check(/revertRefusal\(b\.culprit, readState\(root\), \{ step: step\.name \}\)/.test(w), "worker: stage 2 refusal knows the step");
}

if (failed) process.exit(1);
console.log("PASS  main-guard compile selftest");

// Proves `wt ship` without a pre-merge gate (tools/lib/ship-direct.mjs, docs/worktrees.md "Ship") with fakes, no
// GitHub, pool or game: preflight then push, PR, auto-merge on, no ship queue; a "behind" refusal merges main in,
// pushes again and retries; a human conflict or a red validate pushes nothing; and the cli wiring calls
// shipDirect, never the queue.
// inputs: tools/lib/ship-direct.mjs, tools/cli.mjs, tools/commands/wait.mjs
// red-proof 2026-11-27 (test advice): with the touchedTests call removed from shipDirect it printed "FAIL  advice: the touched-tests lines are logged".
// red-proof 2026-11-19 (driver overlap): before shipDirect merged main on a both-sides driver-merged edit it printed "FAIL driver overlap: main merged before validate and push".
// red-proof 2026-11-16: before tools/lib/ship-direct.mjs existed this printed "FAIL cannot load
// tools/lib/ship-direct.mjs"; with the BEHIND retry removed (break on any auto-merge result) it printed "FAIL behind:
// main merged in and pushed again"; with cli.mjs still calling shipViaQueue it printed "FAIL cli: wt ship calls
// shipDirectCmd".

import { readFileSync } from "node:fs";

let failed = 0;
const check = (c, m) => { if (!c) { console.log(`FAIL  ${m}`); failed++; } };

let lib;
try {
  lib = await import("../lib/ship-direct.mjs");
} catch {
  console.log("FAIL  cannot load tools/lib/ship-direct.mjs");
  process.exit(1);
}

function fakes(over = {}) {
  const calls = [];
  const d = {
    calls,
    log: () => {},
    mergeTree: () => (calls.push("mergeTree"), { status: 0, stdout: "tree\n" }),
    mergeBase: () => calls.push("mergeBase"),
    validate: () => (calls.push("validate"), 0),
    push: () => (calls.push("push"), { status: 0 }),
    findPr: () => (calls.push("findPr"), null),
    createPr: () => (calls.push("createPr"), { number: 7, url: "https://x/pull/7" }),
    autoMerge: () => (calls.push("autoMerge"), { status: 0, stdout: "" }),
    mergeState: () => "CLEAN",
    ...over,
  };
  return d;
}

// 1. Happy path: preflight, push, PR, auto-merge. Nothing else.
{
  const d = fakes();
  const r = await lib.shipDirect(d, { slug: "x" });
  check(r.code === 0 && r.pr?.number === 7, "happy: exit 0 with the PR");
  check(d.calls.join(",") === "mergeTree,validate,push,findPr,createPr,autoMerge", `happy: order was ${d.calls.join(",")}`);
}
// 2. GitHub refuses auto-merge because the branch is behind: merge main, push, retry.
{
  let n = 0;
  const d = fakes({ autoMerge: () => (d.calls.push("autoMerge"), ++n === 1 ? { status: 1, stderr: "GraphQL: Base branch was modified. Review and try the merge again." } : { status: 0 }) });
  const r = await lib.shipDirect(d, { slug: "x" });
  check(r.code === 0, "behind: exit 0 after the retry");
  check(d.calls.join(",") === "mergeTree,validate,push,findPr,createPr,autoMerge,mergeBase,push,autoMerge", "behind: main merged in and pushed again");
}
// 2b. The auto-heal workflow merged main into the PR branch after the last ship: the push is refused as
// non-fast-forward, ship merges the remote branch in once and pushes again.
{
  let n = 0;
  const d = fakes({
    push: () => (d.calls.push("push"), ++n === 1 ? { status: 1, stderr: " ! [rejected]        ws/x -> ws/x (fetch first)\nerror: failed to push some refs" } : { status: 0 }),
    pullBranch: () => d.calls.push("pullBranch"),
  });
  const r = await lib.shipDirect(d, { slug: "x" });
  check(r.code === 0, "remote ahead: exit 0 after merging the remote branch");
  check(d.calls.join(",") === "mergeTree,validate,push,pullBranch,push,findPr,createPr,autoMerge", `remote ahead: order was ${d.calls.join(",")}`);
  const other = fakes({ push: () => ({ status: 1, stderr: "fatal: unable to access: Could not resolve host" }), pullBranch: () => other.calls.push("pullBranch") });
  check((await lib.shipDirect(other, { slug: "x" })).code === 1 && !other.calls.includes("pullBranch"), "a network failure is not a remote-ahead refusal");
}
// 3. Auto-merge on but the PR is BEHIND: same heal, once.
{
  let n = 0;
  const d = fakes({ mergeState: () => (++n === 1 ? "BEHIND" : "CLEAN") });
  const r = await lib.shipDirect(d, { slug: "x" });
  check(r.code === 0 && d.calls.filter((c) => c === "mergeBase").length === 1, "mergeState BEHIND: one merge of main");
}
// 4. A human conflict or a red validate: nothing pushed.
{
  const d = fakes({ mergeTree: () => ({ status: 1, stdout: "tree\naddons/LifeFramework/Scripts/Game/X.c\n\nCONFLICT" }) });
  const r = await lib.shipDirect(d, { slug: "x" });
  check(r.code === 1 && !d.calls.includes("push"), "human conflict: refused, nothing pushed");
  const v = fakes({ validate: () => 1 });
  const r2 = await lib.shipDirect(v, { slug: "x" });
  check(r2.code === 1 && !v.calls.includes("push"), "red validate: refused, nothing pushed");
}
// 5. A refusal that is not "behind" fails loudly, no merge.
{
  const d = fakes({ autoMerge: () => ({ status: 1, stderr: "Pull request is in clean status" }) });
  const r = await lib.shipDirect(d, { slug: "x" });
  check(r.code === 1 && !d.calls.includes("mergeBase"), "other refusal: fail, no merge");
}
// 6a. merge-tree runs the lf-markdown driver, so it calls a both-sides docs/features append clean while GitHub (no
// custom drivers) calls the PR CONFLICTING (#3735, #3736). When both sides touched a driver-merged file the branch
// merges origin/main here before the push, so the driver runs and GitHub sees main already in.
{
  const d = fakes({ driverOverlap: () => ["docs/features/crime-police.md"] });
  const r = await lib.shipDirect(d, { slug: "x" });
  check(r.code === 0, "driver overlap: ship succeeds");
  check(d.calls.join(",") === "mergeTree,mergeBase,validate,push,findPr,createPr,autoMerge", `driver overlap: main merged before validate and push, order was ${d.calls.join(",")}`);
  const n = fakes({ driverOverlap: () => [] });
  await lib.shipDirect(n, { slug: "x" });
  check(!n.calls.includes("mergeBase"), "no driver overlap: main is not merged");
  const q = fakes({ driverOverlap: () => ["docs/features/crime-police.md"] });
  await lib.shipDirect(q, { slug: "x", preflight: false });
  check(q.calls.join(",").startsWith("mergeBase,push"), "driver overlap without preflight: still merged before the push");
  check(JSON.stringify(lib.bothSidesTouched(["a", "b\\c", "d"], ["b/c", "d", "e"])) === JSON.stringify(["b/c", "d"]), "bothSidesTouched: intersection, slashes normalised");
}
// 6. Wiring: wt ship goes through shipDirect; the queue is out of the flow.
{
  const cli = readFileSync(new URL("../cli.mjs", import.meta.url), "utf8");
  const ship = cli.slice(cli.indexOf("async function wtShipCmd"), cli.indexOf("function cmdWt("));
  check(/shipDirectCmd\(/.test(ship), "cli: wt ship calls shipDirectCmd");
  check(!/shipViaQueue|ensureWorker|withQueue/.test(cli), "cli: no ship queue in the ship flow");
  check(/driverOverlap:/.test(cli.slice(cli.indexOf("async function shipDirectCmd"))), "cli: ship passes driverOverlap");
  check(/--auto", "--merge"/.test(cli.slice(cli.indexOf("async function shipDirectCmd"))), "cli: auto-merge --merge");
}

// 6b. The revert audit's test advice (tools/lib/touched-tests.mjs): the lines are logged before validate, and a
// throwing or missing advice dependency never fails or reorders a ship.
{
  const logged = [];
  const d = fakes({ log: (l) => logged.push(l), touchedTests: () => ["ship: run these", "  tools\\cli test --no-build --only LF_Test_X"] });
  const r = await lib.shipDirect(d, { slug: "x" });
  check(r.code === 0 && logged.includes("  tools\\cli test --no-build --only LF_Test_X"), "advice: the touched-tests lines are logged");
  check(d.calls.join(",") === "mergeTree,validate,push,findPr,createPr,autoMerge", `advice: the ship order is unchanged, was ${d.calls.join(",")}`);
  const t = fakes({ touchedTests: () => { throw new Error("boom"); } });
  check((await lib.shipDirect(t, { slug: "x" })).code === 0, "advice: a throwing touchedTests does not fail the ship");
  const cli = readFileSync(new URL("../cli.mjs", import.meta.url), "utf8");
  check(/touchedTests:/.test(cli.slice(cli.indexOf("async function shipDirectCmd"))), "cli: ship passes touchedTests");
}

// 7. shipMergeBase: a generated-only conflict is healed, a human conflict throws with the files named.
{
  const logs = [];
  const healed = lib.shipMergeBase("w", "main", () => ({ status: "healed", resolved: ["addons/LifeFramework/Scripts/Game/Tests/LF_TestRegistrations.generated.c"] }), (l) => logs.push(l));
  check(healed.status === "healed" && /regenerated .*LF_TestRegistrations/.test(logs.join(" ")), "shipMergeBase: registry conflict healed, not failed");
  let msg = "";
  try { lib.shipMergeBase("w", "main", () => ({ status: "needs-human", reason: "conflicts outside generated and driver-merged files", conflicted: ["a.c"] })); } catch (e) { msg = e.message; }
  check(/conflicts in: a.c/.test(msg), "shipMergeBase: human conflict throws and names the file");
  const cli = readFileSync(new URL("../cli.mjs", import.meta.url), "utf8");
  const direct = cli.slice(cli.indexOf("async function shipDirectCmd"));
  check(direct.includes("shipMergeBase(wtRoot, base, healMergeSync"), "cli: ship merges main through healMerge, not a plain git merge");
  check(cli.includes(`argValue(flags, "--commit")`), "cli: wt ship --commit commits a dirty worktree");
}

if (failed) process.exit(1);
console.log("PASS  ship-direct selftest");

// Proves `wt ship` without a pre-merge gate (tools/lib/ship-direct.mjs, docs/worktrees.md "Ship") with fakes, no
// GitHub, pool or game: preflight then push, PR, auto-merge on, no ship queue; a "behind" refusal merges main in,
// pushes again and retries; a human conflict or a red validate pushes nothing; and the cli wiring calls
// shipDirect, never the queue.
// inputs: tools/lib/ship-direct.mjs, tools/lib/ship-queue.mjs, tools/cli.mjs, tools/commands/wait.mjs
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
// 6. Wiring: wt ship goes through shipDirect; the queue is out of the flow.
{
  const cli = readFileSync(new URL("../cli.mjs", import.meta.url), "utf8");
  const ship = cli.slice(cli.indexOf("async function wtShipCmd"), cli.indexOf("function cmdWt("));
  check(/shipDirectCmd\(/.test(ship), "cli: wt ship calls shipDirectCmd");
  check(!/shipViaQueue|ensureWorker|withQueue/.test(cli), "cli: no ship queue in the ship flow");
  check(/--auto", "--merge"/.test(cli.slice(cli.indexOf("async function shipDirectCmd"))), "cli: auto-merge --merge");
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

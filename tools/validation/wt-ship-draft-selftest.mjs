// Proves `wt ship <slug> --draft` (docs/worktrees.md "Ship") with fakes, no GitHub: the body checks and preflight still
// run, the branch is pushed, the PR is created as a draft (`gh pr create --draft`), and no `gh pr merge --auto` step is
// planned, so town-draft and maintainer-review PRs take the same path as every other ship. Without the flag nothing
// changes: the PR is not a draft and auto-merge is on.
// inputs: tools/lib/ship-direct.mjs, tools/wt.mjs, tools/cli.mjs
// red-proof 2026-11-17: before the flag existed (shipDirect ignored `draft`, createPrGh had no --draft) this printed
// "FAIL  draft: no autoMerge step planned" and "FAIL  createPrGh --draft passes --draft to gh pr create".

import { readFileSync } from "node:fs";

let failed = 0;
const check = (c, m) => { if (!c) { console.log(`FAIL  ${m}`); failed++; } };

const lib = await import("../lib/ship-direct.mjs");
const wt = await import("../wt.mjs");

function fakes(over = {}) {
  const calls = [];
  return {
    calls,
    log: () => {},
    mergeTree: () => (calls.push("mergeTree"), { status: 0, stdout: "tree\n" }),
    mergeBase: () => calls.push("mergeBase"),
    validate: () => (calls.push("validate"), 0),
    push: () => (calls.push("push"), { status: 0 }),
    findPr: () => (calls.push("findPr"), null),
    createPr: (o) => (calls.push(o?.draft ? "createPr:draft" : "createPr"), { number: 7, url: "https://x/pull/7" }),
    autoMerge: () => (calls.push("autoMerge"), { status: 0, stdout: "" }),
    mergeState: () => "CLEAN",
    ...over,
  };
}

// 1. --draft: preflight, push, draft PR, and no auto-merge step at all.
{
  const d = fakes();
  const r = await lib.shipDirect(d, { slug: "x", draft: true });
  check(r.code === 0 && r.pr?.number === 7, "draft: exit 0 with the PR");
  check(d.calls.join(",") === "mergeTree,validate,push,findPr,createPr:draft", `draft: no autoMerge step planned, order was ${d.calls.join(",")}`);
  check(r.draft === true, "draft: result says draft");
}
// 2. Without the flag: unchanged (not a draft, auto-merge on).
{
  const d = fakes();
  const r = await lib.shipDirect(d, { slug: "x" });
  check(r.code === 0 && d.calls.join(",") === "mergeTree,validate,push,findPr,createPr,autoMerge", `default: auto-merge still on, order was ${d.calls.join(",")}`);
}
// 3. --draft with a red validate or a human conflict: nothing pushed, same as any ship.
{
  const v = fakes({ validate: () => 1 });
  const r = await lib.shipDirect(v, { slug: "x", draft: true });
  check(r.code === 1 && !v.calls.includes("push"), "draft: red validate pushes nothing");
}
// 4. --draft on a PR that is already open: it is reused, still never auto-merged.
{
  const d = fakes({ findPr: () => (d.calls.push("findPr"), { number: 9, url: "https://x/pull/9" }) });
  const r = await lib.shipDirect(d, { slug: "x", draft: true });
  check(r.code === 0 && r.pr.number === 9 && !d.calls.includes("autoMerge"), "draft: open PR reused, no auto-merge");
}
// 5. createPrGh passes --draft to `gh pr create` only when asked.
{
  const seen = [];
  const fake = (_bin, args) => { seen.push(args); return { status: 0, stdout: "https://github.com/o/r/pull/12\n", stderr: "" }; };
  const root = process.cwd();
  const a = wt.createPrGh(root, { head: "ws/a", title: "t", body: "b", base: "main", draft: true }, { run: fake, gh: "gh" });
  check(a.number === 12, "createPrGh draft: returns the PR");
  check(seen[0]?.includes("--draft") && seen[0][0] === "pr" && seen[0][1] === "create", "createPrGh --draft passes --draft to gh pr create");
  wt.createPrGh(root, { head: "ws/a", title: "t", body: "b", base: "main" }, { run: fake, gh: "gh" });
  check(!seen[1]?.includes("--draft"), "createPrGh without draft: no --draft");
}
// 6. Wiring: wt ship reads --draft, passes it on, and the draft summary says auto-merge is off.
{
  const cli = readFileSync(new URL("../cli.mjs", import.meta.url), "utf8");
  const cmd = cli.slice(cli.indexOf("async function shipDirectCmd"), cli.indexOf("async function wtShipCmd"));
  check(/--draft/.test(cmd) && /draft/.test(cmd.slice(cmd.indexOf("createPr:"))), "cli: shipDirectCmd reads --draft and passes it to createPrGh");
  check(/\{ base, slug, preflight[^}]*draft/.test(cmd), "cli: shipDirectCmd passes draft to shipDirect");
  const ship = cli.slice(cli.indexOf("async function wtShipCmd"), cli.indexOf("function cmdWt("));
  check(/--draft/.test(ship), "cli: wtShipCmd handles --draft on the --gate/no-gh path too");
  const direct = ship.match(/if \(([^\n]*)\) return shipDirectCmd\(/);
  check(direct && !/\bprOnly\b/.test(direct[1]), "cli: --draft still takes the shipDirect path (prOnly is not in its condition)");
}

if (failed) process.exit(1);
console.log("PASS  wt-ship-draft selftest");

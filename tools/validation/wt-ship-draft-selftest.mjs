// Proves `wt ship <slug> --draft` (docs/worktrees.md "Ship") with fakes, no GitHub: the body checks and preflight still
// run, the branch is pushed, the PR is created as a draft (`gh pr create --draft`), and no `gh pr merge --auto` step is
// planned, so town-draft and maintainer-review PRs take the same path as every other ship. Without the flag nothing
// changes: the PR is not a draft and auto-merge is on.
// inputs: tools/lib/ship-direct.mjs, tools/wt.mjs, tools/cli.mjs
// red-proof 2026-11-17: before the flag existed (shipDirect ignored `draft`, createPrGh had no --draft) this printed
// "FAIL  draft: no autoMerge step planned" and "FAIL  createPrGh --draft passes --draft to gh pr create".
// red-proof 2026-11-18 (#2940): with ship-direct.mjs as it was, a reused open PR was left ready with auto-merge on; the
// selftest printed "FAIL  draft: reused PR gets auto-merge off and draft", "FAIL  draft: prints 'draft: auto-merge off
// (REVIEW-NEEDED)'" and "FAIL  draft: a failed auto-merge disable fails the ship".

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
// 4b. --draft on an open PR an earlier ship left ready with auto-merge queued (#2940, PR #2938): auto-merge is turned
// off, the PR goes back to draft, and the summary says so. A refused disable fails the ship instead of passing it.
{
  const d = fakes({
    findPr: () => (d.calls.push("findPr"), { number: 9, url: "https://x/pull/9" }),
    disableAutoMerge: (n) => (d.calls.push(`disableAutoMerge:${n}`), { status: 0, stdout: "" }),
    convertToDraft: (n) => (d.calls.push(`convertToDraft:${n}`), { status: 0, stdout: "" }),
  });
  const lines = [];
  d.log = (l) => lines.push(l);
  const r = await lib.shipDirect(d, { slug: "x", draft: true });
  check(r.code === 0 && d.calls.includes("disableAutoMerge:9") && d.calls.includes("convertToDraft:9"), `draft: reused PR gets auto-merge off and draft, calls were ${d.calls.join(",")}`);
  check(!d.calls.includes("autoMerge"), "draft: reused PR is never auto-merged");
  check(lines.includes("draft: auto-merge off (REVIEW-NEEDED)"), "draft: prints 'draft: auto-merge off (REVIEW-NEEDED)'");
  const f = fakes({ findPr: () => ({ number: 9, url: "https://x/pull/9" }), disableAutoMerge: () => ({ status: 1, stderr: "boom" }) });
  const rf = await lib.shipDirect(f, { slug: "x", draft: true });
  check(rf.code === 1, "draft: a failed auto-merge disable fails the ship");
  const ok = fakes({ findPr: () => ({ number: 9, url: "https://x/pull/9" }), disableAutoMerge: () => ({ status: 1, stderr: "auto-merge is not enabled" }) });
  check((await lib.shipDirect(ok, { slug: "x", draft: true })).code === 0, "draft: 'not enabled' on disable is fine");
  const fresh = fakes();
  fresh.disableAutoMerge = () => (fresh.calls.push("disableAutoMerge"), { status: 0 });
  await lib.shipDirect(fresh, { slug: "x", draft: true });
  check(!fresh.calls.includes("disableAutoMerge"), "draft: a freshly created draft needs no disable call");
}
// 4c. Wiring: the CLI passes real disable / convert-to-draft calls.
{
  const cli = readFileSync(new URL("../cli.mjs", import.meta.url), "utf8");
  const cmd = cli.slice(cli.indexOf("async function shipDirectCmd"), cli.indexOf("async function wtShipCmd"));
  check(/--disable-auto/.test(cmd) && /"ready", String\(n\), "--undo"/.test(cmd), "cli: shipDirectCmd wires --disable-auto and gh pr ready --undo");
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

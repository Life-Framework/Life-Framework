// Proves the ship queue's rules (tools/lib/ship-queue.mjs, docs/worktrees.md "Ship queue") without git,
// ssh or a game: FIFO order, the parallel/overlap rule, the re-gate decision and the queue file.
// inputs: tools/lib/ship-queue.mjs, tools/prs.mjs, tools/ship-queue.mjs, tools/remote/pool.mjs, tools/pool.json
// red-proof 2026-11-14 (ship-train), each broken alone: pickRunnable without the earlier waiting entries in `ahead`
// "b done: c starts, e still waits behind c" (e jumped c); regateDecision without the generated check "main changed
// the test registry and the PR was re-gated"; overlap() counting driver-merged files "two PRs that only both append
// to docs/features.md ran in parallel (abd start)"; environmentFailure without the 0-test shard rule "shards placed nowhere
// (exit 3, 0 tests): requeued".
// red-proof 2026-11-16 (kill-hotspots): docsText back to /^docs/.+.md$/ printed FAIL "overlap() ignores markdown outside
// docs/ too (Feature.md, README)".
// red-proof 2026-11-14 (shipq-slots), each broken alone: overlap() without docsText "two PRs that both change a docs/**/*.md
// file start together"; needsWorkbench() always true "a T0 entry starts while the only Workbench slot is held"; without its
// tier 1 line "T1 without a build takes no slot"; normalize() in pool.mjs without workbenchLimit "loadMachines keeps a
// machine's workbenchLimit" and "gateSlots sums ... (got 2, want 2 + 1)"; poolSettings() without shipQueue "pool.shipQueue in
// lf.local.json reaches gateSlots (got {slots:3,light:4})".

import { mkdtempSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { writeFileSync } from "node:fs";
import * as sq from "../lib/ship-queue.mjs";
import { gateSlots, gateLimits, ensureWorker, workerRunningText } from "../ship-queue.mjs";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import * as pool from "../remote/pool.mjs";

let failed = 0;
const check = (ok, msg) => {
  if (ok) console.log(`PASS  ${msg}`);
  else {
    failed++;
    console.log(`FAIL  ${msg}`);
  }
};

const REG = "addons/LifeFramework/Scripts/Game/Tests/LF_TestRegistrations.generated.c";
const q = { seq: 0, entries: [] };
const add = (slug, files, at) => sq.enqueue(q, { slug, files, branch: `ws/${slug}` }, at);

// 1. FIFO: arrival order, and a re-ship of a waiting slug keeps its place.
// a and d change addons/ files, so each needs a Workbench slot (a PR with no addons/ change does not: 2d).
add("a", ["addons/LifeFramework/Scripts/Game/Core/LF_A.c", "docs/features.md"], "2026-11-14T10:00:00Z");
add("b", ["addons/LifeFramework/Scripts/Game/Feature/Bank/LF_Bank.c", "docs/features.md"], "2026-11-14T10:01:00Z");
add("c", ["addons/LifeFramework/Scripts/Game/Feature/Bank/LF_Bank.c"], "2026-11-14T10:02:00Z");
add("d", ["addons/LifeFramework/Scripts/Game/Core/LF_D.c", "docs/worktrees.md", "docs/features.md"], "2026-11-14T10:03:00Z");
check(sq.ordered(q.entries).map((e) => e.slug).join("") === "abcd", "queued entries come out in arrival order");
add("a", ["addons/LifeFramework/Scripts/Game/Core/LF_A.c", "tools/a2.mjs"], "2026-11-14T10:09:00Z");
check(sq.ordered(q.entries).map((e) => e.slug).join("") === "abcd" && q.entries.filter((e) => e.slug === "a").length === 1, "a re-ship of a waiting slug replaces it and keeps its place");

// 2. Parallel / overlap: one slot runs the head only; three slots run a, b, d side by side
// (they share only driver-merged docs/features.md); c waits for b (same script file), even with room.
let p = sq.pickRunnable(q.entries, { slots: 1 });
check(p.start.map((s) => s.entry.slug).join("") === "a", "one gate slot: only the head of the queue starts");
p = sq.pickRunnable(q.entries, { slots: 3 });
check(p.start.map((s) => s.entry.slug).join("") === "abd", "two PRs that only both append to docs/features.md ran in parallel (abd start)");
check(p.wait.some((w) => w.entry.slug === "c" && /waits for b/.test(w.why)), "c waits for b: both change LF_Bank.c");
// b running, c still queued and blocked; a later e that overlaps c (waiting) must not jump it.
q.entries.find((e) => e.slug === "b").state = "gating";
add("e", ["addons/LifeFramework/Scripts/Game/Feature/Bank/LF_Bank.c", "tools/e.mjs"], "2026-11-14T10:04:00Z");
p = sq.pickRunnable(q.entries, { slots: 4 });
check(!p.start.some((s) => s.entry.slug === "c" || s.entry.slug === "e"), "an entry never jumps an earlier waiting entry that changes the same file (c, e wait behind b)");
q.entries.find((e) => e.slug === "b").state = "passed";
p = sq.pickRunnable(q.entries, { slots: 4 });
check(p.start.some((s) => s.entry.slug === "c") && !p.start.some((s) => s.entry.slug === "e"), "b done: c starts, e still waits behind c");
check(sq.overlap(["docs/features.md", "x.c"], ["docs/features.md", "y.c"]).length === 0, "driver-merged files never count as an overlap");

// 2b. docs/**/*.md never serialises: two PRs that edit docs/worktrees.md (or any docs prose) run side by side;
// a .md outside docs/ and a non-.md file under docs/ still make the later PR wait.
const dq = { seq: 0, entries: [] };
const dadd = (slug, files, at) => sq.enqueue(dq, { slug, files, branch: `ws/${slug}` }, at);
dadd("p", ["tools/p.mjs", "docs/worktrees.md", "docs/architecture/core-vs-pack.md"], "2026-11-14T11:00:00Z");
dadd("q", ["tools/q.mjs", "docs/worktrees.md", "docs/architecture/core-vs-pack.md"], "2026-11-14T11:01:00Z");
dadd("r", ["tools/r.mjs", "README.md", "docs/data/table.json"], "2026-11-14T11:02:00Z");
dadd("s", ["tools/s.mjs", "README.md", "docs/data/table.json"], "2026-11-14T11:03:00Z");
p = sq.pickRunnable(dq.entries, { slots: 4 });
check(["p", "q", "r"].every((x) => p.start.some((s) => s.entry.slug === x)), "two PRs that both change a docs/**/*.md file start together (docs-md-overlap)");
check(p.wait.some((w) => w.entry.slug === "s" && /waits for r/.test(w.why)), "a shared README.md or docs/*.json still makes the later PR wait");
check(sq.overlap(["docs/worktrees.md"], ["docs/worktrees.md"]).length === 0 && sq.overlap(["docs/x/y.MD"], ["docs/x/y.MD"]).length === 0, "overlap() ignores docs/**/*.md");
// #1852 waited for #1850 on Medic/Feature.md: markdown anywhere is driver-merged prose, never an overlap.
const MEDIC_DOC = "addons/LifeFramework/Scripts/Game/Feature/Medic/Feature.md";
check(sq.overlap([MEDIC_DOC, "addons/a.c"], [MEDIC_DOC, "addons/b.c"]).length === 0 && sq.overlap(["README.md"], ["README.md"]).length === 0, "overlap() ignores markdown outside docs/ too (Feature.md, README)");
check(sq.overlap([MEDIC_DOC, "addons/a.c"], [MEDIC_DOC, "addons/a.c"]).join() === "addons/a.c", "overlap() still counts a shared code file beside a shared Feature.md");
// #1850 waited for #1847 on LF_StaticReset.c: with per-feature hooks two reset PRs share no file.
check(sq.overlap(["addons/LifeFramework/Scripts/Game/Feature/Medic/LF_Revive.c", "docs/features/character-survival.md"], ["addons/LifeFramework/Scripts/Game/Feature/Police/LF_Stun.c", "docs/features/crime-police.md"]).length === 0, "two features that each register a static reset do not overlap");
const dd = sq.regateDecision({ prFiles: ["tools/a.mjs", "docs/worktrees.md"], mainFiles: ["docs/worktrees.md"] });
check(!dd.regate, "main editing the same docs prose file does not re-gate (GitHub's merge check catches a text conflict)");

// 2c. Gate slots: "parallel" wins; else the Workbench limits of the ssh machines with the workbench cap.
const ssh = (name, extra = {}) => ({ name, via: "ssh", caps: ["server", "workbench"], ...extra });
check(sq.slotCount({ cfg: { parallel: 5 }, machines: [ssh("a")] }) === 5, "shipQueue.parallel overrides the machine sum (slot-overrides)");
check(sq.slotCount({ machines: [ssh("a"), ssh("b", { workbenchLimit: 2 })] }) === 3, "per-machine workbenchLimit counts: 1 + 2 = 3 slots");
check(sq.slotCount({ machines: [ssh("a", { disabled: true }), ssh("b", { caps: ["server"] }), { name: "local", via: "local", caps: ["workbench"], workbenchLimit: 4 }] }) === 1, "disabled, no-workbench-cap and local machines add no slots; at least 1");
check(sq.lightCount({}) === sq.DEFAULT_LIGHT && sq.lightCount({ cfg: { light: 7 } }) === 7, "shipQueue.light sets the validate-only gate count");
// The real loaders: the override and the per-machine limit survive poolSettings / loadMachines.
{
  const sroot = mkdtempSync(join(tmpdir(), "lf-shipq-slots-"));
  try {
    const machines = [
      { name: "lf-testbox", disabled: true },
      { name: "local", disabled: true },
      { name: "sq-a", via: "ssh", host: "sq-selftest-a", caps: ["server", "workbench"], workbenchLimit: 2 },
      { name: "sq-b", via: "ssh", host: "sq-selftest-b", caps: ["server", "workbench"] },
      { name: "sq-c", via: "ssh", host: "sq-selftest-c", caps: ["server"], workbenchLimit: 9 },
    ];
    writeFileSync(join(sroot, "lf.local.json"), JSON.stringify({ pool: { machines } }));
    check(pool.loadMachines(sroot).find((m) => m.name === "sq-a")?.workbenchLimit === 2, "loadMachines keeps a machine's workbenchLimit");
    check((await gateSlots(sroot)) === 3, `gateSlots sums the pool.machines workbenchLimits from lf.local.json (got ${await gateSlots(sroot)}, want 2 + 1)`);
    writeFileSync(join(sroot, "lf.local.json"), JSON.stringify({ pool: { machines, shipQueue: { parallel: 6, light: 2 } } }));
    const lim = await gateLimits(sroot);
    check(pool.poolSettings(sroot).shipQueue?.parallel === 6 && lim.slots === 6 && lim.light === 2, `pool.shipQueue in lf.local.json reaches gateSlots (got ${JSON.stringify(lim)})`);
  } finally {
    rmSync(sroot, { recursive: true, force: true });
  }
}

// 2d. A gate that needs no Workbench takes no Workbench slot: a docs/tools-only PR (T0) starts beside a running
// Workbench gate with one slot; an addons/ PR waits for the slot. T1 takes one only when it builds.
const lq = { seq: 0, entries: [] };
const ladd = (slug, files, at, extra = {}) => Object.assign(sq.enqueue(lq, { slug, files, branch: `ws/${slug}` }, at), extra);
const G = "addons/LifeFramework/Scripts/Game/Feature/";
ladd("wb1", [`${G}A/LF_A.c`], "2026-11-14T12:00:00Z", { state: "gating" });
ladd("wb2", [`${G}B/LF_B.c`], "2026-11-14T12:01:00Z");
ladd("t0", ["tools/x.mjs", "docs/y.md"], "2026-11-14T12:02:00Z");
ladd("t1d", ["addons/LifeFramework/Configs/a.conf"], "2026-11-14T12:03:00Z", { tier: 1, builds: false });
ladd("t1b", ["addons/LifeFramework/Configs/b.conf"], "2026-11-14T12:04:00Z", { tier: 1, builds: true });
ladd("t0b", ["website/z.js"], "2026-11-14T12:05:00Z");
p = sq.pickRunnable(lq.entries, { slots: 1 });
const started = (x) => p.start.some((s) => s.entry.slug === x);
check(started("t0") && started("t0b"), "a T0 entry (no addons/ change) starts while the only Workbench slot is held (no-wb-slot-t0)");
check(started("t1d") && !started("wb2") && !started("t1b"), "T1 without a build takes no slot; an addons/ build and a building T1 wait for the held slot");
check(p.wait.some((w) => w.entry.slug === "wb2" && /gate slot/.test(w.why)), "the waiting Workbench entry says it waits for a gate slot");
lq.entries.find((e) => e.slug === "wb1").state = "passed";
p = sq.pickRunnable(lq.entries, { slots: 1 });
check(started("wb2") && !started("t1b"), "slot freed: the head Workbench entry takes it, one slot, the next waits");
p = sq.pickRunnable(lq.entries, { slots: 1, lightSlots: 1 });
check(p.start.filter((s) => !sq.needsWorkbench(s.entry)).length === 1, "validate-only gates are capped by lightSlots");
// Running T0 gates do not eat the Workbench slots either.
lq.entries.find((e) => e.slug === "t0").state = "gating";
lq.entries.find((e) => e.slug === "wb2").state = "queued";
p = sq.pickRunnable(lq.entries, { slots: 1 });
check(started("wb2"), "a running validate-only gate does not count against the Workbench slots");

// 3. Re-gate decision.
const pr = ["addons/LifeFramework/Scripts/Game/Feature/Bank/LF_Bank.c", "docs/features.md"];
let d = sq.regateDecision({ prFiles: pr, mainFiles: [] });
check(!d.regate, "main did not move: no re-gate");
d = sq.regateDecision({ prFiles: pr, mainFiles: ["tools/other.mjs", "docs/features.md"] });
check(!d.regate, "main moved on other files and docs/features.md only: pushed as is");
d = sq.regateDecision({ prFiles: pr, mainFiles: ["addons/LifeFramework/Scripts/Game/Feature/Bank/LF_Bank.c"] });
check(d.regate && /LF_Bank\.c/.test(d.why), "main changed a file the PR changes: re-gate");
d = sq.regateDecision({ prFiles: pr, mainFiles: [REG] });
check(d.regate && /generated/.test(d.why), "main changed the test registry and the PR was re-gated");
d = sq.regateDecision({ prFiles: pr, mainFiles: ["addons/LifeFramework/Language/everonlife_localization.de_de.conf"] });
check(d.regate, "main changed a generated language conf: re-gate");
d = sq.regateDecision({ prFiles: pr, mainFiles: [REG], attempts: 3 });
check(!d.regate && d.capped, "after 3 gates the head goes up (GitHub's validate covers the rest)");

// 3b. A pool failure is requeued (with a back-off), a real test failure is not.
const lines = (...l) => l.join("\n");
check(sq.environmentFailure(lines("pool: lf-cloud does not answer ssh", "FAIL  pool: no reachable machine for this run (lf-testbox, lf-cloud)")) !== null, "no reachable machine: requeued, not failed");
check(sq.environmentFailure(lines("shard normal 1/1: exit 3, 2.4 s, 0 test(s)", "shard core 1/1: exit 3, 2.5 s, 0 test(s)")) !== null, "shards placed nowhere (exit 3, 0 tests): requeued");
check(sq.environmentFailure(lines("shard normal 1/2: exit 1, 80 s, 260 test(s)", "shard normal 2/2: exit 3, 2 s, 0 test(s)")) === null, "a shard that ran tests and failed is a real failure");
check(sq.environmentFailure("FAIL  validate/validate-guids.mjs") === null, "a validate failure is a real failure");
const later = { slug: "z", id: "z-1", seq: 1, state: "queued", files: ["z.c"], notBefore: "2026-11-14T10:05:00Z" };
check(sq.pickRunnable([later], { slots: 2, now: Date.parse("2026-11-14T10:04:00Z") }).start.length === 0 && sq.pickRunnable([later], { slots: 2, now: Date.parse("2026-11-14T10:06:00Z") }).start.length === 1, "a requeued entry waits out its back-off, then starts");

// 4. Queue file round trip; finished entries are capped; a running slug cannot be enqueued again.
const dir = mkdtempSync(join(tmpdir(), "lf-shipq-"));
try {
  q.entries.find((e) => e.slug === "a").state = "merging";
  let threw = false;
  try {
    add("a", ["addons/LifeFramework/Scripts/Game/Core/LF_A.c"]);
  } catch {
    threw = true;
  }
  check(threw, "a slug whose gate is running is not enqueued twice");
  for (let i = 0; i < sq.KEEP_DONE + 5; i++) q.entries.push({ slug: `old${i}`, id: `old${i}`, seq: 100 + i, state: "passed", endedAt: new Date(Date.UTC(2026, 10, 1, 0, i)).toISOString() });
  sq.writeQueue(dir, q);
  const back = sq.readQueue(dir);
  check(back.entries.filter((e) => sq.DONE.includes(e.state)).length === sq.KEEP_DONE && back.seq === q.seq, "the queue file round-trips and keeps the newest finished entries");
  check(sq.statusLines(back).some((l) => /merging/.test(l)), "status lines show a running gate");
  check(sq.entryFor(back, "a")?.state === "merging", "entryFor finds the live entry for a slug");
} finally {
  rmSync(dir, { recursive: true, force: true });
}

// 7. `wt ship` ended with "FAIL quiet-launch failed: ... exited within 3s with code 0" when a worker already held the
// lock: the new worker returns 0 at once (runWorker) and quiet-launch calls a fast exit a failed launch. A live worker is
// found before any spawn, and a fast exit 0 from a spawn that lost the lock race is "already running", not a failure.
// red-proof 2026-11-14 (worker-fast-exit): with the catch in ensureWorker rethrowing, "a worker that lost the lock race
// is already running, not a failed launch" never ran: the selftest died with "quiet-launch failed: ... exited within 3s with code 0"; with the alive() pre-check removed,
// "a live worker is not spawned again" failed (spawned: 1).
{
  const repo = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
  const noSleep = () => Promise.resolve();
  let spawned = 0;
  const live = await ensureWorker(repo, { alive: () => 4242, spawn: () => { spawned++; return 1; }, sleep: noSleep });
  check(spawned === 0 && live.pid === 4242 && live.already && live.message === "worker already running (pid 4242)", `a live worker is not spawned again (spawned: ${spawned}, ${live.message})`);

  const fastExit = () => { throw new Error("quiet-launch failed: quiet-launch: node.exe exited within 3s with code 0 (0x0)"); };
  let holder = null;
  const raced = await ensureWorker(repo, { alive: () => holder, spawn: () => { holder = 777; return fastExit(); }, sleep: noSleep });
  check(raced.already && !raced.started && raced.pid === 777 && raced.message === "worker already running (pid 777)", `a worker that lost the lock race is already running, not a failed launch (${JSON.stringify(raced)})`);

  const unknown = await ensureWorker(repo, { alive: () => null, spawn: fastExit, sleep: noSleep });
  check(unknown.already && unknown.pid === null && unknown.message === workerRunningText(null), "a fast exit 0 with no holder visible yet is still not a failure");

  let threw = "";
  try {
    await ensureWorker(repo, { alive: () => null, spawn: () => { throw new Error("quiet-launch failed: quiet-launch: node.exe exited within 3s with code 1 (0x1)"); }, sleep: noSleep });
  } catch (e) {
    threw = String(e.message);
  }
  check(/code 1/.test(threw), "a fast exit with a non-zero code is still a failed launch");

  const started = await ensureWorker(repo, { alive: () => null, spawn: () => 9001, sleep: noSleep });
  check(started.started && started.pid === 9001 && !started.already, "no worker and a clean spawn: started");
}

console.log(failed ? `ship-queue-selftest: ${failed} FAILED` : "ship-queue-selftest: all passed");
process.exit(failed ? 1 : 0);

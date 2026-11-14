// Proves the ship queue's rules (tools/lib/ship-queue.mjs, docs/worktrees.md "Ship queue") without git,
// ssh or a game: FIFO order, the parallel/overlap rule, the re-gate decision and the queue file.
// inputs: tools/lib/ship-queue.mjs, tools/prs.mjs, tools/ship-queue.mjs
// red-proof 2026-11-14 (ship-train), each broken alone: pickRunnable without the earlier waiting entries in `ahead`
// "b done: c starts, e still waits behind c" (e jumped c); regateDecision without the generated check "main changed
// the test registry and the PR was re-gated"; overlap() counting driver-merged files "two PRs that only both append
// to docs/features.md ran in parallel (abd start)".

import { mkdtempSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import * as sq from "../lib/ship-queue.mjs";

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
add("a", ["tools/a.mjs", "docs/features.md"], "2026-11-14T10:00:00Z");
add("b", ["addons/LifeFramework/Scripts/Game/Feature/Bank/LF_Bank.c", "docs/features.md"], "2026-11-14T10:01:00Z");
add("c", ["addons/LifeFramework/Scripts/Game/Feature/Bank/LF_Bank.c"], "2026-11-14T10:02:00Z");
add("d", ["docs/worktrees.md", "docs/features.md"], "2026-11-14T10:03:00Z");
check(sq.ordered(q.entries).map((e) => e.slug).join("") === "abcd", "queued entries come out in arrival order");
add("a", ["tools/a.mjs", "tools/a2.mjs"], "2026-11-14T10:09:00Z");
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

// 4. Queue file round trip; finished entries are capped; a running slug cannot be enqueued again.
const dir = mkdtempSync(join(tmpdir(), "lf-shipq-"));
try {
  q.entries.find((e) => e.slug === "a").state = "merging";
  let threw = false;
  try {
    add("a", ["tools/a.mjs"]);
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

console.log(failed ? `ship-queue-selftest: ${failed} FAILED` : "ship-queue-selftest: all passed");
process.exit(failed ? 1 : 0);

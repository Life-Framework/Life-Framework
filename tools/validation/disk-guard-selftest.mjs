// Proves the disk guard and the idle check behind `lead prune-safe` (tools/lib/disk-guard.mjs, tools/lib/lead.mjs, wt.mjs):
//   1. bookkeeping is not activity: a worktree whose only recent change is tmp/wt.json (the marker `wt list` rewrites)
//      is idle and prunable; a touch in tmp/logs (or a server log, or the reflog) keeps it;
//   2. writeMarker leaves the file (and its mtime) alone when the content is unchanged, so `wt list` no longer
//      touches every worktree, and still writes on a new marker or a changed priority;
//   3. the thresholds: `lead` warns under 30 GB, `wt new` refuses under 10 GB (exactly at the limit is fine, an
//      unknown value never blocks, LF_WT_MIN_FREE_GB=0 turns the refusal off), `wt new` stops before touching anything;
//   4. the DISK block lists this PC and each machine and warns under the threshold; a pool machine's free disk parses;
//   5. `prune-safe --oldest N` parses and takes the N least recently active.
// inputs: tools/**
// red-proof: 2026-11-17, with newestActivityMs counting every file (the bookkeeping filter removed) the "wt.json touch is
// idle" checks failed; with writeMarker writing unconditionally the "unchanged marker keeps its mtime" check failed;
// with the refusal threshold set to 5 the "9.9 GB refuses" checks failed; restored.
import { existsSync, mkdirSync, mkdtempSync, readFileSync, rmSync, statSync, utimesSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import * as wt from "../wt.mjs";
import { classifyWorktree, isBookkeeping, newestActivityMs, IDLE_MS } from "../lib/lead.mjs";
import { diskLines, diskVerdict, oldestFirst, parseOldest, refuseMessage } from "../lib/disk-guard.mjs";
import { parseRemoteState } from "../remote/pool.mjs";

const failures = [];
const ok = (c, m) => { if (!c) failures.push(m); };
const eq = (got, want, m) => { if (JSON.stringify(got) !== JSON.stringify(want)) failures.push(`${m}: ${JSON.stringify(got)} != ${JSON.stringify(want)}`); };

const NOW = Date.parse("2026-11-17T12:00:00Z");
const MIN = 60000;
const merged = [{ number: 10, headRefName: "ws/done", headRefOid: "aaa", baseRefName: "main" }];
const tree = (lastMs) => ({ slug: "done", branch: "ws/done", base: "main", tip: "aaa", exists: true, isGit: true, dirty: false, busy: null, lastMs });
const cls = (lastMs) => classifyWorktree(tree(lastMs), merged, NOW).cls;

// --- 1. bookkeeping versus activity
const old = NOW - 300 * MIN;
const fresh = NOW - 1 * MIN;
ok(isBookkeeping("C:\\w\\tmp\\wt.json"), "tmp/wt.json is bookkeeping");
ok(isBookkeeping("/w/tmp/wt.json.tmp"), "its temp file is bookkeeping");
ok(isBookkeeping("/w/tmp/wt.lock"), "a lock file is bookkeeping");
ok(!isBookkeeping("/w/tmp/logs"), "tmp/logs is activity");
ok(!isBookkeeping("/w/tmp/pr-body-x.md"), "a PR body is activity");
const idleOnlyMarker = newestActivityMs(old, [{ path: "/w/tmp/wt.json", mtimeMs: fresh }]);
eq(idleOnlyMarker, old, "a fresh wt.json alone does not move the last activity");
eq(cls(newestActivityMs(old, [{ path: "/w/tmp/wt.json", mtimeMs: fresh }, { path: "/w/tmp/logs", mtimeMs: old }])), "prunable", "merged, clean, only wt.json touched a minute ago: prunable");
eq(cls(newestActivityMs(old, [{ path: "/w/tmp/wt.json", mtimeMs: fresh }])), "prunable", "...also with no other tmp file");
eq(cls(newestActivityMs(old, [{ path: "/w/tmp/wt.json", mtimeMs: fresh }, { path: "/w/tmp/logs", mtimeMs: fresh }])), "merged-kept", "a tmp/logs touch a minute ago keeps it");
eq(cls(newestActivityMs(old, [{ path: "/w/tmp/logs/validate.log", mtimeMs: fresh }])), "merged-kept", "a file inside tmp/logs keeps it");
eq(cls(newestActivityMs(old, [{ path: "/w/server/profile/x/logs/console.log", mtimeMs: fresh }])), "merged-kept", "a server log touch keeps it");
eq(cls(newestActivityMs(fresh, [{ path: "/w/tmp/wt.json", mtimeMs: old }])), "merged-kept", "a fresh reflog entry keeps it");
eq(cls(newestActivityMs(NaN, [])), "prunable", "no reflog and no files: idle");
eq(cls(newestActivityMs(old, [{ path: "/w/tmp/wt.json", mtimeMs: NOW - IDLE_MS + MIN }])), "prunable", "bookkeeping inside the idle window still never counts");

// --- 2. writeMarker only writes when the content changes
const scratch = mkdtempSync(join(tmpdir(), "lf-disk-guard-"));
try {
  const wtDir = join(scratch, "w");
  mkdirSync(wtDir, { recursive: true });
  ok(wt.writeMarker(wtDir, "alpha") === true, "a new marker is written");
  const p = wt.markerPath(wtDir);
  eq(readFileSync(p, "utf8"), '{"slug":"alpha"}\n', "marker content");
  const past = new Date(NOW - 5 * 3600000);
  utimesSync(p, past, past);
  ok(wt.writeMarker(wtDir, "alpha") === false, "an unchanged marker reports no write");
  ok(Math.abs(statSync(p).mtimeMs - past.getTime()) < 1000, "an unchanged marker keeps its mtime (wt list stops touching every worktree)");
  ok(wt.writeMarker(wtDir, "alpha", "low") === true, "a changed priority is written");
  eq(readFileSync(p, "utf8"), '{"slug":"alpha","priority":"low"}\n', "priority written");
  utimesSync(p, past, past);
  ok(wt.writeMarker(wtDir, "alpha") === false, "the stored priority is kept across a plain rewrite, unchanged");
  ok(wt.writeMarker(wtDir, "beta") === true, "a different slug is written");
  ok(wt.readMarker(wtDir)?.slug === "beta", "slug updated");
} finally {
  rmSync(scratch, { recursive: true, force: true });
}

// --- 3. thresholds (defaults: warn 30, refuse 10)
const saved = { w: process.env.LF_DISK_WARN_GB, r: process.env.LF_WT_MIN_FREE_GB };
delete process.env.LF_DISK_WARN_GB;
delete process.env.LF_WT_MIN_FREE_GB;
try {
  eq(diskVerdict(9.9).level, "refuse", "9.9 GB refuses");
  eq(diskVerdict(10).level, "warn", "exactly 10 GB does not refuse (it warns)");
  eq(diskVerdict(29.9).level, "warn", "29.9 GB warns");
  eq(diskVerdict(30).level, "ok", "exactly 30 GB is fine");
  eq(diskVerdict(250).level, "ok", "plenty is fine");
  eq(diskVerdict(null).level, "unknown", "unknown is not a verdict");
  eq(diskVerdict(0.19).level, "refuse", "the 2026-11-17 case (195 MB) refuses");
  ok(/refused: 9\.9 GB free/.test(refuseMessage(9.9) ?? ""), "the refusal message names the free space");
  ok(/prune-safe --oldest/.test(refuseMessage(9.9) ?? ""), "...and the way out");
  eq(refuseMessage(10), null, "10 GB: no refusal message");
  eq(refuseMessage(null), null, "unknown free space never blocks `wt new`");
  process.env.LF_WT_MIN_FREE_GB = "0";
  eq(refuseMessage(1), null, "LF_WT_MIN_FREE_GB=0 turns the refusal off");
  process.env.LF_WT_MIN_FREE_GB = "20";
  eq(diskVerdict(15).level, "refuse", "LF_WT_MIN_FREE_GB moves the floor");
  process.env.LF_WT_MIN_FREE_GB = "junk";
  eq(diskVerdict(9).level, "refuse", "a junk override falls back to the default");
  delete process.env.LF_WT_MIN_FREE_GB;

  // `wt new` stops before it touches git or the disk.
  const repoRoot = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
  let msg = "";
  try { wt.wtNew(repoRoot, "disk-guard-selftest-slug", { freeGb: () => 5 }); } catch (e) { msg = e.message; }
  ok(/wt new refused: 5\.0 GB free/.test(msg), `wt new refuses with 5 GB free, got '${msg.slice(0, 90)}'`);
  const strayDir = join(dirname(repoRoot), "Life-Framework-ws-disk-guard-selftest-slug");
  ok(!existsSync(strayDir), "a refused wt new created no folder");
  if (existsSync(strayDir)) {
    try { wt.wtPrune(repoRoot, "disk-guard-selftest-slug"); } catch { /* the failure above is the report; prune it by hand */ }
  }

  // --- 4. the DISK block
  const lines = diskLines([
    { name: "this PC", freeGb: 142.3 },
    { name: "lf-testbox", freeGb: 22.5 },
    { name: "lf-cloud", freeGb: 4 },
    { name: "lf-laptop", freeGb: null, note: "DOWN" },
  ]);
  eq(lines[0], "DISK", "block header");
  ok(lines.some((l) => /this PC\s+142 GB free$/.test(l)), "this PC row");
  ok(lines.some((l) => /lf-laptop\s+\? GB free \(DOWN\)/.test(l)), "an unreadable machine shows ? and why");
  ok(lines.some((l) => /WARN DISK: lf-testbox has 22\.5 GB free, under 30 GB$/.test(l)), "22.5 GB warns");
  ok(lines.some((l) => /WARN DISK: lf-cloud has 4\.0 GB free, under 10 GB \(wt new refuses\)/.test(l)), "4 GB warns and says wt new refuses");
  ok(!lines.some((l) => /WARN DISK: this PC/.test(l)), "142 GB does not warn");
  ok(!lines.some((l) => /WARN DISK: lf-laptop/.test(l)), "an unknown value does not warn");
  eq(diskLines([{ name: "this PC", freeGb: 40 }]).length, 2, "a healthy machine adds no warning");
  const pc = diskLines([{ name: "this PC", freeGb: 12 }]);
  ok(pc.some((l) => /WARN DISK: this PC.*lead prune-safe --oldest 20/.test(l)), "this PC's warning names the fix");
} finally {
  for (const [k, v] of [["LF_DISK_WARN_GB", saved.w], ["LF_WT_MIN_FREE_GB", saved.r]]) {
    if (v === undefined) delete process.env[k];
    else process.env[k] = v;
  }
}
eq(parseRemoteState("lf-pool: disk 18.4\nlf-pool: ram 4096 15564\n").diskGb, 18.4, "a pool machine's free disk parses");
eq(parseRemoteState("lf-pool: ram 4096 15564\n").diskGb, null, "an older box with no disk line: null");

// --- 5. prune-safe --oldest
eq(parseOldest(["--dry-run"]), null, "no --oldest: no cap");
eq(parseOldest(["--oldest", "20"]), 20, "--oldest 20");
eq(parseOldest(["--dry-run", "--oldest", "3"]), 3, "--oldest after another flag");
for (const bad of [["--oldest"], ["--oldest", "0"], ["--oldest", "-2"], ["--oldest", "abc"], ["--oldest", "--dry-run"]]) {
  ok(Number.isNaN(parseOldest(bad)), `--oldest ${bad[1] ?? "(none)"} is rejected`);
}
const rows = [{ slug: "c", lastMs: 300 }, { slug: "a", lastMs: 100 }, { slug: "b", lastMs: 200 }, { slug: "d", lastMs: 100 }];
eq(oldestFirst(rows, 2).map((r) => r.slug), ["a", "d"], "the 2 oldest, ties by slug");
eq(oldestFirst(rows, null).map((r) => r.slug), ["a", "d", "b", "c"], "no cap: all, oldest first");
eq(oldestFirst(rows, 99).length, 4, "a cap over the count takes all");
eq(oldestFirst([], 5), [], "nothing qualifies, nothing is taken");

if (failures.length) {
  for (const f of failures) console.log(`FAIL disk-guard: ${f}`);
  process.exit(1);
}
console.log("disk-guard selftest: ok");

// Proves the compiled-ok pin (tools/lib/guard-pin.mjs) that `world sync` and `wt new` use: the newest checked
// commit wins, an unchecked head is never picked, green/red are not overwritten by compiled-ok, the GitHub status
// mapping, and on a throwaway git repo `world-sync` pulls only up to the pinned commit, never the unchecked head.
// inputs: tools/lib/guard-pin.mjs, tools/lib/world-sync.mjs, tools/commands/world-sync.mjs, tools/wt.mjs
// red-proof 2026-11-16: before tools/lib/guard-pin.mjs existed this printed "FAIL cannot load"; with syncWorktree
// ignoring `target` it printed "FAIL world sync stops at the compiled-ok commit".

// red-proof 2026-11-22 (lease contexts): with writeLeaseChecked's cap fallback removed this printed "FAIL a full context moves on to -2";
//   with isLeaseCtx reading only the legacy context "FAIL leaseFromStatuses: newest lease context wins".
import { gitSync } from "../lib/git-retry.mjs";
import { readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { scratchDir } from "../lib/scratch.mjs";

let failed = 0;
const check = (c, m) => { if (!c) { console.log(`FAIL  ${m}`); failed++; } };

let pin, ws;
try {
  pin = await import("../lib/guard-pin.mjs");
  ws = await import("../lib/world-sync.mjs");
} catch (e) {
  console.log(`FAIL  cannot load: ${e.message}`);
  process.exit(1);
}

// 1. Pure pick.
{
  const st = { h3: null, h2: "red", h1: "compiled-ok", h0: "green" };
  const p = pin.pickChecked(["h3", "h2", "h1", "h0"], (s) => st[s]);
  check(p?.sha === "h1" && p.behind === 2 && p.head === "h3", `pick: newest compiled-ok h1, 2 behind; got ${JSON.stringify(p)}`);
  check(pin.pickChecked(["h1", "h0"], () => null) === null, "pick: nothing checked -> null");
  const post = (st, step) => pin.guardToGh(st, step).map((g, i) => ({ ...g, created_at: `2026-11-16T00:00:0${i}Z` }));
  check(pin.guardFromStatuses(post("compiled-ok")) === "compiled-ok" && post("compiled-ok")[0].context === "main-guard/compile", "compiled-ok -> main-guard/compile success");
  check(pin.guardFromStatuses([...post("compiled-ok"), ...post("green").map((g) => ({ ...g, created_at: "2026-11-16T01:00:00Z" }))]) === "green", "compile + suite success -> green");
  check(pin.guardFromStatuses([...post("compiled-ok"), ...post("red", "suite").map((g) => ({ ...g, created_at: "2026-11-16T01:00:00Z" }))]) === "red", "suite failure -> red");
  check(pin.guardFromStatuses(post("red", "compile")) === "red" && post("red", "compile")[0].context === "main-guard/compile", "compile failure -> red on main-guard/compile");
  check(pin.guardFromStatuses([{ context: "main-guard", state: "pending", description: "compiled-ok: x" }]) === "compiled-ok" && pin.guardFromStatuses([{ context: "validate", state: "success" }]) === null, "legacy context read; other contexts ignored");
  // GitHub is the truth, the hub file a cache: a gh answer wins over the cache; the cache only when gh fails.
  // (resolveChecked is exercised against real repos below.)
  const L = pin.leaseDecision;
  const fresh = { state: "held", host: "LEAD-PC", pid: 9, at: new Date().toISOString() };
  check(!L(fresh, "LAPTOP").ok && /LEAD-PC/.test(L(fresh, "LAPTOP").why), "lease: a second machine refuses while another holds it");
  check(L(fresh, "LEAD-PC").ok, "lease: the holder may renew");
  check(L({ ...fresh, at: new Date(Date.now() - pin.LEASE_TTL_MS - 1000).toISOString() }, "LAPTOP").ok, "lease: a stale lease can be taken");
  check(L({ state: "released", host: "LEAD-PC" }, "LAPTOP").ok && L(null, "LAPTOP").ok, "lease: released or none is free");
  check(pin.parseLease({ description: "held LAPTOP 123 2026-11-16T00:00:00.000Z" })?.pid === 123, "lease description parses");
}

// 1b. The lease lives in a context of the day (2026-11-22: GitHub allows 1000 statuses per commit and context; the one
// `main-guard/lease` context filled on 2026-11-20, every renewal after it failed in silence and the lease sat 65 h old
// under a live worker).
{
  const T = Date.parse("2026-11-22T10:00:00Z");
  check(pin.leaseContextFor(T) === "main-guard/lease/20261122" && pin.leaseContextFor(T, 2) === "main-guard/lease/20261122-2", `leaseContextFor: ${pin.leaseContextFor(T)}`);
  check(pin.isLeaseCtx("main-guard/lease") && pin.isLeaseCtx("main-guard/lease/20261122") && !pin.isLeaseCtx("main-guard/leaseholder") && !pin.isLeaseCtx("main-guard/suite"), "isLeaseCtx: the legacy context and the ones under it only");
  // A reader takes the newest status of any lease context: the dated one beats the full legacy one.
  const list = [
    { context: "main-guard/lease/20261122", description: "held PC1 111 2026-11-22T09:55:00.000Z", created_at: "2026-11-22T09:55:00Z" },
    { context: "main-guard/lease", description: "held PC1 99 2026-11-20T00:47:19.334Z", created_at: "2026-11-20T00:47:19Z" },
    { context: "lf-probe-test", description: "probe", created_at: "2026-11-22T09:59:00Z" },
  ];
  const got = pin.leaseFromStatuses(list);
  check(got?.pid === 111 && got.at === "2026-11-22T09:55:00.000Z", `leaseFromStatuses: newest lease context wins, got ${JSON.stringify(got)}`);
  check(pin.leaseFromStatuses([list[1]])?.pid === 99, "leaseFromStatuses: a legacy-only list still reads");
  check(pin.leaseFromStatuses([list[2]]) === null && pin.leaseFromStatuses([]) === null, "leaseFromStatuses: no lease status is null");
  // writeLeaseChecked: lands in the dated context; a full context moves on to `-2`; another failure is returned, not swallowed.
  const posts = [];
  const mk = (rule) => (dir, root, ctx, state, description) => { posts.push({ ctx, state, description }); return rule(ctx); };
  const ok = pin.writeLeaseChecked("d", "held", "PC1", 7, { now: T, root: "r", post: mk(() => ({ status: 0 })) });
  check(ok.ok && ok.ctx === "main-guard/lease/20261122" && posts.length === 1 && posts[0].state === "held" && /^held PC1 7 2026-11-22T10:00:00/.test(posts[0].description), `writeLeaseChecked ok: ${JSON.stringify(ok)}`);
  posts.length = 0;
  const cap = { status: 1, stderr: "gh: Validation failed: This SHA and context has reached the maximum number of statuses. (Validation Failed)" };
  const moved = pin.writeLeaseChecked("d", "held", "PC1", 7, { now: T, root: "r", post: mk((ctx) => (ctx.endsWith("20261122") ? cap : { status: 0 })) });
  check(moved.ok && moved.ctx === "main-guard/lease/20261122-2" && posts.length === 2, `a full context moves on to -2: ${JSON.stringify(moved)}`);
  posts.length = 0;
  const bad = pin.writeLeaseChecked("d", "held", "PC1", 7, { now: T, root: "r", post: mk(() => ({ status: 1, stderr: "gh: HTTP 401 Bad credentials" })) });
  check(!bad.ok && /401/.test(bad.why) && posts.length === 1, `another failure is returned with its reason and not retried: ${JSON.stringify(bad)}`);
  const full = pin.writeLeaseChecked("d", "held", "PC1", 7, { now: T, root: "r", post: mk(() => cap) });
  check(!full.ok && /maximum number of statuses/.test(full.why), "every context full: a failure with GitHub's words");
  check(!pin.writeLeaseChecked("d", "held", "PC1", 7, { now: T, root: null, post: mk(() => ({ status: 0 })) }).ok, "no root commit: not ok");
  const rel = pin.writeLeaseChecked("d", "released", "PC1", 7, { now: T, root: "r", post: mk(() => ({ status: 0 })) });
  check(rel.ok && posts.at(-1).state === "released" && posts.at(-1).description === "released PC1", "released is posted as released");
}

const tmp = scratchDir("lf-guard-pin-");
const git = (dir, ...a) => {
  const r = gitSync(["-C", dir, "-c", "user.name=t", "-c", "user.email=t@t", ...a], { encoding: "utf8" });
  if (r.status !== 0) throw new Error(`git ${a.join(" ")}: ${r.stderr}`);
  return r.stdout.trim();
};
try {
  // 2. State file: green is not downgraded by a late compiled-ok.
  const hub = join(tmp, "hub");
  pin.recordHead(hub, "s1", "green");
  pin.recordHead(hub, "s1", "compiled-ok");
  pin.recordHead(hub, "s2", "compiled-ok");
  const heads = pin.readHeads(hub).heads;
  check(heads.s1.status === "green" && heads.s2.status === "compiled-ok", "state: green kept, compiled-ok recorded");
  check(/main-guard-state\.json$/.test(pin.statePathOf(hub)), "state file name");

  // 3. Real repos: origin main = A (compiled-ok) <- B (unchecked). The maintainer clone is at the root commit.
  const origin = join(tmp, "origin.git");
  gitSync(["init", "-q", "--bare", "-b", "main", origin]);
  const up = join(tmp, "up");
  gitSync(["clone", "-q", origin, up]);
  writeFileSync(join(up, "a.txt"), "0\n");
  git(up, "add", ".");
  git(up, "commit", "-qm", "root");
  git(up, "push", "-q", "origin", "HEAD:main");
  const me = join(tmp, "me");
  gitSync(["clone", "-q", origin, me]);
  git(me, "checkout", "-qb", "ws/town-maintainer-edits");
  writeFileSync(join(up, "a.txt"), "A\n");
  git(up, "commit", "-qam", "A");
  const A = git(up, "rev-parse", "HEAD");
  writeFileSync(join(up, "a.txt"), "B broken\n");
  git(up, "commit", "-qam", "B");
  git(up, "push", "-q", "origin", "HEAD:main");
  git(me, "fetch", "-q", "origin");
  pin.recordHead(hub, A, "compiled-ok");
  const B = git(up, "rev-parse", "HEAD");
  const viaGh = pin.resolveChecked(me, { hub, gh: (d, sha) => (sha === B ? "red" : sha === A ? "compiled-ok" : null) });
  check(viaGh?.sha === A, "gh answer used");
  pin.recordHead(hub, B, "compiled-ok");
  const ghWins = pin.resolveChecked(me, { hub, gh: (d, sha) => (sha === B ? "red" : sha === A ? "compiled-ok" : null) });
  check(ghWins?.sha === A, "gh red beats a stale compiled-ok in the cache");
  const cacheOnly = pin.resolveChecked(me, { hub, gh: () => undefined });
  check(cacheOnly?.sha === B, "gh unavailable: the cache answers");
  pin.recordHead(hub, B, "red");
  const p = pin.resolveChecked(me, { hub, gh: () => null });
  check(p?.sha === A && p.behind === 1, `resolveChecked: A, 1 behind; got ${JSON.stringify(p)}`);
  const r = ws.syncWorktree(me, { target: p?.sha, listProcesses: () => [] });
  check(r.status === "ok", `world sync ok, got ${r.status} ${r.message ?? ""}`);
  check(git(me, "rev-parse", "HEAD") === A && readFileSync(join(me, "a.txt"), "utf8").replace(/\r/g, "") === "A\n", "world sync stops at the compiled-ok commit");
  const cmd = readFileSync(new URL("../commands/world-sync.mjs", import.meta.url), "utf8");
  check(/behind the origin\/main head/.test(cmd) && /--unchecked/.test(cmd), "world-sync prints the pin and how far behind; --unchecked exists");
  const wtSrc = readFileSync(new URL("../wt.mjs", import.meta.url), "utf8");
  check(/resolvePin\(root\)/.test(wtSrc) && /\["branch", branch, start\]/.test(wtSrc), "wt new bases on the pinned commit");
} finally {
  rmSync(tmp, { recursive: true, force: true });
}

if (failed) process.exit(1);
console.log("PASS  guard-pin selftest");

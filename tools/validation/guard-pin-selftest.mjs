// Proves the compiled-ok pin (tools/lib/guard-pin.mjs) that `world sync` and `wt new` use: the newest checked
// commit wins, an unchecked head is never picked, green/red are not overwritten by compiled-ok, the GitHub status
// mapping, and on a throwaway git repo `world-sync` pulls only up to the pinned commit, never the unchecked head.
// inputs: tools/lib/guard-pin.mjs, tools/lib/world-sync.mjs, tools/commands/world-sync.mjs, tools/wt.mjs
// red-proof 2026-11-16: before tools/lib/guard-pin.mjs existed this printed "FAIL cannot load"; with syncWorktree
// ignoring `target` it printed "FAIL world sync stops at the compiled-ok commit".

import { spawnSync } from "node:child_process";
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";

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

const tmp = mkdtempSync(join(tmpdir(), "lf-guard-pin-"));
const git = (dir, ...a) => {
  const r = spawnSync("git", ["-C", dir, "-c", "user.name=t", "-c", "user.email=t@t", ...a], { encoding: "utf8" });
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
  spawnSync("git", ["init", "-q", "--bare", "-b", "main", origin]);
  const up = join(tmp, "up");
  spawnSync("git", ["clone", "-q", origin, up]);
  writeFileSync(join(up, "a.txt"), "0\n");
  git(up, "add", ".");
  git(up, "commit", "-qm", "root");
  git(up, "push", "-q", "origin", "HEAD:main");
  const me = join(tmp, "me");
  spawnSync("git", ["clone", "-q", origin, me]);
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

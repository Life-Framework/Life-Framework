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
  check(pin.ghToGuard({ context: "main-guard", state: "pending", description: pin.guardToGh("compiled-ok").description }) === "compiled-ok", "gh status round trip compiled-ok");
  check(pin.ghToGuard({ context: "main-guard", state: "success" }) === "green" && pin.ghToGuard({ context: "validate", state: "success" }) === null, "gh status mapping");
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

// Proves the warm look server's job logic offline (tools/lib/look-job.mjs and its users): phase timing, the idle
// and starved-close rule, the RAM verdict on the pool's admission (a resident Workbench gives its memory back to
// its own restart, a full machine queues, the floor holds), the mirror decision, the in-place viewer write on a
// real temporary git repo (same file identity, git clean afterwards), the warm remote argv and --cold, a bad
// target failing before the queue, the box's resident-server state line, and the pool's reap of it.
// inputs: tools/commands/evidence.mjs, tools/lib/look-job.mjs, tools/lib/pool-ram.mjs, tools/lib/wb-gate.mjs, tools/visual/look.mjs, tools/visual/look-serve.mjs, tools/remote/pool.mjs, tools/validation/look-job-selftest.mjs
// red-proof: 2026-11-16, idleVerdict ignoring `starved` failed "a starved machine closes the resident Workbench
// at once"; mirrorViewer using `git checkout` instead of an in-place write failed "the viewer file keeps its identity";
// lookStartVerdict not adding the resident's working set failed "a restart gets the resident's memory back".
// 2026-11-17: mirrorMode without the REOPEN_SAFE_EXT check failed "a modified addons/A/Prefabs/Shop.et restarts even
// when listed for reopen"; renderProblem returning null always failed "another tree rendered: refused".

import { spawnSync } from "node:child_process";
import { mkdirSync, mkdtempSync, statSync, writeFileSync, readFileSync, existsSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { idleVerdict, lookStartVerdict, mirrorMode, phaseText, renderProblem, requestNeedsRam, samePairProblem, timeline, LOOK_IDLE_MIN } from "../lib/look-job.mjs";
import { parseWbState } from "../lib/wb-gate.mjs";
import { parseArgs, remoteLookArgs, targetProblem } from "../visual/look.mjs";
import { classifyChange, mirrorViewer, viewerAt } from "../visual/look-serve.mjs";
import { reapRemoteLookServer } from "../remote/pool.mjs";

const failures = [];
const check = (name, ok) => {
  if (!ok) failures.push(name);
};

// ---- phases
const t = timeline();
t.add("queue", 217).add("sync", 4.7).add("queue", 3);
check("phases sum a repeated phase and keep order", JSON.stringify(t.get()) === JSON.stringify({ queue: 220, sync: 4.7 }));
check("phaseText names every phase", phaseText({ a: 1, b: 2.25 }) === "a 1.0 s, b 2.3 s" || phaseText({ a: 1, b: 2.25 }) === "a 1.0 s, b 2.2 s");

// ---- idle and starved
const now = 10_000_000;
check("a used server stays", !idleVerdict({ lastUsedMs: now - 60_000, now }).close);
check("idle past the limit closes", idleVerdict({ lastUsedMs: now - (LOOK_IDLE_MIN * 60000 + 1), now }).close);
check("a starved machine closes the resident Workbench at once", idleVerdict({ lastUsedMs: now - 5000, now, starved: true }).close);
check("a request in flight is never closed, starved or not", !idleVerdict({ lastUsedMs: 0, now, starved: true, busy: true }).close);
check("the idle limit is about 5 minutes", LOOK_IDLE_MIN === 5);

// ---- RAM on the pool's admission
const settings = { minFreeMb: 1500 };
const procsWb = [{ kind: "workbench", pid: 99, wsMb: 3600, world: "small", privMb: 3700 }];
check("a full machine queues the look Workbench", !lookStartVerdict({ state: { freeMb: 3000, procs: [] }, settings }).ok);
check("room for the peak and the floor admits it", lookStartVerdict({ state: { freeMb: 3700 + 1500, procs: [] }, settings }).ok);
check("a restart gets the resident's memory back", lookStartVerdict({ state: { freeMb: 1600, procs: procsWb }, settings, residentPid: 99 }).ok);
check("without it counted back the same machine queues", !lookStartVerdict({ state: { freeMb: 1600, procs: procsWb }, settings, residentPid: null }).ok);
check("a Workbench still loading counts for its growth", !lookStartVerdict({ state: { freeMb: 5500, procs: [{ kind: "workbench", pid: 7, wsMb: 1000, world: "small", privMb: 1000 }] }, settings }).ok);
check("a request to a live server needs no RAM", requestNeedsRam(true) === false && requestNeedsRam(false) === true);

// ---- the mirror decision
const M = (status, path) => ({ status, path });
check("same files: shoot", mirrorMode({ changes: [] }).reload === "none");
check("no server: restart", mirrorMode({ changes: [], hasServer: false }).reload === "restart");
check("a changed look plugin restarts", mirrorMode({ changes: [], pluginChanged: true }).reload === "restart");
check("docs only: reopen", mirrorMode({ changes: [M("M", "docs/x.md")] }).reload === "reopen");
check("a modified .layer restarts by default (nothing proven)", mirrorMode({ changes: [M("M", "addons/A/W.layer")] }).reload === "restart");
check("a modified .layer reopens when its extension is proven", mirrorMode({ changes: [M("M", "addons/A/W.layer")], reopenExt: [".layer"] }).reload === "reopen");
check("an added .layer always restarts", mirrorMode({ changes: [M("A", "addons/A/W.layer")], reopenExt: [".layer"] }).reload === "restart");
// A prefab, material or config is a resource a live Workbench keeps across a world reopen: listing it in
// LF_LOOK_REOPEN_EXT must not let the warm server shoot the old one (#2364 suspicion, 2026-11-17).
for (const p of ["addons/A/Prefabs/Shop.et", "addons/A/M.emat", "addons/A/C.conf"]) {
  check(`a modified ${p} restarts even when listed for reopen`, mirrorMode({ changes: [M("M", p)], reopenExt: [".layer", ".et", ".emat", ".conf"] }).reload === "restart");
  check(`the server classifier restarts on a modified ${p} even when listed`, classifyChange({ from: "a", to: "b", changes: [M("M", p)], reopenExt: [".et", ".emat", ".conf"] }).reload === "restart");
}
// ---- what a picture shows (index.json inputs)
check("the requested tree rendered: trusted", renderProblem({ expected: "abc123", rendered: "abc123" }) === null);
check("another tree rendered: refused", /rendered addons tree def456/.test(renderProblem({ expected: "abc123", rendered: "def456" }) || ""));
check("no rendered hash: refused", renderProblem({ expected: "abc123", rendered: null }) !== null);
check("a before and after of the same addon files is refused", /same addon files/.test(samePairProblem("t1", "t1") || ""));
check("a before and after of different files passes", samePairProblem("t1", "t2") === null);
check("a picture with no recorded tree is not judged", samePairProblem(null, "t1") === null && samePairProblem("t1", null) === null);
check("a script restarts even when .layer is proven", mirrorMode({ changes: [M("M", "addons/A/S.c")], reopenExt: [".layer"] }).reload === "restart");
check("classifyChange delegates the same rule", classifyChange({ from: "a", to: "b", changes: [M("M", "addons/A/W.layer")], reopenExt: [".layer"] }).reload === "reopen" && classifyChange({ from: "a", to: "b", changes: [M("D", "addons/A/W.layer")], reopenExt: [".layer"] }).reload === "restart");

// ---- the in-place viewer write, on a real repo
const git = (cwd, ...a) => spawnSync("git", ["-C", cwd, "-c", "user.name=t", "-c", "user.email=t@t", ...a], { encoding: "utf8" });
const tmp = mkdtempSync(join(tmpdir(), "lf-look-job-"));
const repo = join(tmp, "repo");
const viewer = join(tmp, "viewer");
mkdirSync(join(repo, "addons", "A"), { recursive: true });
git(tmp, "init", "-q", repo);
git(repo, "config", "core.autocrlf", "false"); // the repo itself says eol=lf; a temp repo must not inherit the machine's CRLF setting
writeFileSync(join(repo, "addons", "A", "W.layer"), "one\n");
writeFileSync(join(repo, "doc.md"), "d1\n");
git(repo, "add", "-A");
git(repo, "commit", "-q", "-m", "c1");
const c1 = git(repo, "rev-parse", "HEAD").stdout.trim();
writeFileSync(join(repo, "addons", "A", "W.layer"), "two, longer\n");
writeFileSync(join(repo, "doc.md"), "d2\n");
git(repo, "commit", "-q", "-am", "c2");
const c2 = git(repo, "rev-parse", "HEAD").stdout.trim();
git(repo, "worktree", "add", "--detach", viewer, c1);
const f = join(viewer, "addons", "A", "W.layer");
const idBefore = statSync(f, { bigint: true }).ino;
const mir = mirrorViewer(repo, viewer, c1, c2, [M("M", "addons/A/W.layer"), M("M", "doc.md")]);
check("the mirror reports ok", mir.ok === true);
check("the viewer file has the new content", readFileSync(f, "utf8") === "two, longer\n" && readFileSync(join(viewer, "doc.md"), "utf8") === "d2\n");
check("the viewer file keeps its identity (written in place)", statSync(f, { bigint: true }).ino === idBefore);
check("git sees the viewer at c2 and clean", viewerAt(viewer, c2).ok);
check("an addon file that was added cannot be mirrored in place", mirrorViewer(repo, viewer, c2, c1, [M("A", "addons/A/N.layer")]).ok === false);
check("the viewer is still c2 after that refusal", viewerAt(viewer, c2).ok);

// ---- argv, flags, targets
const warm = remoteLookArgs(["grid:x", "--hour", "9"], "tmp/o", { warm: true });
check("a routed look is warm on the box (--serve)", warm.includes("--serve") && warm.includes("--local"));
check("--cold stays cold", !remoteLookArgs(["grid:x", "--cold"], "tmp/o", { warm: true }).includes("--serve"));
check("--fresh stays cold", !remoteLookArgs(["grid:x", "--fresh"], "tmp/o", { warm: true }).includes("--serve"));
check("look item stays cold", !remoteLookArgs(["item", "a.et"], "tmp/o", { warm: true }).includes("--serve"));
check("the old two-argument form is unchanged (no --serve)", !remoteLookArgs(["bank"], "tmp/o").includes("--serve"));
check("a misspelt grid cell fails before any queue", /no grid cell/.test(targetProblem(parseArgs(["grid:gunstore"])) ?? ""));
check("a real area resolves", targetProblem(parseArgs(["bank"])) === null);
check("look item is checked where its prefab resolves", targetProblem(parseArgs(["item", "x.et"])) === null);

// ---- the box's state line and the pool's reap
const st = parseWbState("lf-wb: 1 4200\nlf-look: 4321 95\n", 1);
check("the box's resident look server is read", st.lookServer?.pid === 4321 && st.lookServer?.idleS === 95 && st.wbAlive === 1);
check("no resident server, no field", parseWbState("lf-wb: 0 4200\n", 1).lookServer === undefined);
const stops = [];
const rd = (idleS, idleBeat = true) => () => ({ pid: 1, idleS, idleBeat, raw: "C:/raw" });
const stop = (m, row) => stops.push(row.pid);
check("the pool closes a resident Workbench idle past the limit", reapRemoteLookServer("r", { name: "b" }, { now, read: rd(400), stop }) !== null && stops.length === 1);
check("the pool leaves a fresh one alone", reapRemoteLookServer("r", { name: "b" }, { now, read: rd(30), stop }) === null && stops.length === 1);
check("a waiting job closes it at once", reapRemoteLookServer("r", { name: "b" }, { now, read: rd(30), stop, waiting: true }) !== null && stops.length === 2);
check("never while a request runs (no heartbeat)", reapRemoteLookServer("r", { name: "b" }, { now, read: rd(900, false), stop, waiting: true }) === null && stops.length === 2);
check("no server, nothing to close", reapRemoteLookServer("r", { name: "b" }, { now, read: () => null, stop }) === null);

if (failures.length) {
  for (const x of failures) console.log(`look-job-selftest FAIL: ${x}`);
  process.exit(1);
}
console.log("look-job-selftest: OK (phases, idle and starved close, RAM on pool admission, mirror decision and in-place viewer write, warm argv and --cold, target check, resident-server state, pool reap)");
void existsSync;

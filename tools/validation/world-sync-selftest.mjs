// tools/validation/world-sync-selftest.mjs
// inputs: tools/lib/world-sync.mjs, tools/commands/world-sync.mjs
//
// Proves tools\cli world-sync against throwaway git repos (a bare origin, a "maintainer" clone on ws/town and a
// second clone that moves main): no overlap with local edits (fast-forward and merge commit, staged + unstaged +
// untracked edits byte-identical afterwards), overlap on a .layer (held, the rest pulled, HEAD unmoved), a dirty
// file that is also an upstream rename (the pair is held; a non-layer one blocks), a dry run, the Workbench
// reload line, and failures part-way (after the working tree moved, after HEAD moved) that put the exact prior
// state back and leave no stash and no backup behind.
//
// red-proof: 2026-11-16, with planSync's layer branch disabled (a layer overlap blocks) this printed
// "FAIL layer overlap: status is 'blocked', want 'ok'"; with the restore step disabled this printed
// "FAIL failure after the working tree moved: restored=false". With the library absent:
// "FAIL cannot load tools/lib/world-sync.mjs or tools/commands/world-sync.mjs".

import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, mkdtempSync, readFileSync, readdirSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
let failed = 0;
const fail = (m) => { console.log(`FAIL  ${m}`); failed++; };
const check = (c, m) => { if (!c) fail(m); };

let lib, cmd;
try {
  lib = await import(pathToFileURL(join(ROOT, "tools", "lib", "world-sync.mjs")).href);
  cmd = await import(pathToFileURL(join(ROOT, "tools", "commands", "world-sync.mjs")).href);
} catch (e) {
  fail(`cannot load tools/lib/world-sync.mjs or tools/commands/world-sync.mjs: ${e.code ?? e.message}`);
  console.log(`SUMMARY world-sync-selftest fail ${failed}`);
  process.exit(1);
}

const GENV = { GIT_AUTHOR_NAME: "t", GIT_AUTHOR_EMAIL: "t@t.invalid", GIT_COMMITTER_NAME: "t", GIT_COMMITTER_EMAIL: "t@t.invalid", GIT_CONFIG_GLOBAL: "NUL", GIT_CONFIG_SYSTEM: "NUL" };
if (process.platform !== "win32") GENV.GIT_CONFIG_GLOBAL = "/dev/null";
// The library under test inherits this environment too: no user autocrlf, no identity from the real machine.
Object.assign(process.env, GENV);
function git(dir, ...args) {
  let r;
  // clone/push between local repos now and then fail on Windows when something else holds a file for a moment.
  for (let i = 0; i < 4; i++) {
    r = spawnSync("git", args, { cwd: dir, env: { ...process.env, ...GENV }, encoding: "utf8", windowsHide: true, maxBuffer: 1 << 28 });
    if (r.status === 0 || !["clone", "push"].includes(args[0])) break;
  }
  if (r.status !== 0) throw new Error(`git ${args.join(" ")} in ${dir}: ${r.stderr}`);
  return r.stdout.trim();
}
const put = (dir, rel, text) => { const p = join(dir, rel); mkdirSync(dirname(p), { recursive: true }); writeFileSync(p, text); };
const read = (dir, rel) => (existsSync(join(dir, rel)) ? readFileSync(join(dir, rel), "utf8") : null);

const LAYERS = "addons/LifeFramework/Worlds/DebugWorld/DebugWorld_Layers";
const SJ = `${LAYERS}/Street_Jobs.layer`;
const AL = `${LAYERS}/A.layer`;
const OT = `${LAYERS}/Other.layer`;

// A bare origin with main, a maintainer clone on ws/town, and a second clone ("up") that pushes to main.
function scene() {
  const tmp = mkdtempSync(join(tmpdir(), "lf-world-sync-"));
  const origin = join(tmp, "origin.git");
  const seed = join(tmp, "seed");
  git(tmp, "init", "-q", "--bare", "-b", "main", origin);
  git(tmp, "init", "-q", "-b", "main", seed);
  put(seed, SJ, "street jobs v1\n");
  put(seed, AL, "a layer v1\n");
  put(seed, OT, "other v1\n");
  put(seed, "docs/a.md", "doc a v1\n");
  put(seed, "docs/b.md", "doc b v1\n");
  put(seed, "notes.txt", "notes v1\n");
  put(seed, "scripts/x.c", "x v1\n");
  git(seed, "add", "-A"); git(seed, "commit", "-qm", "seed");
  git(seed, "remote", "add", "origin", origin); git(seed, "push", "-q", "origin", "main");
  const work = join(tmp, "Life-Framework-ws-town");
  git(tmp, "clone", "-q", origin, work);
  git(work, "checkout", "-q", "-b", "ws/town");
  const up = join(tmp, "up");
  git(tmp, "clone", "-q", origin, up);
  const push = (msg) => { git(up, "add", "-A"); git(up, "commit", "-qm", msg); git(up, "push", "-q", "origin", "main"); };
  return { tmp, origin, work, up, push, done: () => rmSync(tmp, { recursive: true, force: true }) };
}

const snapshot = (dir) => ({
  head: git(dir, "rev-parse", "HEAD"),
  status: git(dir, "status", "--porcelain=v1", "--untracked-files=all"),
  diff: git(dir, "diff", "--binary"),
  cached: git(dir, "diff", "--cached", "--binary"),
  stash: git(dir, "stash", "list"),
});
const noBackup = (dir) => !readdirSync(join(dir, ".git")).some((n) => n.startsWith("lf-world-sync-backup"));

// ---- 1a. no overlap, fast-forward, staged + unstaged + untracked edits survive
{
  const s = scene();
  put(s.work, SJ, "street jobs v1\nMAINTAINER EDIT\n");          // unstaged
  put(s.work, "docs/b.md", "doc b v1\nstaged edit\n"); git(s.work, "add", "docs/b.md"); // staged
  put(s.work, `${LAYERS}/New.layer`, "brand new layer\n");        // untracked
  const before = snapshot(s.work);
  put(s.up, "docs/a.md", "doc a v2\n"); put(s.up, "docs/new.md", "new doc\n");
  rmSync(join(s.up, "notes.txt")); put(s.up, `${LAYERS}/Mixed.layer`, "mixed\n");
  s.push("up 1");
  const r = lib.syncWorktree(s.work, { listProcesses: () => [] });
  check(r.status === "ok", `no overlap: status is '${r.status}', want 'ok' (${r.message ?? ""})`);
  check(r.pulled === 4 && r.held.length === 0, `no overlap: pulled=${r.pulled} held=${r.held.length}, want 4 and 0`);
  check(r.summary === "SUMMARY world-sync ok pulled=4 files held=0 reload-workbench=no", `no overlap: summary '${r.summary}'`);
  const after = snapshot(s.work);
  check(read(s.work, "docs/a.md") === "doc a v2\n" && read(s.work, "notes.txt") === null && read(s.work, `${LAYERS}/Mixed.layer`) === "mixed\n", "no overlap: upstream changes were not applied");
  check(read(s.work, SJ) === "street jobs v1\nMAINTAINER EDIT\n", "no overlap: the unstaged layer edit changed");
  check(after.diff === before.diff && after.cached === before.cached, "no overlap: local diff is not byte-identical");
  check(after.head === git(s.work, "rev-parse", "origin/main"), "no overlap: HEAD was not fast-forwarded");
  check(after.stash === "" && noBackup(s.work), "no overlap: a stash or backup was left behind");
  s.done();
}

// ---- 1b. no overlap, but the maintainer has a commit main lacks: a merge commit, edits still intact
{
  const s = scene();
  put(s.work, OT, "other v1\ncommitted by maintainer\n"); git(s.work, "add", "-A"); git(s.work, "commit", "-qm", "maint");
  put(s.work, SJ, "street jobs v1\nDIRTY\n");
  const before = snapshot(s.work);
  put(s.up, "docs/a.md", "doc a v2\n"); s.push("up");
  const r = lib.syncWorktree(s.work, { listProcesses: () => [] });
  check(r.status === "ok" && r.pulled === 1, `merge: status '${r.status}' pulled ${r.pulled} (${r.message ?? ""})`);
  const parents = git(s.work, "rev-list", "--parents", "-n", "1", "HEAD").split(" ").length - 1;
  check(parents === 2, `merge: HEAD has ${parents} parents, want a merge commit`);
  check(read(s.work, OT) === "other v1\ncommitted by maintainer\n" && read(s.work, "docs/a.md") === "doc a v2\n", "merge: file contents wrong");
  check(snapshot(s.work).diff === before.diff, "merge: dirty diff not byte-identical");
  const st = git(s.work, "status", "--porcelain").split("\n");
  check(st.length === 1 && st[0].endsWith("Street_Jobs.layer"), `merge: status is ${JSON.stringify(st)}`);
  s.done();
}

// ---- 2. overlap on a layer: held, everything else pulled, HEAD stays
{
  const s = scene();
  put(s.work, SJ, "street jobs v1\nMY HAND EDIT\n");
  const mine = read(s.work, SJ);
  const head0 = git(s.work, "rev-parse", "HEAD");
  put(s.up, SJ, "street jobs v2 from main\n"); put(s.up, "docs/a.md", "doc a v2\n"); s.push("up");
  const r = lib.syncWorktree(s.work, { listProcesses: () => [] });
  check(r.status === "ok", `layer overlap: status is '${r.status}', want 'ok' (${r.message ?? ""})`);
  check(read(s.work, SJ) === mine, "layer overlap: the held layer changed");
  check(read(s.work, "docs/a.md") === "doc a v2\n", "layer overlap: the other upstream file was not pulled");
  check(git(s.work, "rev-parse", "HEAD") === head0, "layer overlap: HEAD moved");
  check(r.held.length === 1 && r.pulled === 1, `layer overlap: held=${r.held.length} pulled=${r.pulled}`);
  check(r.summary === "SUMMARY world-sync ok pulled=1 files held=1 (Street_Jobs.layer) reload-workbench=no", `layer overlap: summary '${r.summary}'`);
  check(r.lines.some((l) => l.startsWith("held: you and main both changed") && l.includes("Street_Jobs.layer")), "layer overlap: no 'held: you and main both changed' line");
  // a second run is stable: still held, nothing more to pull
  const r2 = lib.syncWorktree(s.work, { listProcesses: () => [] });
  check(r2.status === "ok" && r2.pulled === 0 && r2.held.length === 1 && read(s.work, SJ) === mine, `layer overlap: rerun status '${r2.status}' pulled ${r2.pulled}`);
  s.done();
}

// ---- 3a. a dirty layer that main renamed: the pair is held, the rest pulled
{
  const s = scene();
  put(s.work, AL, "a layer v1\nMY EDIT\n");
  put(s.up, "docs/a.md", "doc a v2\n"); s.push("docs");
  git(s.up, "mv", AL, `${LAYERS}/B.layer`); s.push("rename");
  const r = lib.syncWorktree(s.work, { listProcesses: () => [] });
  check(r.status === "ok", `rename: status is '${r.status}' (${r.message ?? ""})`);
  check(read(s.work, AL) === "a layer v1\nMY EDIT\n", "rename: the dirty layer was touched");
  check(read(s.work, `${LAYERS}/B.layer`) === null, "rename: the renamed copy was pulled next to the held file");
  check(read(s.work, "docs/a.md") === "doc a v2\n", "rename: the rest was not pulled");
  check(r.held.length === 2, `rename: held ${r.held.length}, want 2 (old and new name)`);
  s.done();
}

// ---- 3b. a dirty non-layer file that main renamed: nothing changes, blocked
{
  const s = scene();
  put(s.work, "docs/a.md", "doc a v1\nMY EDIT\n");
  put(s.up, "docs/b.md", "doc b v2\n"); s.push("b");
  git(s.up, "mv", "docs/a.md", "docs/c.md"); s.push("rename");
  const before = snapshot(s.work);
  const r = lib.syncWorktree(s.work, { listProcesses: () => [] });
  check(r.status === "blocked" && r.code === 2, `blocked: status '${r.status}' code ${r.code}`);
  const after = snapshot(s.work);
  check(JSON.stringify(after) === JSON.stringify(before) && read(s.work, "docs/b.md") === "doc b v1\n", "blocked: the worktree changed");
  check(/^SUMMARY world-sync blocked /.test(r.summary), `blocked: summary '${r.summary}'`);
  s.done();
}

// ---- 4. dry run changes nothing
{
  const s = scene();
  put(s.work, SJ, "street jobs v1\nDRY\n");
  put(s.up, SJ, "v2\n"); put(s.up, "docs/a.md", "doc a v2\n"); s.push("up");
  const before = snapshot(s.work);
  const r = lib.syncWorktree(s.work, { dryRun: true, listProcesses: () => [] });
  check(r.status === "ok" && r.pulled === 1 && r.held.length === 1, `dry run: status ${r.status} pulled ${r.pulled} held ${r.held.length}`);
  check(JSON.stringify(snapshot(s.work)) === JSON.stringify(before) && read(s.work, "docs/a.md") === "doc a v1\n", "dry run: the worktree changed");
  check(/^SUMMARY world-sync ok dry-run pulled=1 /.test(r.summary), `dry run: summary '${r.summary}'`);
  s.done();
}

// ---- 5. Workbench running on this worktree: still syncs, says to reload; another project does not count
{
  const s = scene();
  put(s.up, "docs/a.md", "doc a v2\n"); s.push("up");
  const cmdline = `"C:\\Program Files\\Arma Reforger Tools\\Workbench\\ArmaReforgerWorkbench.exe" -gproj "${s.work}\\addons\\LifeFramework\\addon.gproj"`;
  const other = lib.workbenchProcessesFor(s.work, () => [`ArmaReforgerWorkbench.exe -gproj "${s.tmp}\\other-checkout\\addon.gproj"`]);
  check(other.length === 0, "workbench: a process on another path was counted");
  const r = lib.syncWorktree(s.work, { listProcesses: () => [cmdline.toUpperCase()] });
  check(r.status === "ok" && r.reload === true && /reload-workbench=yes$/.test(r.summary), `workbench: status ${r.status} reload ${r.reload} '${r.summary}'`);
  check(r.lines.some((l) => /reload the world in Workbench \(File > Reload, or reopen it\)/.test(l)), "workbench: no reload line");
  check(read(s.work, "docs/a.md") === "doc a v2\n", "workbench: upstream was not pulled");
  s.done();
}

// ---- 6. failure part-way restores the exact prior state (after the tree moved; after HEAD moved)
for (const [name, hook] of [["afterWorktree", "after the working tree moved"], ["afterHead", "after HEAD moved"]]) {
  const s = scene();
  put(s.work, SJ, "street jobs v1\nHAND\n");
  put(s.work, "docs/b.md", "doc b v1\nstaged\n"); git(s.work, "add", "docs/b.md");
  put(s.work, `${LAYERS}/New.layer`, "untracked\n");
  put(s.up, "docs/a.md", "doc a v2\n"); put(s.up, "scripts/x.c", "x v2\n"); s.push("up");
  const before = snapshot(s.work);
  const r = lib.syncWorktree(s.work, { listProcesses: () => [], hooks: { [name]: () => { throw new Error("injected failure"); } } });
  check(r.status === "error" && r.code === 1, `failure ${hook}: status '${r.status}' code ${r.code}`);
  check(r.restored === true, `failure ${hook}: restored=${r.restored}`);
  check(/injected failure/.test(r.message ?? ""), `failure ${hook}: message '${r.message}'`);
  const after = snapshot(s.work);
  check(JSON.stringify(after) === JSON.stringify(before), `failure ${hook}: Street_Jobs.layer ... not restored (state differs)`);
  check(read(s.work, "docs/a.md") === "doc a v1\n" && read(s.work, "scripts/x.c") === "x v1\n", `failure ${hook}: pulled files were left behind`);
  check(read(s.work, SJ) === "street jobs v1\nHAND\n" && read(s.work, `${LAYERS}/New.layer`) === "untracked\n", `failure ${hook}: the maintainer's files changed`);
  check(after.stash === "" && noBackup(s.work), `failure ${hook}: a stash or backup was left behind`);
  check(/^SUMMARY world-sync error /.test(r.summary), `failure ${hook}: summary '${r.summary}'`);
  s.done();
}

// ---- 7. guards: not a ws/ branch, a merge in progress, unreachable origin: errors with nothing changed
{
  const s = scene();
  put(s.up, "docs/a.md", "doc a v2\n"); s.push("up");
  git(s.work, "checkout", "-q", "main");
  const r = lib.syncWorktree(s.work, { listProcesses: () => [] });
  check(r.status === "error" && /not a ws\/ branch/.test(r.message), `guard: main branch gave '${r.status}' ${r.message}`);
  const f = lib.syncWorktree(s.work, { force: true, listProcesses: () => [] });
  check(f.status === "ok" && read(s.work, "docs/a.md") === "doc a v2\n", `guard: --force on main gave '${f.status}'`);
  git(s.work, "checkout", "-q", "ws/town");
  git(s.work, "remote", "set-url", "origin", join(s.tmp, "does-not-exist.git"));
  const before = snapshot(s.work);
  const g = lib.syncWorktree(s.work, { listProcesses: () => [] });
  check(g.status === "error" && /fetch failed/.test(g.message) && JSON.stringify(snapshot(s.work)) === JSON.stringify(before), `guard: bad origin gave '${g.status}' ${g.message}`);
  s.done();
}

// ---- 8. the CLI entry: arguments and the check-in hint
{
  const a = cmd.parseArgs(["--wt", "x", "--dry-run"]);
  check(a.slug === "x" && a.dryRun === true && a.force === false, "cli: --wt/--dry-run not parsed");
  check(cmd.parseArgs(["--bogus"]).error, "cli: an unknown argument was accepted");
  check(cmd.parseArgs(["--wt"]).error, "cli: --wt without a slug was accepted");
  const hint = lib.watchHint("town-maintainer-edits");
  check(/^tools\\cli world-sync --wt town-maintainer-edits\b/.test(hint), `cli: watch hint '${hint}'`);
  check(lib.planSync([{ status: "M", path: "a.layer" }], new Set(["a.layer"])).held.size === 1, "plan: a layer overlap is not held");
  check(lib.planSync([{ status: "M", path: "a.txt" }], new Set(["a.txt"])).blocked.length === 1, "plan: a non-layer overlap is not blocked");
}

console.log(failed ? `SUMMARY world-sync-selftest fail ${failed}` : "SUMMARY world-sync-selftest ok");
process.exit(failed ? 1 : 0);

// tools/lib/world-sync.mjs - keep the maintainer's world-editing worktree (ws/town-maintainer-edits)
// level with origin/main without ever losing or mangling his hand edits.
// docs/worktrees.md "Maintainer world edits".
//
// syncWorktree(dir, { dryRun, force }) -> { status, code, summary, lines, ... }
//   1. fetch origin <base>
//   2. upstream = files changed on origin/<base> since merge-base(HEAD, origin/<base>) (renames count as
//      two paths); local = uncommitted + staged + untracked + committed-not-on-<base>
//   3. no overlap: fast-forward, or a merge commit built from HEAD plus the upstream path states; the
//      working tree and index move with `git read-tree -m -u` (the engine under a fast-forward), which
//      carries every local change across and refuses on any file it cannot carry
//   4. overlap on a `.layer`: that file (and the other half of an upstream rename of it) is HELD and
//      stays exactly as it is; every other upstream change is applied to the working tree and index
//      only (no commit: history is never told those files were merged). Overlap on any other file:
//      blocked, nothing changes
//   5. a Workbench process whose command line names the worktree: still allowed, one line to reload
//   6. never reset --hard, --force, checkout -- or a stash. Before the first write the dirty files are
//      copied aside; on ANY failure the prior state is put back (HEAD, index, files) and the result is
//      an error. The result is checked against a fingerprint of the local changes.
// Exit codes (the CLI): 0 ok (held files still count as ok), 2 blocked (nothing changed), 1 error (state restored).

import { spawnSync } from "node:child_process";
import { createHash } from "node:crypto";
import { existsSync, lstatSync, mkdirSync, readFileSync, readlinkSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join, resolve, sep } from "node:path";
import { tmpdir } from "node:os";

const BIG = 1 << 30;

function runGit(dir, args, { env, input, allowFail = false, timeout = 600000 } = {}) {
  const r = spawnSync("git", args, {
    cwd: dir,
    env: { ...process.env, GIT_TERMINAL_PROMPT: "0", ...(env ?? {}) },
    input,
    maxBuffer: BIG,
    timeout,
    windowsHide: true,
  });
  if (r.error) throw new Error(`git ${args[0]}: ${r.error.message}`);
  if (r.status !== 0 && !allowFail) {
    throw new Error(`git ${args.join(" ")} failed (${r.status}): ${String(r.stderr).trim().split("\n").slice(-3).join(" | ")}`);
  }
  return { code: r.status, out: r.stdout, err: String(r.stderr) };
}
const gitText = (dir, args, o) => runGit(dir, args, o).out.toString("utf8").trim();
const sha256 = (buf) => createHash("sha256").update(buf).digest("hex");
const splitZ = (buf) => buf.toString("utf8").split("\0").filter((s) => s.length);

const isLayer = (p) => p.toLowerCase().endsWith(".layer");

// ---------------------------------------------------------------- git facts

export function parseNameStatusZ(buf) {
  // `diff --name-status -z -M`: "M\0path\0", "R100\0old\0new\0". Returns rows { status, path, from }.
  const f = splitZ(buf);
  const rows = [];
  for (let i = 0; i < f.length; ) {
    const st = f[i++];
    if (st[0] === "R" || st[0] === "C") rows.push({ status: st[0], from: f[i++], path: f[i++] });
    else rows.push({ status: st[0], path: f[i++] });
  }
  return rows;
}

export function parseStatusZ(buf) {
  // `status --porcelain=v1 -z`: "XY path\0", a rename/copy has the old path as the next field.
  const f = splitZ(buf);
  const paths = new Set();
  for (let i = 0; i < f.length; i++) {
    const rec = f[i];
    const x = rec[0];
    paths.add(rec.slice(3));
    if (x === "R" || x === "C") paths.add(f[++i]);
  }
  return paths;
}

function indexMap(dir) {
  const m = new Map();
  for (const rec of splitZ(runGit(dir, ["ls-files", "-s", "-z"]).out)) {
    const tab = rec.indexOf("\t");
    m.set(rec.slice(tab + 1), rec.slice(0, tab));
  }
  return m;
}

function fileFingerprint(dir, rel) {
  const full = join(dir, rel);
  let st;
  try {
    st = lstatSync(full);
  } catch {
    return "absent";
  }
  if (st.isSymbolicLink()) return `link:${readlinkSync(full)}`;
  if (!st.isFile()) return "other";
  return `file:${sha256(readFileSync(full))}`;
}

function gitDir(dir) {
  return resolve(dir, gitText(dir, ["rev-parse", "--git-dir"]));
}

function inProgress(dir) {
  const g = gitDir(dir);
  for (const n of ["MERGE_HEAD", "CHERRY_PICK_HEAD", "REVERT_HEAD", "rebase-merge", "rebase-apply", "BISECT_LOG"]) {
    if (existsSync(join(g, n))) return n;
  }
  return "";
}

// ---------------------------------------------------------------- Workbench

export function workbenchProcessesFor(dir, listProcesses = defaultProcessList) {
  const norm = (s) => String(s).replace(/\\/g, "/").replace(/\/+$/, "").toLowerCase();
  const want = norm(resolve(dir));
  return listProcesses().filter((cmd) => cmd && norm(cmd).includes(want));
}

function defaultProcessList() {
  // Command lines of Workbench processes. Failure to list is "none found": the reload line is advice, not a gate.
  try {
    if (process.platform === "win32") {
      const r = spawnSync(
        "powershell",
        ["-NoProfile", "-NonInteractive", "-Command", "Get-CimInstance Win32_Process | Where-Object { $_.Name -like 'ArmaReforgerWorkbench*' } | ForEach-Object { $_.CommandLine }"],
        { encoding: "utf8", windowsHide: true, timeout: 20000 },
      );
      return String(r.stdout ?? "").split(/\r?\n/).filter(Boolean);
    }
    const r = spawnSync("ps", ["-eo", "args"], { encoding: "utf8", timeout: 20000 });
    return String(r.stdout ?? "").split("\n").filter((l) => /workbench/i.test(l));
  } catch {
    return [];
  }
}

// ---------------------------------------------------------------- plan

// Pure: from the upstream rows, the local path set and a blob-equality test, decide pulled / held / blocked.
export function planSync(upRows, localPaths, { same = () => false } = {}) {
  const upPaths = new Set();
  const pairOf = new Map(); // upstream rename: old <-> new
  for (const r of upRows) {
    upPaths.add(r.path);
    if (r.from) {
      upPaths.add(r.from);
      pairOf.set(r.from, r.path);
      pairOf.set(r.path, r.from);
    }
  }
  const held = new Map(); // path -> reason
  const blocked = [];
  const already = new Set(); // local state already equals upstream (a rerun after a partial pull)
  for (const p of upPaths) {
    if (localPaths.has(p) && same(p)) { already.add(p); continue; }
    if (!localPaths.has(p)) continue;
    if (isLayer(p)) {
      held.set(p, `you and main both changed ${p}`);
      const mate = pairOf.get(p);
      if (mate && !held.has(mate)) held.set(mate, `main renamed ${p} to ${mate}; held with it so the world does not get two copies`);
    } else {
      blocked.push(p);
    }
  }
  // A held rename half drags in its mate even when the mate is not itself overlapping.
  for (const p of [...held.keys()]) {
    const mate = pairOf.get(p);
    if (mate && !held.has(mate)) held.set(mate, `held with ${p} (upstream rename)`);
  }
  const apply = [...upPaths].filter((p) => !held.has(p) && !already.has(p));
  return { upPaths, held, blocked, apply };
}

// ---------------------------------------------------------------- main

export function formatSummary(r) {
  const heldNames = [...new Set((r.held ?? []).map((p) => p.split("/").pop()))];
  const shown = heldNames.slice(0, 5).join(",") + (heldNames.length > 5 ? `,+${heldNames.length - 5}` : "");
  const held = `held=${heldNames.length}${heldNames.length ? ` (${shown})` : ""}`;
  const dry = r.dryRun ? " dry-run" : "";
  if (r.status === "error") return `SUMMARY world-sync error${dry} state-restored=${r.restored ? "yes" : "no"} ${String(r.message ?? "").replace(/\s+/g, " ").slice(0, 200)}`;
  if (r.status === "blocked") return `SUMMARY world-sync blocked${dry} pulled=0 files ${held} blocked=${r.blocked.length} (${r.blocked.slice(0, 3).map((p) => p.split("/").pop()).join(",")}) reload-workbench=no`;
  return `SUMMARY world-sync ok${dry} pulled=${r.pulled} files ${held} reload-workbench=${r.reload ? "yes" : "no"}`;
}

export function syncWorktree(dirIn, { dryRun = false, force = false, base = "main", log = () => {}, listProcesses, hooks = {} } = {}) {
  const dir = resolve(dirIn);
  const lines = [];
  const say = (s) => { lines.push(s); log(s); };
  const result = (o) => {
    const r = { dryRun, lines, held: [], blocked: [], pulled: 0, reload: false, restored: false, ...o };
    r.summary = formatSummary(r);
    r.code = r.status === "ok" ? 0 : r.status === "blocked" ? 2 : 1;
    return r;
  };
  const fail = (message, restored = false) => result({ status: "error", message, restored });

  // ---- preconditions (nothing is written yet)
  let headRef = "";
  try {
    if (!existsSync(dir)) return fail(`worktree not found: ${dir}`);
    if (gitText(dir, ["rev-parse", "--is-inside-work-tree"], { allowFail: true }) !== "true") return fail(`not a git worktree: ${dir}`);
    const busy = inProgress(dir);
    if (busy) return fail(`a git operation is in progress here (${busy}); finish or abort it first. Nothing was changed.`);
    if (runGit(dir, ["ls-files", "-u", "-z"]).out.length) return fail("the index has unmerged entries; resolve them first. Nothing was changed.");
    const branch = gitText(dir, ["symbolic-ref", "-q", "--short", "HEAD"], { allowFail: true });
    if (!force && !branch.startsWith("ws/")) return fail(`HEAD is '${branch || "detached"}', not a ws/ branch; pass --force to sync it anyway. Nothing was changed.`);
    headRef = gitText(dir, ["symbolic-ref", "-q", "HEAD"], { allowFail: true }); // "" when detached
  } catch (e) {
    return fail(e.message);
  }

  // ---- 1. fetch
  const remoteRef = `refs/remotes/origin/${base}`;
  try {
    const f = runGit(dir, ["fetch", "--quiet", "origin", base], { timeout: 180000, allowFail: true });
    if (f.code !== 0) runGit(dir, ["fetch", "--quiet", "origin", base], { timeout: 180000 }); // one retry: a flaky link, not a verdict
  } catch (e) {
    return fail(`fetch failed: ${e.message}. Nothing was changed.`);
  }
  let head, target, baseSha;
  try {
    head = gitText(dir, ["rev-parse", "HEAD"]);
    target = gitText(dir, ["rev-parse", remoteRef]);
    baseSha = gitText(dir, ["merge-base", "HEAD", remoteRef]);
  } catch (e) {
    return fail(`cannot resolve origin/${base} against HEAD: ${e.message}. Nothing was changed.`);
  }

  // ---- 2. what changed on each side
  let upRows, localPaths, dirty;
  try {
    upRows = parseNameStatusZ(runGit(dir, ["diff", "--name-status", "-z", "-M", baseSha, target]).out);
    dirty = parseStatusZ(runGit(dir, ["status", "--porcelain=v1", "-z", "--untracked-files=all"]).out);
    const committed = splitZ(runGit(dir, ["diff", "--name-only", "-z", "--no-renames", baseSha, "HEAD"]).out);
    localPaths = new Set([...dirty, ...committed]);
  } catch (e) {
    return fail(`could not read the change lists: ${e.message}. Nothing was changed.`);
  }
  const blobAt = (rev, p) => gitText(dir, ["rev-parse", `${rev}:${p}`], { allowFail: true });
  const plan = planSync(upRows, localPaths, {
    // Not a clash: the local copy already IS the upstream state (committed there, or pulled by an earlier partial run).
    same: (p) => {
      const want = blobAt(target, p);
      if (!dirty.has(p)) return want !== "" && blobAt("HEAD", p) === want;
      const full = join(dir, p);
      if (!existsSync(full)) return want === "";
      return want !== "" && gitText(dir, ["hash-object", "--", p], { allowFail: true }) === want;
    },
  });
  const held = [...plan.held.keys()].sort();
  for (const p of held) say(`held: ${plan.held.get(p)}`);

  const procs = workbenchProcessesFor(dir, listProcesses);
  const wbRunning = procs.length > 0;

  if (plan.blocked.length) {
    for (const p of plan.blocked) say(`blocked: you and main both changed ${p} (not a .layer, so it is not safe to merge around it)`);
    say("nothing was changed. Commit or set aside the listed files, then run again.");
    return result({ status: "blocked", held, blocked: plan.blocked });
  }
  if (head === target || (!plan.apply.length && !held.length)) {
    say(`up to date with origin/${base}`);
    return result({ status: "ok", held, pulled: 0, upToDate: true });
  }

  if (!plan.apply.length) {
    say("nothing to pull except held files");
    return result({ status: "ok", held, pulled: 0 });
  }
  const fastForward = held.length === 0 && baseSha === head;
  const pulled = plan.apply.length;
  const mode = held.length ? "partial (held files stay as they are; pulled files are staged, HEAD does not move)" : fastForward ? "fast-forward" : "merge commit";
  say(`${dryRun ? "would pull" : "pulling"} ${pulled} file(s) from origin/${base}: ${mode}`);
  if (dryRun) {
    if (wbRunning) say(`Workbench is open on this worktree: after a real run, reload the world (File > Reload, or reopen it).`);
    return result({ status: "ok", held, pulled, reload: wbRunning && pulled > 0 });
  }

  // ---- 3. snapshot, back up, apply
  const gd = gitDir(dir);
  const backup = join(gd, `lf-world-sync-backup-${process.pid}`);
  let snap;
  let applied = false;
  let moved = false;
  let newTree = "";
  const oldTree = gitText(dir, ["rev-parse", "HEAD^{tree}"]);
  try {
    const beforeIndex = indexMap(dir);
    snap = {
      indexTree: gitText(dir, ["write-tree"]),
      index: beforeIndex,
      files: new Map([...dirty].map((p) => [p, fileFingerprint(dir, p)])),
      unstaged: sha256(runGit(dir, ["diff", "--binary"]).out),
      cached: sha256(runGit(dir, ["diff", "--cached", "--binary"]).out),
    };
    mkdirSync(backup, { recursive: true });
    for (const p of dirty) {
      const full = join(dir, p);
      if (existsSync(full) && lstatSync(full).isFile()) {
        const to = join(backup, "files", p);
        mkdirSync(dirname(to), { recursive: true });
        writeFileSync(to, readFileSync(full));
      }
    }

    // New tree = HEAD's tree plus the upstream state of every non-held upstream path (temp index, repo untouched).
    const tmpIdx = join(backup, "index.tmp");
    const env = { GIT_INDEX_FILE: tmpIdx };
    runGit(dir, ["read-tree", head], { env });
    const upIdx = [];
    const rm = [];
    const want = new Set(plan.apply);
    const raw = splitZ(runGit(dir, ["diff", "--raw", "-z", "--no-renames", "--no-abbrev", baseSha, target]).out);
    for (let i = 0; i + 1 < raw.length; i += 2) {
      const m = raw[i].match(/^:(\d+) (\d+) (\w+) (\w+) (\w)/);
      const p = raw[i + 1];
      if (!m || !want.has(p)) continue;
      if (m[5] === "D") rm.push(p);
      else upIdx.push(`${m[2]} ${m[4]}\t${p}`);
    }
    if (upIdx.length) runGit(dir, ["update-index", "-z", "--add", "--index-info"], { env, input: upIdx.join("\0") + "\0" });
    if (rm.length) runGit(dir, ["update-index", "-z", "--force-remove", "--stdin"], { env, input: rm.join("\0") + "\0" });
    newTree = gitText(dir, ["write-tree"], { env });

    let newHead = head;
    if (!held.length) {
      newHead = fastForward
        ? target
        : gitText(dir, ["commit-tree", newTree, "-p", head, "-p", target, "-m", `Merge origin/${base} into ${gitText(dir, ["symbolic-ref", "-q", "--short", "HEAD"], { allowFail: true }) || "HEAD"} (world-sync)`], {
            env: { GIT_AUTHOR_NAME: "world-sync", GIT_AUTHOR_EMAIL: "world-sync@life-framework.invalid", GIT_COMMITTER_NAME: "world-sync", GIT_COMMITTER_EMAIL: "world-sync@life-framework.invalid" },
          });
    }

    // The working tree and index follow, carrying local changes (read-tree refuses a file it cannot carry).
    applied = true;
    runGit(dir, ["read-tree", "-m", "-u", oldTree, newTree]);
    hooks.afterWorktree?.();
    if (newHead !== head) {
      const upd = headRef ? ["update-ref", "-m", "world-sync", headRef, newHead, head] : ["update-ref", "--no-deref", "-m", "world-sync", "HEAD", newHead, head];
      runGit(dir, upd);
      moved = true;
    }
    hooks.afterHead?.();

    // ---- verify: every local change is as it was
    const after = indexMap(dir);
    const problems = [];
    for (const [p, fp] of snap.files) {
      if (fileFingerprint(dir, p) !== fp) problems.push(`${p}: file content changed`);
      if (snap.index.get(p) !== after.get(p)) problems.push(`${p}: index entry changed`);
    }
    const touched = new Set(plan.apply);
    for (const [p, e] of snap.index) if (!touched.has(p) && after.get(p) !== e) problems.push(`${p}: index entry changed`);
    if (sha256(runGit(dir, ["diff", "--binary"]).out) !== snap.unstaged) problems.push("unstaged diff is not byte-identical");
    if (!held.length && sha256(runGit(dir, ["diff", "--cached", "--binary"]).out) !== snap.cached) problems.push("staged diff is not byte-identical");
    if (problems.length) throw new Error(`verification failed: ${problems.slice(0, 5).join("; ")}`);
  } catch (e) {
    const restored = restore();
    return fail(`${e.message}. ${restored ? "The prior state was restored." : `RESTORE INCOMPLETE; your copies of the dirty files are in ${backup}.`}`, restored);
  }
  if (existsSync(backup)) rmSync(backup, { recursive: true, force: true });

  const reload = wbRunning && pulled > 0;
  if (reload) say("Workbench is open on this worktree: reload the world in Workbench (File > Reload, or reopen it) to see the upstream changes.");
  say(`local diff verified byte-identical (${snap.files.size} local file(s))`);
  return result({ status: "ok", held, pulled, reload });

  function restore() {
    try {
      if (applied) {
        // Back out the working tree and index the way they went in (a 2-tree read-tree carries local changes).
        runGit(dir, ["read-tree", "-m", "-u", newTree || oldTree, oldTree], { allowFail: true });
      }
      if (moved || (headRef && gitText(dir, ["rev-parse", "HEAD"], { allowFail: true }) !== head)) {
        const upd = headRef ? ["update-ref", headRef, head] : ["update-ref", "--no-deref", "HEAD", head];
        runGit(dir, upd, { allowFail: true });
      }
      if (snap) {
        runGit(dir, ["read-tree", snap.indexTree], { allowFail: true });
        for (const [p, fp] of snap.files) {
          if (fileFingerprint(dir, p) === fp) continue;
          const src = join(backup, "files", p);
          const full = join(dir, p);
          if (fp === "absent") { rmSync(full, { force: true }); continue; }
          if (existsSync(src)) {
            mkdirSync(dirname(full), { recursive: true });
            writeFileSync(full, readFileSync(src));
          }
        }
        const ok =
          gitText(dir, ["rev-parse", "HEAD"], { allowFail: true }) === head &&
          [...snap.files].every(([p, fp]) => fileFingerprint(dir, p) === fp) &&
          sha256(runGit(dir, ["diff", "--binary"], { allowFail: true }).out) === snap.unstaged &&
          sha256(runGit(dir, ["diff", "--cached", "--binary"], { allowFail: true }).out) === snap.cached;
        if (ok && existsSync(backup)) rmSync(backup, { recursive: true, force: true });
        return ok;
      }
      if (existsSync(backup)) rmSync(backup, { recursive: true, force: true });
      return true;
    } catch {
      return false;
    }
  }
}

// ---------------------------------------------------------------- CLI helpers

export const DEFAULT_SLUG = "town-maintainer-edits";

export function watchHint(slug = DEFAULT_SLUG) {
  return `tools\\cli world-sync --wt ${slug}    # run at each check-in; add --dry-run to preview; exit 0 ok, 2 blocked, 1 error (state restored)`;
}

export function worktreeDirFor(mainRoot, slug) {
  return join(dirname(resolve(mainRoot)), `Life-Framework-ws-${slug}`);
}

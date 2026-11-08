// tools/remote/testbox.mjs - run the in-game suite on a second Windows machine.
//
// `tools\cli test --remote [host]` ships the worktree's committed AND
// uncommitted state (tracked edits plus untracked, non-ignored files) to the
// test box as a git bundle, checks it out in a per-slug worktree there, and
// runs `tools\cli test --no-build --local` under the box's own server-2001
// lock. Output streams back live; the exit code is the remote suite's.
// The console.log always comes back to
// <worktree>/server/profile/test/logs/remote-<host>-<logdir>/ so
// `tools\cli logs` greps it as usual.
//
// Without --remote/--local, `test` moves to the box by itself when this
// machine's server-2001 lock is held by a live run and the box answers ssh.
//
// Nothing is pushed: the snapshot is a commit object built from a temporary
// index (the real index and HEAD are untouched) and travels as a bundle over
// scp. The box needs no credentials; it fetches main from the public GitHub
// repo. Box setup: tools/remote/provision-testbox.ps1 (`tools\cli testbox provision`).
//
// On the box, run worktrees are C:\LF\Life-Framework-rt-<slug> (detached HEAD,
// linked to the box's main clone). They are reused across runs, so a run only
// checks out the changed files. `tools\cli testbox prune <slug>|--all` removes
// them through the junction-safe helper.

import { spawn, spawnSync } from "node:child_process";
import { copyFileSync, existsSync, mkdirSync, readFileSync, appendFileSync, rmSync, statSync } from "node:fs";
import { basename, dirname, join, resolve } from "node:path";
import * as wt from "../wt.mjs";
import { localSetting } from "../lib/local-config.mjs";

const SSH_OPTS = ["-o", "BatchMode=yes", "-o", "ConnectTimeout=8", "-o", "ServerAliveInterval=30"];
const DEFAULT_REMOTE_ROOT = "C:/LF/Life-Framework";
const SERVER_SLOT = "server-2001";

export function configuredHost(root) {
  return localSetting(root, "LF_TESTBOX");
}

function remoteRoot(root) {
  return (localSetting(root, "LF_TESTBOX_ROOT") || DEFAULT_REMOTE_ROOT).replace(/\\/g, "/").replace(/\/$/, "");
}

function remoteRunDir(root, slug) {
  return `${dirname(remoteRoot(root)).replace(/\\/g, "/")}/Life-Framework-rt-${slug}`;
}

function psQuote(s) {
  return `'${String(s).replace(/'/g, "''")}'`;
}

// Runs a PowerShell script on the box. -EncodedCommand sidesteps every layer
// of quoting between this shell, ssh, and the remote default shell. Two
// wrinkles of a redirected Windows PowerShell 5.1: Write-Host records come
// back on stderr as CLIXML (so Write-Host is redefined to plain stdout), and
// the remote default shell turns any nonzero inner exit into 1 (so it exits
// with $LASTEXITCODE explicitly).
const PS_PRELUDE = "$ProgressPreference = 'SilentlyContinue'\nfunction Write-Host { [Console]::Out.WriteLine(($args -join ' ')) }\n";

function sshPsArgs(host, script) {
  const encoded = Buffer.from(PS_PRELUDE + script, "utf16le").toString("base64");
  return [...SSH_OPTS, host, `powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass -EncodedCommand ${encoded}; exit $LASTEXITCODE`];
}

function sshPs(host, script, opts = {}) {
  return spawnSync("ssh", sshPsArgs(host, script), { windowsHide: true, encoding: "utf8", timeout: opts.timeout ?? 600000, maxBuffer: 64 * 1024 * 1024 });
}

function sshPsStream(host, script, onLine) {
  return new Promise((resolvePromise) => {
    const child = spawn("ssh", sshPsArgs(host, script), { windowsHide: true, stdio: ["ignore", "pipe", "pipe"] });
    let buf = "";
    const feed = (chunk) => {
      const text = chunk.toString("utf8");
      process.stdout.write(text);
      buf += text;
      let nl;
      while ((nl = buf.indexOf("\n")) >= 0) {
        onLine(buf.slice(0, nl).replace(/\r$/, ""));
        buf = buf.slice(nl + 1);
      }
    };
    child.stdout.on("data", feed);
    child.stderr.on("data", feed);
    child.on("error", (e) => {
      console.log(`FAIL  ssh: ${e.message}`);
      resolvePromise(3);
    });
    child.on("close", (code) => {
      if (buf) onLine(buf);
      resolvePromise(code ?? 3);
    });
  });
}

function scp(args) {
  return spawnSync("scp", ["-q", ...SSH_OPTS, ...args], { windowsHide: true, encoding: "utf8", timeout: 600000 });
}

export function reachable(host) {
  const res = spawnSync("ssh", [...SSH_OPTS, "-o", "ConnectTimeout=5", host, "exit 0"], { windowsHide: true, encoding: "utf8", timeout: 15000 });
  return res.status === 0;
}

// The live owner of this machine's server-2001 lock, or null when it is free.
export function serverSlotOwner(root) {
  const file = join(wt.hubDirOf(root), "locks", `${SERVER_SLOT}.lock`);
  if (!existsSync(file)) return null;
  try {
    const info = JSON.parse(readFileSync(file, "utf8"));
    if (!info.pid) return null;
    process.kill(info.pid, 0);
    return info;
  } catch {
    return null;
  }
}

// Where `test` runs: { host } for the test box, or null for this machine.
// opts.local / opts.remote come from --local / --remote [host].
export function chooseTarget(root, opts = {}) {
  if (opts.local) return null;
  const host = configuredHost(root);
  if (opts.remote !== undefined) {
    const h = opts.remote || host;
    if (!h) throw new Error("--remote needs a host: --remote <ssh-host>, or set LF_TESTBOX in the env or lf.local.json");
    return { host: h, reason: "--remote" };
  }
  if (!host || localSetting(root, "LF_TESTBOX_AUTO") === "0") return null;
  const owner = serverSlotOwner(root);
  if (!owner) return null;
  if (!reachable(host)) {
    console.log(`test: local ${SERVER_SLOT} is busy (pid ${owner.pid}) and ${host} does not answer; queuing here`);
    return null;
  }
  return { host, reason: `local ${SERVER_SLOT} busy (pid ${owner.pid})` };
}

function git(root, args, env) {
  return spawnSync("git", ["-C", root, ...args], { windowsHide: true, encoding: "utf8", env: env ? { ...process.env, ...env } : process.env, maxBuffer: 64 * 1024 * 1024 });
}

function gitOut(root, args, env) {
  const r = git(root, args, env);
  if (r.status !== 0) throw new Error(`git ${args.join(" ")}: ${(r.stderr || r.stdout).trim()}`);
  return r.stdout.trim();
}

// A commit holding the working tree as it is now: HEAD plus tracked edits plus
// untracked files that are not ignored. Built in a copy of the index, so the
// real index, HEAD, and branch never change.
export function snapshotCommit(root) {
  const head = gitOut(root, ["rev-parse", "HEAD"]);
  const tmpDir = join(root, "tmp", "remote");
  mkdirSync(tmpDir, { recursive: true });
  const index = join(tmpDir, "snapshot.index");
  const realIndex = resolve(root, gitOut(root, ["rev-parse", "--git-path", "index"]));
  if (existsSync(realIndex)) copyFileSync(realIndex, index);
  const env = { GIT_INDEX_FILE: index };
  try {
    gitOut(root, ["add", "-A"], env);
    const tree = gitOut(root, ["write-tree"], env);
    if (tree === gitOut(root, ["rev-parse", "HEAD^{tree}"])) return { sha: head, head, dirty: false };
    const ident = {
      GIT_AUTHOR_NAME: "lf-remote", GIT_AUTHOR_EMAIL: "lf-remote@localhost",
      GIT_COMMITTER_NAME: "lf-remote", GIT_COMMITTER_EMAIL: "lf-remote@localhost",
    };
    const sha = gitOut(root, ["commit-tree", tree, "-p", head, "-m", "lf remote test snapshot (uncommitted state)"], ident);
    const changed = gitOut(root, ["diff", "--name-only", head, sha]).split(/\r?\n/).filter(Boolean).length;
    return { sha, head, dirty: true, changed };
  } finally {
    rmSync(index, { force: true });
  }
}

// Bundle of everything the box cannot fetch from GitHub (null when the
// snapshot is already on origin/main).
function makeBundle(root, slug, sha) {
  if (git(root, ["merge-base", "--is-ancestor", sha, "origin/main"]).status === 0) return null;
  const ref = `refs/lf-remote/${slug}`;
  const file = join(root, "tmp", "remote", "run.bundle");
  rmSync(file, { force: true });
  gitOut(root, ["update-ref", ref, sha]);
  try {
    const hasBase = git(root, ["rev-parse", "--verify", "-q", "origin/main"]).status === 0;
    gitOut(root, ["bundle", "create", file, ref, ...(hasBase ? ["--not", "origin/main"] : [])]);
  } finally {
    git(root, ["update-ref", "-d", ref]);
  }
  return file;
}

function appendStamp(root, line, sha, consoleLog, host) {
  const m = /\[ELTEST\] SUMMARY tier=(\w+) passed=(\d+) failed=(\d+) total=(\d+)/.exec(line);
  if (!m) return;
  const dir = join(root, "tmp", "test-results");
  mkdirSync(dir, { recursive: true });
  const rec = { sha, tier: m[1], passed: +m[2], failed: +m[3], total: +m[4], consoleLog: consoleLog || "", host };
  appendFileSync(join(dir, "summaries.jsonl"), JSON.stringify(rec) + "\n", "utf8");
}

// Steam build of a dedicated server install: SteamCMD keeps the manifest in
// <install>/steamapps, the Steam client in <library>/steamapps.
export function serverBuildId(serverDir) {
  const dir = serverDir.replace(/[\\/]+$/, "").replace(/[\\/]Arma\w*\.exe$/i, "");
  for (const m of [join(dir, "steamapps", "appmanifest_1874900.acf"), join(dir, "..", "..", "appmanifest_1874900.acf")]) {
    try {
      const id = /"buildid"\s+"(\d+)"/.exec(readFileSync(m, "utf8"))?.[1];
      if (id) return id;
    } catch {}
  }
  return null;
}

function localServerDir(root) {
  return localSetting(root, "ENFUSION_SERVER_PATH") || "C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger Server";
}

// PowerShell that prints "lf-remote: buildid <n>" for the box's server install.
const PS_BUILDID = [
  "$cfg = Join-Path $main 'lf.local.json'",
  "if (Test-Path -LiteralPath $cfg) { $sp = (Get-Content -LiteralPath $cfg -Raw | ConvertFrom-Json).ENFUSION_SERVER_PATH; foreach ($m in @((Join-Path $sp 'steamapps\\appmanifest_1874900.acf'), (Join-Path $sp '..\\..\\appmanifest_1874900.acf'))) { if (Test-Path -LiteralPath $m) { if ((Get-Content -LiteralPath $m -Raw) -match '\"buildid\"\\s+\"(\\d+)\"') { Write-Host \"lf-remote: buildid $($matches[1])\"; break } } } }",
].join("\n");

const KEY_LINE = /SCRIPT\s+\(E\)|Virtual Machine Exception|VME|Unable to find|Can't compile|\[ELTEST\]   - |\( E \)/;

// tier/only as cmdTest resolved them. Returns the suite's exit code; 3 means
// the box could not be prepared (no verdict).
export async function runRemoteTest(root, { host, tier = "all", only = [] }) {
  const started = Date.now();
  const slug = wt.slugOfRoot(root) || basename(root).replace(/^Life-Framework-ws-/, "").toLowerCase().replace(/[^a-z0-9-]/g, "-");
  const rRoot = remoteRoot(root);
  const rDir = remoteRunDir(root, slug);

  let snap;
  try {
    snap = snapshotCommit(root);
  } catch (e) {
    console.log(`FAIL  snapshot: ${e.message}`);
    return 3;
  }
  console.log(`test: remote ${host} ${rDir} @ ${snap.sha.slice(0, 10)}${snap.dirty ? ` (HEAD ${snap.head.slice(0, 10)} + ${snap.changed} uncommitted path(s))` : ""}`);

  // 1. The box fetches main and makes sure the run worktree exists.
  const prep = sshPs(host, [
    `$main = ${psQuote(rRoot)}; $dir = ${psQuote(rDir)}`,
    "if (-not (Test-Path -LiteralPath (Join-Path $main '.git'))) { Write-Host \"lf-remote: no clone at $main (run: tools\\cli testbox provision)\"; exit 3 }",
    "git -C $main fetch -q origin; if ($LASTEXITCODE) { Write-Host 'lf-remote: git fetch origin failed'; exit 3 }",
    "if (-not (Test-Path -LiteralPath (Join-Path $dir '.git'))) { git -C $main worktree add -q --detach $dir origin/main; if ($LASTEXITCODE) { exit 3 } }",
    "New-Item -ItemType Directory -Force -Path (Join-Path $dir 'tmp\\remote-in') | Out-Null",
    PS_BUILDID,
    "Write-Host 'lf-remote: ready'",
  ].join("\n"), { timeout: 300000 });
  if (prep.status !== 0 || !/lf-remote: ready/.test(prep.stdout)) {
    console.log(`FAIL  ${host}: could not prepare ${rDir}`);
    process.stdout.write(prep.stdout + prep.stderr);
    return 3;
  }
  // Results only compare across machines on the same server build.
  const boxBuild = /lf-remote: buildid (\d+)/.exec(prep.stdout)?.[1] ?? null;
  const hereBuild = serverBuildId(localServerDir(root));
  console.log(`test: server build ${host} ${boxBuild ?? "unknown"}, this PC ${hereBuild ?? "unknown"}`);
  if (boxBuild && hereBuild && boxBuild !== hereBuild) {
    console.log(`WARN  server builds differ (${host} ${boxBuild} vs this PC ${hereBuild}); update the older one before comparing results (tools\\cli testbox provision updates the box)`);
  }

  // 2. Bundle + resource database (gitignored, so it never rides the bundle).
  let bundle;
  try {
    bundle = makeBundle(root, slug, snap.sha);
  } catch (e) {
    console.log(`FAIL  bundle: ${e.message}`);
    return 3;
  }
  wt.copyResourceDatabase(root);
  const rdb = join(root, "addons", "LifeFramework", "resourceDatabase.rdb");
  const send = [bundle, existsSync(rdb) ? rdb : null].filter(Boolean);
  if (send.length) {
    const up = scp([...send, `${host}:${rDir}/tmp/remote-in/`]);
    if (up.status !== 0) {
      console.log(`FAIL  scp to ${host}: ${(up.stderr || up.stdout).trim()}`);
      return 3;
    }
  }
  if (bundle) rmSync(bundle, { force: true });

  // 3. Check out the snapshot and run the suite under the box's own lock.
  const args = ["test", "--no-build", "--local", "--tier", tier, ...(only.length ? ["--only", only.join(",")] : [])];
  const script = [
    `$dir = ${psQuote(rDir)}; $sha = ${psQuote(snap.sha)}; $ref = ${psQuote(`refs/lf-remote/${slug}`)}`,
    "Set-Location -LiteralPath $dir",
    "$in = Join-Path $dir 'tmp\\remote-in'",
    "$bundle = Join-Path $in 'run.bundle'",
    "if (Test-Path -LiteralPath $bundle) { git fetch -q $bundle $ref; if ($LASTEXITCODE) { Write-Host 'lf-remote: git fetch of the bundle failed'; exit 3 }; Remove-Item -LiteralPath $bundle -Force }",
    "git checkout -q --detach -f $sha; if ($LASTEXITCODE) { Write-Host \"lf-remote: checkout $sha failed\"; exit 3 }",
    "$rdbIn = Join-Path $in 'resourceDatabase.rdb'",
    "if (Test-Path -LiteralPath $rdbIn) { Move-Item -LiteralPath $rdbIn -Destination (Join-Path $dir 'addons\\LifeFramework\\resourceDatabase.rdb') -Force }",
    `& node tools\\cli.mjs ${args.map(psQuote).join(" ")}`,
    "$code = $LASTEXITCODE",
    "$ld = Get-ChildItem -LiteralPath (Join-Path $dir 'server\\profile\\test\\logs') -Directory -ErrorAction SilentlyContinue | Sort-Object Name | Select-Object -Last 1",
    "if ($ld -and (Test-Path -LiteralPath (Join-Path $ld.FullName 'console.log'))) { Write-Host \"lf-remote: console.log $($ld.FullName)\\console.log\" }",
    "exit $code",
  ].join("\n");

  let summaryLine = "";
  let remoteLog = "";
  const code = await sshPsStream(host, script, (line) => {
    if (/\[ELTEST\] SUMMARY tier=/.test(line)) summaryLine = line;
    const m = /^lf-remote: console\.log (.+)$/.exec(line.trim());
    if (m) remoteLog = m[1].trim();
  });

  // 4. Bring the console.log home so `tools\cli logs` and the report read it.
  let localLog = "";
  if (remoteLog) {
    const logDir = basename(dirname(remoteLog.replace(/\\/g, "/")));
    const dest = join(root, "server", "profile", "test", "logs", `remote-${host}-${logDir}`);
    mkdirSync(dest, { recursive: true });
    const down = scp([`${host}:${remoteLog.replace(/\\/g, "/")}`, join(dest, "console.log")]);
    if (down.status === 0) localLog = join(dest, "console.log");
    else console.log(`WARN  could not fetch ${remoteLog}: ${(down.stderr || down.stdout).trim()}`);
  }
  if (localLog) {
    const engine = readFileSync(localLog, "utf8").split(/\r?\n/, 40).find((l) => /Initializing engine, version/.test(l));
    if (engine) console.log(`test: ${host} ${engine.replace(/^.*?ENGINE\s*:\s*/, "").trim()}`);
  }
  if (code !== 0 && localLog) {
    const key = readFileSync(localLog, "utf8").split(/\r?\n/).filter((l) => KEY_LINE.test(l)).slice(0, 15);
    if (key.length) {
      console.log(`test: key lines from the ${host} console.log:`);
      for (const l of key) console.log(`  ${l.trim()}`);
    }
  }
  if (summaryLine) appendStamp(root, summaryLine, snap.head, localLog, host);
  const secs = Math.round((Date.now() - started) / 1000);
  if (localLog) console.log(`test: log ${localLog}`);
  console.log(code === 3 ? `test: remote ${host} FAILED to start the run (${secs} s)` : `test: remote ${host} exit ${code} (${secs} s)`);
  return code;
}

// ---------------------------------------------------------------- testbox command

function cmdProvision(root, host, flags) {
  const script = join(root, "tools", "remote", "provision-testbox.ps1");
  const rRoot = remoteRoot(root);
  const parent = dirname(rRoot);
  console.log(`testbox: provisioning ${host} (clone ${rRoot})`);
  const mk = sshPs(host, `New-Item -ItemType Directory -Force -Path ${psQuote(parent)} | Out-Null`);
  if (mk.status !== 0) {
    console.log(`FAIL  ssh ${host}: ${(mk.stderr || mk.stdout).trim()}`);
    return 1;
  }
  const up = scp([script, `${host}:${parent}/provision-testbox.ps1`]);
  if (up.status !== 0) {
    console.log(`FAIL  scp: ${(up.stderr || up.stdout).trim()}`);
    return 1;
  }
  const extra = flags.includes("--skip-server-update") ? " -SkipServerUpdate" : "";
  const res = spawnSync("ssh", [...SSH_OPTS, host, `powershell -NoProfile -ExecutionPolicy Bypass -File ${parent}/provision-testbox.ps1 -RepoDir ${rRoot}${extra}; exit $LASTEXITCODE`], { stdio: "inherit", windowsHide: true });
  if (res.status !== 0) return res.status ?? 1;
  // The box has no Workbench to build resourceDatabase.rdb; give its main
  // clone this checkout's copy so runs started on the box find one.
  const rdb = join(wt.mainRootOf(root), "addons", "LifeFramework", "resourceDatabase.rdb");
  if (existsSync(rdb)) {
    const r = scp([rdb, `${host}:${rRoot}/addons/LifeFramework/resourceDatabase.rdb`]);
    console.log(r.status === 0 ? `testbox: sent resourceDatabase.rdb (${statSync(rdb).size} bytes)` : `WARN  rdb copy failed: ${(r.stderr || r.stdout).trim()}`);
  }
  return 0;
}

function cmdStatus(root, host) {
  const up = reachable(host);
  console.log(`testbox: ${host} ${up ? "reachable" : "NOT reachable"}`);
  const owner = serverSlotOwner(root);
  console.log(`testbox: local ${SERVER_SLOT} ${owner ? `busy (pid ${owner.pid})` : "free"}`);
  if (!up) return 1;
  const rRoot = remoteRoot(root);
  const res = sshPs(host, [
    `$main = ${psQuote(rRoot)}`,
    "$cfg = Join-Path $main 'lf.local.json'",
    "if (Test-Path $cfg) { $c = Get-Content $cfg -Raw | ConvertFrom-Json; Write-Host \"server:  $($c.ENFUSION_SERVER_PATH)\" } else { Write-Host \"no $cfg (run: tools\\cli testbox provision)\" }",
    PS_BUILDID,
    "$lock = Join-Path $main 'tmp\\wt\\locks\\server-2001.lock'",
    "if (Test-Path $lock) { $i = Get-Content $lock -Raw | ConvertFrom-Json; if (Get-Process -Id $i.pid -ErrorAction SilentlyContinue) { Write-Host \"lock:    server-2001 busy (pid $($i.pid), since $($i.at))\" } else { Write-Host 'lock:    server-2001 stale' } } else { Write-Host 'lock:    server-2001 free' }",
    "Write-Host \"main:    $(git -C $main log -1 --format='%h %s' origin/main)\"",
    "git -C $main worktree list | Select-String -SimpleMatch 'Life-Framework-rt-' | ForEach-Object { Write-Host \"run:     $_\" }",
  ].join("\n"), { timeout: 60000 });
  process.stdout.write((res.stdout + res.stderr).replace(/^lf-remote: buildid (\d+)/m, (_, id) => `buildid: ${id} (this PC ${serverBuildId(localServerDir(root)) ?? "unknown"})`));
  return res.status ?? 1;
}

function cmdPrune(root, host, flags) {
  const rRoot = remoteRoot(root);
  const all = flags.includes("--all");
  const slug = flags.find((f) => !f.startsWith("--"));
  if (!all && !slug) {
    console.log("usage: tools\\cli testbox prune <slug>|--all");
    return 2;
  }
  // removeWorktree (tools/link-safety.mjs) unlinks every junction first, so
  // the game-addons links into the server install are never followed.
  const pattern = all ? "Life-Framework-rt-" : `Life-Framework-rt-${slug}`;
  const res = sshPs(host, [
    `$main = ${psQuote(rRoot)}; $pattern = ${psQuote(pattern)}; $exact = ${all ? "$false" : "$true"}`,
    "$dirs = @(git -C $main worktree list --porcelain | Where-Object { $_ -like 'worktree *' } | ForEach-Object { $_.Substring(9) } | Where-Object { $leaf = Split-Path -Leaf $_; if ($exact) { $leaf -eq $pattern } else { $leaf.StartsWith($pattern) } })",
    "if ($dirs.Count -eq 0) { Write-Host 'testbox: nothing to prune'; exit 0 }",
    "$helper = 'file:///' + (Join-Path $main 'tools\\link-safety.mjs').Replace('\\', '/')",
    "$fail = 0",
    "foreach ($d in $dirs) {",
    "  $js = \"import('$helper').then((m) => { const r = m.removeWorktree(process.argv[1], process.argv[2], { force: true }); process.stdout.write((r.stdout || '') + (r.stderr || '')); process.exit(r.status ?? 1); })\"",
    "  & node --input-type=module -e $js $main $d",
    "  if ($LASTEXITCODE) { Write-Host \"testbox: FAIL removing $d\"; $fail = 1 } else { Write-Host \"testbox: removed $d\" }",
    "}",
    "exit $fail",
  ].join("\n"), { timeout: 300000 });
  process.stdout.write(res.stdout + res.stderr);
  return res.status ?? 1;
}

export function cmdTestbox(root, args) {
  const [sub, ...rest] = args;
  const hi = rest.indexOf("--host");
  const host = (hi >= 0 ? rest[hi + 1] : undefined) || configuredHost(root);
  const flags = hi >= 0 ? rest.filter((_, i) => i !== hi && i !== hi + 1) : rest;
  if (!sub || sub === "help" || !["provision", "status", "prune"].includes(sub)) {
    console.log(`usage: tools\\cli testbox provision|status|prune [--host <ssh-host>]
  provision [--skip-server-update]  install Git/Node (if missing), SteamCMD + Arma Reforger Server
                                    (anonymous), clone the repo, write lf.local.json; send the rdb
  status                            reachability, server build, the box's server-2001 lock, run dirs
  prune <slug>|--all                remove run worktrees on the box (junction-safe)
host: --host, else LF_TESTBOX (env or lf.local.json). Runs: tools\\cli test --remote [host]`);
    return sub && sub !== "help" ? 2 : 0;
  }
  if (!host) {
    console.log("FAIL  no test box: pass --host <ssh-host> or set LF_TESTBOX in the env or lf.local.json");
    return 2;
  }
  if (sub === "provision") return cmdProvision(root, host, flags);
  if (sub === "status") return cmdStatus(root, host);
  return cmdPrune(root, host, flags);
}

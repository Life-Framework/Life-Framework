// tools/remote/testbox.mjs - run a worktree on another Windows machine of the
// test pool (tools/remote/pool.mjs picks the machine and holds its slot).
//
// Sync: the worktree's committed AND uncommitted state (tracked edits plus
// untracked, non-ignored files) becomes one snapshot commit, built from a copy
// of the index (the real index, HEAD, and branch never change). The remote run
// folder <box>/Life-Framework-rt-<slug> is a linked worktree of the box's own
// clone, kept between runs. The pool holder reports which commits it already
// has (the snapshot, the last one this worktree sent, origin/main); the git
// bundle carries only the commits past those, so a repeat run of an unchanged
// worktree sends nothing and an edit sends one small commit.
//
// resourceDatabase.rdb is gitignored, so it travels beside the bundle: the
// local copy is refreshed first (headless Workbench, tools/lib/rdb-fresh.mjs;
// the box has no Workbench build) and sent only when its SHA-1 differs from
// the run folder's. Each addon has its own (the pack's too, #322); the core's
// travels as resourceDatabase.rdb, another addon's as resourceDatabase.<name>.rdb.
//
// The remote command runs under the slot the holder owns (LF_SERVER_SLOT_HELD
// names the lock and its token); output streams back live and the exit code is
// the remote command's. The newest console.log comes back to
// <worktree>/server/profile/test/logs/remote-<machine>-<logdir>/ so
// `tools\cli logs` greps it as usual.
//
// Nothing is pushed: the box fetches main from the public GitHub repo and holds
// no credentials. Box setup: tools/remote/provision-testbox.ps1
// (`tools\cli testbox provision`). Remove run folders with `tools\cli pool
// prune` or `testbox prune`, never by hand (junctions into the server install).

import { spawn, spawnSync } from "node:child_process";
import { createHash } from "node:crypto";
import { gzipSync } from "node:zlib";
import { appendFileSync, copyFileSync, existsSync, mkdirSync, readFileSync, rmSync, statSync } from "node:fs";
import { basename, dirname, join, resolve } from "node:path";
import * as wt from "../wt.mjs";
import { ADDONS, CORE } from "../lib/addons.mjs";
import { localSetting } from "../lib/local-config.mjs";
import { cmdRecord, cmdScreenshot, cmdStart, cmdVdd } from "./desktop.mjs";

const DEFAULT_REMOTE_ROOT = "C:/LF/Life-Framework";
const SERVER_SLOT = "server-2001";

function sshOpts(connectTimeout = 8) {
  return ["-o", "BatchMode=yes", "-o", `ConnectTimeout=${connectTimeout}`, "-o", "ServerAliveInterval=15", "-o", "ServerAliveCountMax=3"];
}

export function configuredHost(root) {
  return localSetting(root, "LF_TESTBOX");
}

function remoteRoot(root) {
  return (localSetting(root, "LF_TESTBOX_ROOT") || DEFAULT_REMOTE_ROOT).replace(/\\/g, "/").replace(/\/$/, "");
}

export function psQuote(s) {
  return `'${String(s).replace(/'/g, "''")}'`;
}

// Runs a PowerShell script on the box. -EncodedCommand sidesteps every layer
// of quoting between this shell, ssh, and the remote default shell. Two
// wrinkles of a redirected Windows PowerShell 5.1: Write-Host records come
// back on stderr as CLIXML (so Write-Host is redefined to plain stdout), and
// the remote default shell turns any nonzero inner exit into 1 (so it exits
// with $LASTEXITCODE explicitly).
const PS_PRELUDE = "$ProgressPreference = 'SilentlyContinue'\nfunction Write-Host { [Console]::Out.WriteLine(($args -join ' ')) }\n";

// The remote command line is cut at about 8 KB (a long -EncodedCommand failed
// to parse mid-script), so a long script travels gzipped and the remote
// default shell (PowerShell) inflates and runs it itself.
export function sshPsArgs(host, script, { connectTimeout } = {}) {
  const full = PS_PRELUDE + script;
  const encoded = Buffer.from(full, "utf16le").toString("base64");
  if (encoded.length < 7000) {
    return [...sshOpts(connectTimeout), host, `powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass -EncodedCommand ${encoded}; exit $LASTEXITCODE`];
  }
  const gz = gzipSync(Buffer.from(full, "utf8")).toString("base64");
  const boot = `$lfz = New-Object IO.Compression.GZipStream((New-Object IO.MemoryStream(,[Convert]::FromBase64String('${gz}'))), [IO.Compression.CompressionMode]::Decompress); $lfs = (New-Object IO.StreamReader($lfz, [Text.Encoding]::UTF8)).ReadToEnd(); Invoke-Expression $lfs; exit $LASTEXITCODE`;
  if (boot.length > 7800) throw new Error(`remote script too long for one ssh command (${boot.length} chars gzipped)`);
  return [...sshOpts(connectTimeout), host, boot];
}

export function sshPs(host, script, opts = {}) {
  return spawnSync("ssh", sshPsArgs(host, script, opts), { windowsHide: true, encoding: "utf8", timeout: opts.timeout ?? 600000, maxBuffer: 64 * 1024 * 1024 });
}

export function sshPsStream(host, script, onLine) {
  return new Promise((resolvePromise) => {
    const child = spawn("ssh", sshPsArgs(host, script, { connectTimeout: 5 }), { windowsHide: true, stdio: ["ignore", "pipe", "pipe"] });
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

export function scp(args) {
  return spawnSync("scp", ["-q", ...sshOpts(), ...args], { windowsHide: true, encoding: "utf8", timeout: 600000 });
}

export function reachable(host) {
  const res = spawnSync("ssh", [...sshOpts(5), host, "exit 0"], { windowsHide: true, encoding: "utf8", timeout: 15000 });
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
// real index, HEAD, and branch never change. Same tree as last time -> same
// commit (fixed identity and date), so an unchanged worktree is already there.
export function snapshotCommit(root) {
  const head = gitOut(root, ["rev-parse", "HEAD"]);
  const tmpDir = join(root, "tmp", "remote");
  mkdirSync(tmpDir, { recursive: true });
  const index = join(tmpDir, `snapshot-${process.pid}.index`);
  const realIndex = resolve(root, gitOut(root, ["rev-parse", "--git-path", "index"]));
  if (existsSync(realIndex)) copyFileSync(realIndex, index);
  const env = { GIT_INDEX_FILE: index };
  try {
    gitOut(root, ["add", "-A"], env);
    const tree = gitOut(root, ["write-tree"], env);
    if (tree === gitOut(root, ["rev-parse", "HEAD^{tree}"])) return { sha: head, head, dirty: false, changed: 0 };
    const ident = {
      GIT_AUTHOR_NAME: "lf-remote", GIT_AUTHOR_EMAIL: "lf-remote@localhost",
      GIT_COMMITTER_NAME: "lf-remote", GIT_COMMITTER_EMAIL: "lf-remote@localhost",
      GIT_AUTHOR_DATE: "2000-01-01T00:00:00Z", GIT_COMMITTER_DATE: "2000-01-01T00:00:00Z",
    };
    const sha = gitOut(root, ["commit-tree", tree, "-p", head, "-m", "lf remote snapshot (uncommitted state)"], ident);
    const changed = gitOut(root, ["diff", "--name-only", head, sha]).split(/\r?\n/).filter(Boolean).length;
    return { sha, head, dirty: true, changed };
  } finally {
    rmSync(index, { force: true });
  }
}

// Bundle of the commits the box lacks: everything reachable from the snapshot
// minus what the holder said the box has. null when the box has the snapshot.
function makeBundle(root, slug, sha, has) {
  if (has.has(sha)) return null;
  const ref = `refs/lf-remote/${slug}`;
  const file = join(root, "tmp", "remote", `run-${process.pid}.bundle`);
  rmSync(file, { force: true });
  gitOut(root, ["update-ref", ref, sha]);
  try {
    const not = [...has].filter((c) => git(root, ["cat-file", "-e", `${c}^{commit}`]).status === 0);
    gitOut(root, ["bundle", "create", file, ref, ...(not.length ? ["--not", ...not] : [])]);
  } finally {
    git(root, ["update-ref", "-d", ref]);
  }
  return file;
}

function sha1File(file) {
  return createHash("sha1").update(readFileSync(file)).digest("hex");
}

function appendStamp(root, line, sha, consoleLog, host) {
  const m = /\[LFTEST\] SUMMARY tier=(\w+) passed=(\d+) failed=(\d+) total=(\d+)/.exec(line);
  if (!m) return null;
  const dir = join(root, "tmp", "test-results");
  mkdirSync(dir, { recursive: true });
  const rec = { sha, tier: m[1], passed: +m[2], failed: +m[3], total: +m[4], consoleLog: consoleLog || "", host };
  appendFileSync(join(dir, "summaries.jsonl"), JSON.stringify(rec) + "\n", "utf8");
  return rec;
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

export function localServerDir(root) {
  return localSetting(root, "ENFUSION_SERVER_PATH") || "C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger Server";
}

// PowerShell that prints "lf-remote: buildid <n>" for the box's server install ($main = its clone).
export const PS_BUILDID = [
  "$cfg = Join-Path $main 'lf.local.json'",
  "if (Test-Path -LiteralPath $cfg) { $sp = (Get-Content -LiteralPath $cfg -Raw | ConvertFrom-Json).ENFUSION_SERVER_PATH; foreach ($m in @((Join-Path $sp 'steamapps\\appmanifest_1874900.acf'), (Join-Path $sp '..\\..\\appmanifest_1874900.acf'))) { if (Test-Path -LiteralPath $m) { if ((Get-Content -LiteralPath $m -Raw) -match '\"buildid\"\\s+\"(\\d+)\"') { Write-Host \"lf-remote: buildid $($matches[1])\"; break } } } }",
].join("\n");

const KEY_LINE = /SCRIPT\s+\(E\)|Virtual Machine Exception|VME|Unable to find|Can't compile|\[LFTEST\]   - |\( E \)/;

// Sends the snapshot (delta bundle) and the rdb to the slot's run folder.
// Returns { ok, sentBundle, bundleBytes, sentRdb, ms } and records what the
// folder now has in <worktree>/tmp/remote/pool-sync.json.
export function syncToSlot(root, slot, { writeSyncState } = {}) {
  const t0 = Date.now();
  const m = slot.machine;
  const snap = slot.snap;
  const slug = basename(slot.rDir).replace(/^Life-Framework-rt-/, "");
  let bundle;
  try {
    bundle = makeBundle(root, slug, snap.sha, slot.prep.has);
  } catch (e) {
    console.log(`FAIL  bundle: ${e.message}`);
    return { ok: false };
  }
  const rdbs = localRdbs(root);
  const toSend = rdbs.filter((r) => r.hash !== slot.prep.rdbs?.[r.name]);
  const sendRdb = toSend.length > 0;
  const send = [bundle, ...toSend.map((r) => stageRdb(root, r))].filter(Boolean);
  const rdbHash = Object.fromEntries(rdbs.map((r) => [r.name, r.hash]));
  const bundleBytes = bundle ? statSync(bundle).size : 0;
  if (send.length) {
    const up = scp([...send, `${m.host}:${slot.rDir}/tmp/remote-in/`]);
    if (up.status !== 0) {
      console.log(`FAIL  scp to ${m.host}: ${(up.stderr || up.stdout).trim()}`);
      if (bundle) rmSync(bundle, { force: true });
      return { ok: false };
    }
  }
  if (bundle) rmSync(bundle, { force: true });
  writeSyncState?.(root, m.name, { sha: snap.sha, rdb: rdbHash });
  const ms = Date.now() - t0;
  const what = [
    slot.prep.fresh ? "new run folder" : "reused run folder",
    bundle ? `bundle ${(bundleBytes / 1024).toFixed(1)} KiB` : "no bundle (box has the snapshot)",
    sendRdb ? toSend.map((r) => `rdb${r.name === CORE ? "" : ` ${r.name}`} ${(statSync(r.path).size / 1048576).toFixed(1)} MiB`).join(", ") : rdbs.length ? "rdb unchanged" : "no local rdb",
    slot.prep.fetched ? "box fetched origin" : null,
  ].filter(Boolean).join(", ");
  console.log(`pool: sync to ${m.name}: ${what} (${(ms / 1000).toFixed(1)} s)`);
  return { ok: true, sentBundle: !!bundle, bundleBytes, sentRdb: !!sendRdb, ms, bundleName: bundle ? basename(bundle) : null };
}

// Every addon's local resourceDatabase.rdb: { name, path, hash, wire } (wire is the file name it
// travels under; the box's psCheckout moves it into addons/<name>/).
function localRdbs(root) {
  return ADDONS.map(({ name }) => ({ name, path: join(root, "addons", name, "resourceDatabase.rdb") }))
    .filter((r) => existsSync(r.path))
    .map((r) => ({ ...r, hash: sha1File(r.path), wire: r.name === CORE ? "resourceDatabase.rdb" : `resourceDatabase.${r.name}.rdb` }));
}

// The path scp sends: the core's rdb as it is, another addon's copied under its wire name.
function stageRdb(root, r) {
  if (r.name === CORE) return r.path;
  const out = join(root, "tmp", "remote", r.wire);
  mkdirSync(dirname(out), { recursive: true });
  copyFileSync(r.path, out);
  return out;
}

// Exit code of psCheckout when the bundle cannot be applied, even after a fetch of origin.
export const BUNDLE_FAILED = 4;

// The PowerShell that checks a synced snapshot out in the run folder.
function psCheckout(slot, bundleName, slug) {
  return [
    `$dir = ${psQuote(slot.rDir)}; $sha = ${psQuote(slot.snap.sha)}; $ref = ${psQuote(`refs/lf-remote/${slug}`)}`,
    "Set-Location -LiteralPath $dir",
    "$in = Join-Path $dir 'tmp\\remote-in'",
    bundleName ? `$bundle = Join-Path $in ${psQuote(bundleName)}` : "$bundle = $null",
    "function LfGit { $o = (& git @args 2>&1 | ForEach-Object { \"$_\" }) -join ' | '; $global:LfGitOut = $o }",
    "if ($bundle -and (Test-Path -LiteralPath $bundle)) {",
    "  LfGit fetch -q $bundle $ref",
    "  if ($LASTEXITCODE) {",
    "    Write-Host \"lf-remote: git fetch of the bundle failed: $LfGitOut\"",
    "    # A crash (blue screen) can leave loose ref files full of NULs; git then refuses every bundle (\"bad object <ref>\"). Delete them; the fetch below restores them.",
    "    $cd = (git rev-parse --git-common-dir); foreach ($rf in @(Get-ChildItem -LiteralPath (Join-Path $cd 'refs') -Recurse -File -ErrorAction SilentlyContinue)) { $t = ([IO.File]::ReadAllText($rf.FullName) -replace \"`0\", '').Trim(); if ($t -notmatch '^([0-9a-f]{40}|ref: .+)$') { Write-Host \"lf-remote: dropping corrupt ref file $($rf.FullName) ($($rf.Length) bytes)\"; Remove-Item -LiteralPath $rf.FullName -Force } }",
    "    Write-Host 'lf-remote: fetching origin on the box, retrying the bundle'",
    "    LfGit fetch -q origin",
    "    LfGit fetch -q $bundle $ref",
    "    if ($LASTEXITCODE) { Write-Host \"lf-remote: bundle-fetch-failed: $LfGitOut\"; exit " + BUNDLE_FAILED + " }",
    "  }",
    "  Remove-Item -LiteralPath $bundle -Force",
    "}",
    "git checkout -q --detach -f $sha 2>&1 | Out-Null; if ($LASTEXITCODE) { Write-Host \"lf-remote: checkout $sha failed\"; exit 3 }",
    "$rdbIn = Join-Path $in 'resourceDatabase.rdb'",
    "if (Test-Path -LiteralPath $rdbIn) { Move-Item -LiteralPath $rdbIn -Destination (Join-Path $dir 'addons\\LifeFramework\\resourceDatabase.rdb') -Force }",
    // Another addon's rdb travels as resourceDatabase.<name>.rdb (localRdbs).
    "foreach ($f in @(Get-ChildItem -LiteralPath $in -Filter 'resourceDatabase.*.rdb' -File -ErrorAction SilentlyContinue)) { $an = $f.Name.Substring(17, $f.Name.Length - 21); $ad = Join-Path $dir \"addons\\$an\"; if (Test-Path -LiteralPath $ad) { Move-Item -LiteralPath $f.FullName -Destination (Join-Path $ad 'resourceDatabase.rdb') -Force } }",
  ].join("\n");
}

// Files a run takes to the box and brings back (the placement audit: its input list goes up through an
// env var, its out.txt comes down), as the scp pairs and the env lines to run with. files:
// { env: { NAME: localFile }, down: [{ from: path under the run folder, to: local path }] }. Pure.
export function remoteFilePlan(rDir, files = {}) {
  const base = String(rDir).replace(/\\/g, "/").replace(/\/+$/, "");
  const uploads = [];
  const env = [];
  for (const [name, local] of Object.entries(files.env ?? {})) {
    const to = `${base}/tmp/remote-in/${basename(local)}`;
    uploads.push({ from: local, to });
    env.push(`$env:${name} = ${psQuote(to.replace(/\//g, "\\"))}`);
  }
  const downloads = (files.down ?? []).map((d) => ({ from: `${base}/${d.from}`, to: d.to }));
  return { uploads, env, downloads };
}

// Syncs, then runs `node <script> <args>` in the slot's run folder (cliArgs[0]
// is the script path relative to the repo, default tools/cli.mjs). Returns
// { code, summary, localLog }. code 3: the box could not be prepared. files: remoteFilePlan.
export async function runRemote(root, slot, { args, script = "tools\\cli.mjs", writeSyncState, fetchLog = true, files }) {
  const m = slot.machine;
  const slug = basename(slot.rDir).replace(/^Life-Framework-rt-/, "");
  const snap = slot.snap;
  console.log(`remote: ${m.name} ${slot.rDir} @ ${snap.sha.slice(0, 10)}${snap.dirty ? ` (HEAD ${snap.head.slice(0, 10)} + ${snap.changed} uncommitted path(s))` : ""}`);
  let sync;
  let summaryLine = "";
  let remoteLog = "";
  let code = 0;
  for (let attempt = 0; attempt < 2; attempt++) {
    sync = syncToSlot(root, slot, { writeSyncState });
    if (!sync.ok) return { code: 3 };
    const quiet = "$env:LF_QUIET_CHILD = '1'"; // this run is parsed here, so the box prints the full transcript
    const env0 = slot.lockName ? `$env:LF_SERVER_SLOT_HELD = ${psQuote(`${slot.lockName}:${slot.token}`)}` : "";
    const plan = remoteFilePlan(slot.rDir, files);
    for (const u of plan.uploads) {
      const up = scp([u.from, `${m.host}:${u.to}`]);
      if (up.status !== 0) {
        console.log(`FAIL  could not copy ${u.from} to ${m.name}: ${(up.stderr || up.stdout).trim()}`);
        return { code: 3 };
      }
    }
    const ps = [
      psCheckout(slot, sync.bundleName, slug),
      quiet,
      env0,
      ...plan.env,
      `& node ${psQuote(script)} ${args.map(psQuote).join(" ")}`,
      "$code = $LASTEXITCODE",
      "$ld = Get-ChildItem -LiteralPath (Join-Path $dir 'server\\profile\\test\\logs') -Directory -ErrorAction SilentlyContinue | Sort-Object LastWriteTime | Select-Object -Last 1",
      "if ($ld -and (Test-Path -LiteralPath (Join-Path $ld.FullName 'console.log'))) { Write-Host \"lf-remote: console.log $($ld.FullName)\\console.log\" }",
      "exit $code",
    ].join("\n");
    code = await sshPsStream(m.host, ps, (line) => {
      if (/\[LFTEST\] SUMMARY tier=/.test(line)) summaryLine = line;
      const mm = /^lf-remote: console\.log (.+)$/.exec(line.trim());
      if (mm) remoteLog = mm[1].trim();
    });
    if (code !== BUNDLE_FAILED || !sync.sentBundle || attempt) break;
    // The box could not apply the delta bundle even after fetching origin: its
    // history does not match what the sync state claimed. Forget that state and
    // send a self-contained bundle (no prerequisites).
    console.log(`remote: ${m.name} could not apply the delta bundle; clearing its sync state and sending a full bundle`);
    writeSyncState?.(root, m.name, { sha: null, rdb: null });
    slot.prep.has = new Set();
  }
  if (slot.lost) console.log(`WARN  the slot on ${m.name} was lost during the run; its verdict is not trusted`);
  for (const d of remoteFilePlan(slot.rDir, files).downloads) {
    mkdirSync(dirname(d.to), { recursive: true });
    const down = scp([`${m.host}:${d.from}`, d.to]);
    if (down.status !== 0) console.log(`WARN  could not fetch ${d.from} from ${m.name}: ${(down.stderr || down.stdout).trim()}`);
  }

  let localLog = "";
  if (remoteLog && fetchLog) {
    const logDir = basename(dirname(remoteLog.replace(/\\/g, "/")));
    const dest = join(root, "server", "profile", "test", "logs", `remote-${m.name}-${logDir}`);
    mkdirSync(dest, { recursive: true });
    const down = scp([`${m.host}:${remoteLog.replace(/\\/g, "/")}`, join(dest, "console.log")]);
    if (down.status === 0) localLog = join(dest, "console.log");
    else console.log(`WARN  could not fetch ${remoteLog}: ${(down.stderr || down.stdout).trim()}`);
  }
  if (localLog) {
    const engine = readFileSync(localLog, "utf8").split(/\r?\n/, 40).find((l) => /Initializing engine, version/.test(l));
    if (engine) console.log(`remote: ${m.name} ${engine.replace(/^.*?ENGINE\s*:\s*/, "").trim()}`);
  }
  if (code !== 0 && localLog) {
    const key = readFileSync(localLog, "utf8").split(/\r?\n/).filter((l) => KEY_LINE.test(l)).slice(0, 15);
    if (key.length) {
      console.log(`remote: key lines from the ${m.name} console.log:`);
      for (const l of key) console.log(`  ${l.trim()}`);
    }
  }
  const summary = summaryLine ? appendStamp(root, summaryLine, snap.head, localLog, m.name) : null;
  if (localLog) console.log(`remote: log ${localLog}`);
  return { code: slot.lost && code === 0 ? 3 : code, summary, localLog, sync };
}

// Sync only (no slot): used by sessions, whose server takes the box's lock itself.
export function psCheckoutOnly(slot, bundleName) {
  return psCheckout(slot, bundleName, basename(slot.rDir).replace(/^Life-Framework-rt-/, ""));
}

// Removes run folders on a box through the junction-safe helper of the box's
// own clone (tools/link-safety.mjs removeWorktree unlinks every junction first,
// so the game-addons links into the server install are never followed).
export function removeRemoteDirs(host, mainRoot, dirs) {
  const res = sshPs(host, [
    `$main = ${psQuote(mainRoot)}`,
    `$dirs = @(${dirs.map(psQuote).join(",")})`,
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
  const res = spawnSync("ssh", [...sshOpts(), host, `powershell -NoProfile -ExecutionPolicy Bypass -File ${parent}/provision-testbox.ps1 -RepoDir ${rRoot}${extra}; exit $LASTEXITCODE`], { stdio: "inherit", windowsHide: true });
  if (res.status !== 0) return res.status ?? 1;
  // The box has no Workbench to build resourceDatabase.rdb; give its main
  // clone this checkout's copy so runs started on the box find one.
  for (const { name, path: rdb } of localRdbs(wt.mainRootOf(root))) {
    const r = scp([rdb, `${host}:${rRoot}/addons/${name}/resourceDatabase.rdb`]);
    console.log(r.status === 0 ? `testbox: sent ${name} resourceDatabase.rdb (${statSync(rdb).size} bytes)` : `WARN  ${name} rdb copy failed: ${(r.stderr || r.stdout).trim()}`);
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
    console.log("usage: tools\\cli testbox prune <slug>|--all   (tools\\cli pool prune removes the ones whose worktree is gone)");
    return 2;
  }
  const list = sshPs(host, [
    `$main = ${psQuote(rRoot)}`,
    "git -C $main worktree list --porcelain | Where-Object { $_ -like 'worktree *' } | ForEach-Object { Write-Host (\"lf-remote: wt \" + $_.Substring(9)) }",
  ].join("\n"), { timeout: 60000 });
  const dirs = (list.stdout ?? "").split(/\r?\n/).map((l) => /^lf-remote: wt (.+)$/.exec(l.trim())?.[1]).filter(Boolean).filter((d) => {
    const leaf = d.replace(/\\/g, "/").split("/").pop();
    if (!leaf.startsWith("Life-Framework-rt-")) return false;
    if (all) return true;
    const rest = leaf.slice("Life-Framework-rt-".length);
    return rest === slug || rest === `${slug}-session` || new RegExp(`^${slug.replace(/[.*+?^${}()|[\]\\]/g, "\\$&")}(-session)?-s\\d+$`).test(rest);
  });
  if (dirs.length === 0) {
    console.log("testbox: nothing to prune");
    return 0;
  }
  return removeRemoteDirs(host, rRoot, dirs);
}

export function cmdTestbox(root, args) {
  const [sub, ...rest] = args;
  const hi = rest.indexOf("--host");
  const host = (hi >= 0 ? rest[hi + 1] : undefined) || configuredHost(root);
  const flags = hi >= 0 ? rest.filter((_, i) => i !== hi && i !== hi + 1) : rest;
  if (!sub || sub === "help" || !["provision", "status", "prune", "screenshot", "record", "start", "vdd"].includes(sub)) {
    console.log(`usage: tools\\cli testbox provision|status|prune|screenshot|record|start|vdd [--host <ssh-host>]
  provision [--skip-server-update]  install Git/Node (if missing), SteamCMD + Arma Reforger Server
                                    (anonymous), clone the repo, write lf.local.json; send the rdb
  status                            reachability, server build, the box's server-2001 lock, run dirs
  prune <slug>|--all                remove run worktrees on the box (junction-safe)
  screenshot                        png of the box's desktop -> tmp/testbox/
  record [secs] [--gdigrab]         mp4 of the box's screen + system audio (default 10 s) -> tmp/testbox/
  start <exe> [args...]             start a program in the box's desktop session (a game client)
  vdd                               install the virtual display driver (no reboot)
host: --host, else LF_TESTBOX (env or lf.local.json). Pool view of every machine: tools\\cli pool status`);
    return sub && sub !== "help" ? 2 : 0;
  }
  if (!host) {
    console.log("FAIL  no test box: pass --host <ssh-host> or set LF_TESTBOX in the env or lf.local.json");
    return 2;
  }
  if (sub === "provision") return cmdProvision(root, host, flags);
  if (sub === "status") return cmdStatus(root, host);
  if (sub === "screenshot") return cmdScreenshot(root, host);
  if (sub === "record") return cmdRecord(root, host, flags);
  if (sub === "start") return cmdStart(root, host, flags);
  if (sub === "vdd") return cmdVdd(root, host);
  return cmdPrune(root, host, flags);
}

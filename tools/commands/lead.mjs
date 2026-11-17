// tools\cli lead: the lead agent's one-screen dispatch view, so it stops gathering state with ad-hoc commands.
//
//   tools\cli lead [--prs N]          milestones, PRs, worktrees, pool, ship queue, accounts, slices (about 40 lines, under 15 s)
//   tools\cli lead prune-safe [--dry-run] [--oldest N]   prune only worktrees whose PR is MERGED on GitHub, clean, idle and unlocked
//                                     (--oldest N: only the N least recently active of those; the rules do not change)
//
// The sections run in parallel, each with its own timeout; one that fails or times out prints one line and the rest
// still print. The pool section reads each machine in its own process, in parallel, with a per-machine timeout
// (LF_LEAD_POOL_MACHINE_MS, 9 s) inside an overall budget (LF_LEAD_POOL_BUDGET_MS, 12 s): a slow machine prints "slow"
// or "DOWN" beside the rest, and the budget line prints when it is hit. The classification is pure and lives in tools/lib/lead.mjs (proved by tools/validation/lead-selftest.mjs).
// A worktree counts as merged only through a merged PR whose head SHA is the branch tip, never through git ancestry:
// a fresh worktree is an ancestor of main and must never be listed.
import { execFile } from "node:child_process";
import { existsSync, lstatSync, opendirSync, readFileSync, readdirSync, writeFileSync, writeSync, mkdirSync } from "node:fs";
import { basename, dirname, join, normalize } from "node:path";
import * as wt from "../wt.mjs";
import { predictConflicts } from "../prs.mjs";
import {
  classifyWorktree, conflictHint, gatherPool, isConflicting, milestoneCounts, milestoneLine, newestActivityMs, poolLines,
  prLine, shipQueueLine, sortPrs, timeoutHealth, unregisteredDirs, worktreeSummary,
} from "../lib/lead.mjs";
import { readQueue } from "../lib/ship-queue.mjs";
import { diskLines, oldestFirst, parseOldest } from "../lib/disk-guard.mjs";

export const help = "lead [--prs N] | lead prune-safe [--dry-run] [--oldest N]   one-screen dispatch view: milestones, PRs, worktrees, pool, ship queue, accounts, slices";


// ---------------------------------------------------------------- process helpers

function sh(cmd, args, { cwd, timeout = 12000 } = {}) {
  return new Promise((resolve) => {
    execFile(cmd, args, { cwd, timeout, windowsHide: true, encoding: "utf8", maxBuffer: 64 * 1024 * 1024 }, (err, stdout, stderr) => {
      resolve({
        code: err ? (typeof err.code === "number" ? err.code : 1) : 0,
        out: stdout ?? "",
        err: stderr ?? "",
        timedOut: !!(err && (err.killed || err.code === "ETIMEDOUT")),
      });
    });
  });
}

async function ghJson(gh, root, args, timeout) {
  const r = await sh(gh, args, { cwd: root, timeout });
  if (r.timedOut) throw new Error(`gh timed out after ${Math.round(timeout / 1000)} s`);
  if (r.code !== 0) throw new Error(`gh failed: ${(r.err || r.out).trim().split("\n")[0].slice(0, 120)}`);
  try {
    return JSON.parse(r.out || "[]");
  } catch {
    throw new Error("gh output was not JSON");
  }
}

async function pooled(items, limit, fn) {
  const out = new Array(items.length);
  let next = 0;
  await Promise.all(Array.from({ length: Math.min(limit, items.length) }, async () => {
    while (next < items.length) {
      const i = next++;
      out[i] = await fn(items[i]);
    }
  }));
  return out;
}

// A section is a function returning lines; a throw or a timeout becomes one line, never a crash.
async function section(name, ms, fn) {
  let timer;
  const timeout = new Promise((res) => { timer = setTimeout(() => res([`${name} unavailable: timed out after ${Math.round(ms / 1000)} s`]), ms); });
  try {
    return await Promise.race([fn(), timeout]);
  } catch (e) {
    return [`${name} unavailable: ${String(e?.message ?? e).split("\n")[0].slice(0, 140)}`];
  } finally {
    clearTimeout(timer);
  }
}

// ---------------------------------------------------------------- milestones

async function milestonesSection(gh, root) {
  const issues = await ghJson(gh, root, ["issue", "list", "--state", "open", "--limit", "1000", "--json", "number,labels,milestone"], 12000);
  const counts = milestoneCounts(issues);
  return ["MILESTONES", ...Object.entries(counts).map(([k, c]) => `  ${milestoneLine(k, c)}`)];
}

// ---------------------------------------------------------------- pull requests

const PR_FIELDS = "number,title,headRefName,mergeStateStatus,mergeable,statusCheckRollup,createdAt,isDraft";

async function prsSection(gh, root, maxLines) {
  const [list] = await Promise.all([
    ghJson(gh, root, ["pr", "list", "--state", "open", "--limit", "100", "--json", PR_FIELDS], 12000),
    sh("git", ["-C", root, "fetch", "origin", "--prune", "--quiet"], { timeout: 9000 }),
  ]);
  if (!list.length) return ["PRS none open"];
  // UNKNOWN means GitHub has not computed mergeability yet; the REST endpoint makes it.
  await pooled(list.filter((p) => p.mergeStateStatus === "UNKNOWN"), 5, async (p) => {
    const r = await sh(gh, ["api", `repos/{owner}/{repo}/pulls/${p.number}`, "--jq", ".mergeable_state"], { cwd: root, timeout: 8000 });
    const v = r.code === 0 ? r.out.trim().toUpperCase() : "";
    if (v && v !== "UNKNOWN" && v !== "NULL") p.mergeStateStatus = v;
  });
  const now = Date.now();
  const sorted = sortPrs(list);
  const conflicting = sorted.filter(isConflicting).slice(0, 10);
  const hints = new Map();
  await pooled(conflicting, 5, async (p) => {
    let files = null;
    try {
      files = predictConflicts(root, "origin/main", `origin/${p.headRefName}`);
    } catch { /* branch not fetched */ }
    hints.set(p.number, conflictHint(files));
  });
  const tally = {};
  for (const p of sorted) {
    const k = isConflicting(p) ? "CONFLICT" : p.isDraft ? "DRAFT" : p.mergeStateStatus;
    tally[k] = (tally[k] ?? 0) + 1;
  }
  const head = `PRS ${list.length} open: ${Object.entries(tally).map(([k, n]) => `${n} ${k}`).join(", ")}`;
  const lines = sorted.map((p) => `  ${prLine(p, now, hints.get(p.number))}`);
  if (lines.length > maxLines) lines.splice(maxLines, lines.length - maxLines, `  +${sorted.length - maxLines} more (tools\\cli prs status)`);
  return [head, ...lines];
}

// ---------------------------------------------------------------- worktrees

function warmPaths(state) {
  const out = [];
  const walk = (v) => {
    if (!v || typeof v !== "object") return;
    if (typeof v.path === "string") out.push(v.path);
    for (const x of Object.values(v)) if (x && typeof x === "object") walk(x);
  };
  walk(state.warm);
  return out;
}

function mtimeOf(p) {
  try { return lstatSync(p).mtimeMs; } catch { return 0; }
}

function childrenOf(dir) {
  try { return readdirSync(dir).map((n) => join(dir, n)); } catch { return []; }
}

// Newest agent change in one worktree: its last HEAD reflog entry, files git reports changed or untracked,
// tmp/ (and the files in tmp/logs, which grow without touching their folder) and the session logs. Shallow, never
// follows a junction (same rule as prs.mjs lastActivity). The registry marker tmp/wt.json is bookkeeping, not
// activity (newestActivityMs drops it).
async function inspect(e) {
  const exists = existsSync(e.path);
  const isGit = exists && existsSync(join(e.path, ".git"));
  const base = { ...e, exists, isGit, dirty: false, lastMs: 0 };
  if (!isGit) return base;
  const [st, rl] = await Promise.all([
    sh("git", ["-C", e.path, "status", "--porcelain", "-uall"], { timeout: 12000 }),
    sh("git", ["-C", e.path, "reflog", "-1", "--format=%ct", "HEAD"], { timeout: 8000 }),
  ]);
  if (st.code !== 0) return { ...base, isGit: false };
  const paths = [];
  let dirty = false;
  for (const line of st.out.split(/\r?\n/)) {
    if (line.length < 4) continue;
    dirty = true;
    let f = line.slice(3);
    if (f.includes(" -> ")) f = f.split(" -> ")[1];
    paths.push(join(e.path, f.replace(/^"|"$/g, "")));
  }
  paths.push(...childrenOf(join(e.path, "tmp")), ...childrenOf(join(e.path, "tmp", "logs")));
  for (const scope of childrenOf(join(e.path, "server", "profile"))) paths.push(...childrenOf(join(scope, "logs")));
  const lastMs = newestActivityMs(Number(rl.out.trim()) * 1000, paths.map((p) => ({ path: p, mtimeMs: mtimeOf(p) })));
  return { ...base, dirty, lastMs };
}

// Everything the worktree section and prune-safe share. Throws when the merged-PR list is unknown.
export async function gatherWorktrees(root, gh) {
  const main = wt.mainRootOf(root);
  const state = wt.readState(root);
  const listed = wt.listWorktrees(root).filter((w) => normalize(w.path).toLowerCase() !== normalize(main).toLowerCase());
  const [merged, refs] = await Promise.all([
    ghJson(gh, root, ["pr", "list", "--state", "merged", "--limit", "500", "--json", "headRefName,headRefOid,baseRefName,number"], 14000),
    sh("git", ["-C", root, "for-each-ref", "--format=%(refname:short) %(objectname)", "refs/heads/"], { timeout: 8000 }),
  ]);
  const tips = new Map(refs.out.split(/\r?\n/).filter(Boolean).map((l) => l.split(" ")));
  const byPath = new Map(Object.entries(state.worktrees ?? {}).map(([slug, e]) => [normalize(e.path).toLowerCase(), { slug, e }]));
  const entries = listed.map((w) => {
    const reg = byPath.get(normalize(w.path).toLowerCase());
    return {
      slug: reg?.slug ?? basename(w.path).replace(/^.*-ws-/, ""),
      registered: !!reg,
      path: w.path,
      branch: w.branch,
      base: reg ? wt.baseOf(reg.e) : "main",
      tip: w.branch ? tips.get(w.branch) : undefined,
    };
  });
  const inspected = await pooled(entries, 10, inspect);
  const now = Date.now();
  const rows = inspected.map((e) => {
    // The lock check is a few file reads; ask only about trees that would otherwise be prunable.
    let busy = null;
    const probe = classifyWorktree({ ...e, busy: null }, merged, now);
    if (probe.cls === "prunable") busy = wt.worktreeBusy(root, { path: e.path });
    return { ...e, busy, ...classifyWorktree({ ...e, busy }, merged, now) };
  });
  // Registered in the worktree registry but git does not list it: a leftover entry.
  const inGit = new Set(listed.map((w) => normalize(w.path).toLowerCase()));
  for (const [slug, e] of Object.entries(state.worktrees ?? {})) {
    if (e.status === "creating" || inGit.has(normalize(e.path).toLowerCase())) continue;
    rows.push({ slug, path: e.path, branch: e.branch, cls: "orphan", why: existsSync(e.path) ? "registry only, not in git" : "registry only, dir missing" });
  }
  const parent = dirname(main);
  const prefix = `${basename(main)}-ws-`;
  const dirs = childrenOf(parent).filter((p) => basename(p).startsWith(prefix));
  const known = [...listed.map((w) => w.path), ...Object.values(state.worktrees ?? {}).map((e) => e.path), ...warmPaths(state)];
  const orphanDirs = unregisteredDirs(dirs, known);
  return { rows, orphanDirs, allDirs: [...new Set([...dirs])], main };
}

// Disk use of every worktree folder: cached per folder (24 h), refreshed oldest-first inside a deadline.
// The walk never follows a junction, so the Steam installs behind game-addons are not counted.
function sizeOf(dir, deadline) {
  let bytes = 0;
  const walk = (d) => {
    if (Date.now() > deadline) return false;
    let h;
    try { h = opendirSync(d); } catch { return true; }
    let e;
    try {
      while ((e = h.readSync())) {
        if (e.isSymbolicLink()) continue;
        const p = join(d, e.name);
        if (e.isDirectory()) {
          if (!walk(p)) return false;
        } else {
          try { bytes += lstatSync(p).size; } catch { /* vanished */ }
        }
      }
    } finally {
      h.closeSync();
    }
    return true;
  };
  return walk(dir) ? bytes : null;
}

export function diskUse(main, dirs, budgetMs) {
  const file = join(wt.hubDirOf(main), "lead-disk.json"); // the shared worktree hub (main/tmp/wt), gitignored
  let cache = {};
  try { cache = JSON.parse(readFileSync(file, "utf8")); } catch { /* first run */ }
  const now = Date.now();
  const deadline = now + Math.max(0, budgetMs);
  const stale = dirs.filter((d) => !cache[d] || now - cache[d].at > 24 * 3600000).sort((a, b) => (cache[a]?.at ?? 0) - (cache[b]?.at ?? 0));
  for (const d of stale) {
    if (Date.now() > deadline) break;
    const bytes = sizeOf(d, deadline);
    if (bytes !== null) cache[d] = { gb: bytes / 1073741824, at: Date.now() };
  }
  for (const k of Object.keys(cache)) if (!dirs.includes(k)) delete cache[k];
  try {
    mkdirSync(dirname(file), { recursive: true });
    writeFileSync(file, JSON.stringify(cache));
  } catch { /* a cache that cannot be written is only slower next time */ }
  const sized = dirs.filter((d) => cache[d]);
  const gb = sized.reduce((s, d) => s + cache[d].gb, 0);
  return { gb, sized: sized.length, total: dirs.length, estGb: sized.length ? (gb / sized.length) * dirs.length : 0 };
}

function names(list, max = 8) {
  const shown = list.slice(0, max).map((r) => r.slug ?? basename(r));
  return shown.join(", ") + (list.length > max ? ` +${list.length - max}` : "");
}

function worktreeLines(g) {
  const s = worktreeSummary(g.rows, g.orphanDirs);
  const orphanN = s.by.orphan.length + s.orphanDirs.length;
  const lines = [`WORKTREES ${g.rows.length + s.orphanDirs.length}: ${s.by.active.length} active | ${s.by.idle.length} idle | ${s.by.prunable.length} merged-prunable | ${s.by["merged-kept"].length} merged-held | ${orphanN} orphan`];
  if (s.by.active.length) lines.push(`  active: ${names(s.by.active, 10)}`);
  if (s.by.prunable.length) lines.push(`  prunable (PR merged, clean): ${names(s.by.prunable)}  -> tools\\cli lead prune-safe`);
  if (s.by["merged-kept"].length) lines.push(`  merged but held: ${s.by["merged-kept"].slice(0, 5).map((r) => `${r.slug} (${r.why})`).join(", ")}`);
  if (orphanN) {
    // One line per reason, names only: the reason is the same for most of them.
    const byWhy = new Map();
    for (const r of s.by.orphan) byWhy.set(r.why, [...(byWhy.get(r.why) ?? []), r.slug]);
    if (s.orphanDirs.length) byWhy.set("no registration", s.orphanDirs.map((d) => basename(d).replace(/^.*-ws-/, "")));
    for (const [why, slugs] of byWhy) lines.push(`  orphans (${why}): ${slugs.slice(0, 6).join(", ")}${slugs.length > 6 ? ` +${slugs.length - 6}` : ""}`);
  }
  return lines;
}

async function worktreesSection(gh, root) {
  return worktreeLines(await gatherWorktrees(root, gh));
}

// ---------------------------------------------------------------- pool

// One machine per child process (`lead --pool-worker <name>`): its ssh calls are synchronous, so a slow machine can
// only be cut off by killing its process. The worker prints one JSON snapshot per stage; the last complete one wins.
const NL = String.fromCharCode(10);

function poolProbe(root) {
  return (m, { report, signal }) => new Promise((resolve) => {
    let buf = "";
    let last = null;
    const child = execFile(process.execPath, [join(root, "tools", "cli.mjs"), "lead", "--pool-worker", m.name], { cwd: root, windowsHide: true, encoding: "utf8", maxBuffer: 8 * 1024 * 1024 }, () => resolve(last));
    child.stdout.on("data", (d) => {
      buf += d;
      const lines = buf.split(NL);
      buf = lines.pop();
      for (const l of lines) {
        try { last = JSON.parse(l); report(last); } catch { /* not a snapshot line */ }
      }
    });
    signal.addEventListener("abort", () => { child.kill(); resolve(last); });
  });
}

const envMs = (name, dflt) => (/^[0-9]+$/.test(process.env[name] ?? "") ? Number(process.env[name]) : dflt);

// The machines' last snapshots, for the DISK block (poolSection fills it; each snapshot carries the machine's free disk).
let poolSnaps = [];

async function poolSection(root) {
  const pool = await import("../remote/pool.mjs");
  const machines = pool.loadMachines(root);
  if (!machines.length) throw new Error("pool has no machines (tools/pool.json)");
  const budgetMs = envMs("LF_LEAD_POOL_BUDGET_MS", 12000);
  const g = await gatherPool({
    machines,
    probe: poolProbe(root),
    machineMs: envMs("LF_LEAD_POOL_MACHINE_MS", 9000),
    budgetMs,
    fallback: (m) => timeoutHealth(pool.lastHealth(root, m.name), Date.now(), pool.RECENT_OK_MS),
  });
  poolSnaps = g.snaps;
  return poolLines({ snaps: g.snaps, queue: pool.liveTickets(root).length, exceeded: g.exceeded, budgetMs, elapsedMs: g.elapsedMs });
}

async function poolWorker(root, name) {
  const pool = await import("../remote/pool.mjs");
  const m = pool.findMachine(pool.loadMachines(root), name);
  if (!m) { console.log(JSON.stringify({ name, health: "DOWN", note: "not in the pool" })); return 1; }
  const emit = (snap) => writeSync(1, JSON.stringify(snap) + NL);
  emit(pool.poolSnapshot(root, m, emit));
  return 0;
}

// ---------------------------------------------------------------- ship queue

async function shipSection(root) {
  return [shipQueueLine(readQueue(wt.ensureHub(root)), Date.now())];
}

// ---------------------------------------------------------------- main guard (tools/main-guard.mjs)

async function guardSection(root) {
  const g = await import("../main-guard.mjs");
  const { statusLine } = await import("../lib/main-guard.mjs");
  return [statusLine(g.readState(root), { alive: g.workerAlive(root), off: process.env.LF_MAIN_GUARD === "0" })];
}

// ---------------------------------------------------------------- accounts, slices (other branches; skipped until they land)

async function accountsSection(root) {
  const has = ["tools/commands/usage.mjs", "tools/agents/usage.mjs"].some((f) => {
    try { return readFileSync(join(root, f), "utf8").includes("--accounts"); } catch { return false; }
  });
  if (!has) return ["ACCOUNTS skipped: `tools\\cli usage --accounts` is not on this branch yet"];
  const r = await sh(process.execPath, [join(root, "tools", "cli.mjs"), "usage", "--accounts"], { cwd: root, timeout: 12000 });
  if (r.timedOut) throw new Error("usage --accounts timed out");
  const lines = r.out.split(/\r?\n/).map((l) => l.trimEnd()).filter(Boolean);
  return ["ACCOUNTS", ...lines.slice(0, 4).map((l) => `  ${l}`)];
}

async function usageSection(root) {
  return (await import("../agents/usage-plan.mjs")).leadUsageLines(root);
}

async function slicesSection(root) {
  if (!existsSync(join(root, "tools", "commands", "slice.mjs"))) return ["SLICES skipped: `tools\\cli slice list --ready` is not on this branch yet"];
  const r = await sh(process.execPath, [join(root, "tools", "cli.mjs"), "slice", "list", "--ready"], { cwd: root, timeout: 12000 });
  if (r.timedOut) throw new Error("slice list timed out");
  const lines = r.out.split(/\r?\n/).map((l) => l.trimEnd()).filter(Boolean);
  return ["SLICES ready, by critical path (top 10)", ...lines.slice(0, 10).map((l) => `  ${l}`)];
}

// ---------------------------------------------------------------- commands

async function pruneSafe(root, args) {
  const gh = wt.ghBin();
  if (!gh) { console.log("FAIL  GitHub CLI not found"); return 1; }
  const dry = args.includes("--dry-run");
  const oldest = parseOldest(args);
  if (Number.isNaN(oldest)) { console.log("FAIL  --oldest needs a positive number, e.g. --oldest 20"); return 1; }
  let g;
  try {
    wt.git(root, ["fetch", "origin", "--prune", "--quiet"]);
    g = await gatherWorktrees(root, gh);
  } catch (e) {
    console.log(`WARN  cannot list merged PRs (${e.message}); pruning nothing. Ancestry alone never qualifies a worktree.`);
    return 1;
  }
  // Same rules for every row (MERGED PR, clean, idle, unlocked); --oldest N only caps how many go, least recently active first.
  const all = oldestFirst(g.rows.filter((r) => r.cls === "prunable"), null);
  const take = oldest === null ? all : all.slice(0, oldest);
  if (oldest !== null && all.length > take.length) console.log(`  --oldest ${oldest}: taking ${take.length} of ${all.length} that qualify`);
  for (const r of g.rows.filter((x) => x.cls === "merged-kept")) console.log(`  kept    ${r.slug}: ${r.via}, but ${r.why}`);
  if (!take.length) { console.log("prune-safe: nothing qualifies (needs a MERGED PR on GitHub, clean, idle 30 min, unlocked)"); return 0; }
  let failed = 0;
  const state = wt.readState(root);
  for (const r of take) {
    if (normalize(r.path).toLowerCase() === normalize(root).toLowerCase()) { console.log(`  kept    ${r.slug}: this is the running checkout`); continue; }
    if (!state.worktrees?.[r.slug]) { console.log(`  kept    ${r.slug}: not in the worktree registry (prune by hand)`); continue; }
    if (dry) { console.log(`  would prune ${r.slug} (${r.via})`); continue; }
    // Re-check right before the delete: the picture above is seconds old.
    if (wt.git(r.path, ["status", "--porcelain"]).stdout.trim()) { console.log(`  kept    ${r.slug}: became dirty`); continue; }
    const busy = wt.worktreeBusy(root, { path: r.path });
    if (busy) { console.log(`  kept    ${r.slug}: ${busy}`); continue; }
    try {
      const res = wt.wtPrune(root, r.slug); // never forced
      console.log(`  pruned  ${res.slug} (${r.via}): removed ${res.path} and branch ${res.branch}`);
    } catch (e) {
      failed++;
      console.log(`  FAIL    ${r.slug}: ${e.message}`);
    }
  }
  return failed ? 1 : 0;
}

// The size walk is synchronous, so it runs in its own process beside the others (`lead --disk-worker`).
function diskWorker(root) {
  const main = wt.mainRootOf(root);
  const dirs = childrenOf(dirname(main)).filter((p) => basename(p).startsWith(`${basename(main)}-ws-`));
  console.log(JSON.stringify(diskUse(main, dirs, 9000)));
  return 0;
}

export async function main(args, { root }) {
  if (args[0] === "--disk-worker") return diskWorker(root);
  if (args[0] === "--pool-worker") return poolWorker(root, args[1]);
  if (args[0] === "prune-safe") return pruneSafe(root, args.slice(1));
  const t0 = Date.now();
  const gh = wt.ghBin();
  const pi = args.indexOf("--prs");
  const maxPrs = pi >= 0 && /^\d+$/.test(args[pi + 1] ?? "") ? Number(args[pi + 1]) : 12;
  const needGh = (name, fn) => (gh ? fn() : Promise.resolve([`${name} unavailable: GitHub CLI not found`]));
  const sections = Promise.all([
    section("MILESTONES", 13000, () => needGh("MILESTONES", () => milestonesSection(gh, root))),
    section("PRS", 13000, () => needGh("PRS", () => prsSection(gh, root, maxPrs))),
    section("WORKTREES", 14000, () => needGh("WORKTREES", async () => worktreeLines(await gatherWorktrees(root, gh)))),
    section("POOL", 13500, () => poolSection(root)),
    section("USAGE", 13000, () => usageSection(root)),
    section("SHIP QUEUE", 5000, () => shipSection(root)),
    section("MAIN GUARD", 3000, () => guardSection(root)),
    section("ACCOUNTS", 13000, () => accountsSection(root)),
    section("SLICES", 13000, () => slicesSection(root)),
  ]);
  // Disk: its own process (the walk is synchronous), cached sizes refreshed inside a deadline; appended to the worktree block.
  const diskP = sh(process.execPath, [join(root, "tools", "cli.mjs"), "lead", "--disk-worker"], { cwd: root, timeout: 12500 });
  const [ms, prs, wts, pool, usage, ship, guard, acc, sl] = await sections;
  let diskLine;
  try {
    const dr = await diskP;
    const d = JSON.parse(dr.out.trim().split("\n").pop());
    const partial = d.sized < d.total;
    const used = partial ? `~${d.estGb.toFixed(0)}` : d.gb.toFixed(1);
    const note = partial ? ` (${d.sized}/${d.total} measured, the rest next run)` : "";
    diskLine = `  disk: worktrees ${used} GB${note}`;
  } catch {
    diskLine = "  disk: unavailable (size walk failed)";
  }
  wts.push(diskLine);
  // DISK: free space on this PC and on each pool machine, a warning under 30 GB (`wt new` refuses under 10 GB).
  const disk = diskLines([
    { name: "this PC", freeGb: wt.freeDiskGbAt(wt.mainRootOf(root)) },
    ...poolSnaps.filter((m) => m.via !== "local").map((m) => ({ name: m.name, freeGb: Number.isFinite(m.diskGb) ? m.diskGb : null, note: !m.health || m.health === "up" ? undefined : m.health })),
  ]);
  const out = [...ms, ...prs, ...wts, ...disk, ...pool, ...usage, ...ship, ...guard, ...acc, ...sl];
  out.push(`lead ${((Date.now() - t0) / 1000).toFixed(1)} s`);
  console.log(out.join("\n"));
  return 0;
}

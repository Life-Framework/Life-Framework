// tools/lib/quiet-run.mjs - one compact summary per heavy command, the full output in a log file.
//
// A heavy command (validate, test, session up, a proof, wt ship, ...) ran its whole
// transcript into the agent's context, and agents piped it through `tail` by hand and
// sometimes cut the line they needed. Now the entry script re-runs itself as a child
// with the output going to tmp/logs/<label>-<time>.log, and prints:
//
//   SUMMARY <label> ok|fail exit=N 12.3s lines=267 failures=0 warnings=23 log=<path>
//   <failing items: file:line and the exact error>
//   <warnings that change a decision, capped>
//   <the command's own verdict lines (SUMMARY, PR url, session ready, ...)>
//
// `--verbose` (or LF_VERBOSE=1) runs the command as before, full output. Output of at most
// PASSTHROUGH lines that exits 0 is printed as it is (it is already compact). A failure,
// a timeout and a warning are never dropped: they are counted, and shown up to a cap
// with the rest named in the log.
//
// Nested runs inherit LF_QUIET_CHILD, so a command that runs other commands (wt gate runs
// validate and test) gets their raw output into its own log, not into the agent's context.

import { spawn } from "node:child_process";
import { closeSync, existsSync, mkdirSync, openSync, readdirSync, readFileSync, rmSync, statSync, writeSync } from "node:fs";
import { basename, dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

export const PASSTHROUGH = 10; // a successful run this short prints verbatim
export const MAX_FAILURE_LINES = 30;
export const MAX_WARN_LINES = 6;
export const MAX_KEEP_LINES = 6;
export const MAX_LINE = 240;
const LOG_KEEP_MS = 3 * 24 * 3600 * 1000;

// A line that is a failure. Upper-case words only: "failed=0" and "timeouts 0/100" are not.
const FAIL_RE = /^(FAIL|ERROR)\b|\b(FAIL|FAILED|ERROR|TIMEOUT|FATAL)\b|SCRIPT \(E\)|VME:|Unable to find|timed out|Traceback|Unhandled|-> (error|timeout)\b|\bExit code [1-9]/;
// A warning that changes a decision: validate WARN lines, a pool cap/reclaim, a stale or differing machine.
const WARN_RE = /^WARN\b|^pool: .*(past its|warned|closes in|reclaimed|no server process)|\bSTALE\b|\(DIFFERS\)|\bDOWN\b|queued behind|is lowspec/;
// validate prints the output of its self-tests too (pool and merge fixtures): only its own WARN lines count.
const VALIDATE_WARN_RE = /^WARN\b/;
// The command's own verdict: kept when the run passes, so the agent never needs the log for them.
const KEEP_RE = /^(SUMMARY|validate:|incremental:|slowest:|test-e2e: (SUMMARY|OK|FAIL|log|feature debug)|test: pass cache|pool: .* on .*: exit|session: (ready|nobody|\d+ (request|session)|full log)|look: \d+\/\d+|PR |gate:|ship:|merged|checks)|https:\/\/github\.com\/\S+\/pull\/\d+|\bproof: (PASS|FAIL)|^town (ok|FAILED)/;

// A reused test pass (tools/lib/test-cache.mjs): always shown, so a gate never hides that its suite did not boot.
const CACHED_RE = /^test: CACHED pass\b/;

function clip(line, n = MAX_LINE) {
  const s = line.replace(/\s+$/, "");
  return s.length > n ? `${s.slice(0, n - 1)}...` : s;
}

// Pure: turn a captured transcript into the lines to print. Exported for the selftest.
export function summarize({ label, code, text, logPath, seconds }) {
  const lines = text.split(/\r?\n/);
  while (lines.length && lines[lines.length - 1] === "") lines.pop();
  const failures = [];
  const warns = [];
  const keeps = [];
  const skips = [];
  const cached = [];
  let inFailBlock = 0;
  let fixtureBlock = false; // the output of a passing *-selftest.mjs is fixtures (pool, merge), not this run
  for (const raw of lines) {
    const line = raw.replace(/\r$/, "");
    const header = /^(PASS|FAIL)\b/.exec(line);
    if (header) {
      inFailBlock = header[1] === "FAIL" ? 12 : 0;
      fixtureBlock = header[1] === "PASS" && line.includes("selftest.mjs");
    }
    const skip = /^SKIP\s+(\S+)/.exec(line);
    if (skip) {
      skips.push(skip[1].replace("validate/", ""));
      continue;
    }
    if (!fixtureBlock && (label === "validate" ? VALIDATE_WARN_RE : WARN_RE).test(line)) {
      warns.push(line);
      continue;
    }
    if (FAIL_RE.test(line)) {
      failures.push(line);
      continue;
    }
    // The lines under a "FAIL validate/x" header are that check's own explanation.
    if (inFailBlock > 0 && !header) {
      failures.push(`  ${line}`);
      inFailBlock--;
      continue;
    }
    if (CACHED_RE.test(line)) cached.push(line);
    else if (KEEP_RE.test(line)) keeps.push(line);
  }
  const failed = code !== 0;
  const out = [];
  const secs = seconds === undefined ? "" : ` ${seconds.toFixed(1)}s`;
  out.push(`SUMMARY ${label} ${failed ? "fail" : "ok"} exit=${code}${secs} lines=${lines.length} failures=${failures.length} warnings=${warns.length} log=${logPath}`);
  if (!failed && lines.length <= PASSTHROUGH) {
    // Already compact: the transcript itself is the report.
    return [out[0], ...lines.map((l) => clip(l))];
  }
  const failShown = failures.slice(0, MAX_FAILURE_LINES);
  for (const l of failShown) out.push(clip(l));
  if (failures.length > failShown.length) out.push(`... ${failures.length - failShown.length} more failure line(s) in ${logPath}`);
  const warnCap = label === "validate" ? 3 : MAX_WARN_LINES; // validate's advisory WARN lines are a backlog, not a decision
  const warnShown = warns.slice(0, warnCap);
  for (const l of warnShown) out.push(clip(l));
  if (warns.length > warnShown.length) out.push(`... ${warns.length - warnShown.length} more warning(s) in ${logPath}`);
  if (skips.length) out.push(`skipped ${skips.length} check(s): ${skips.join(", ")}`);
  for (const l of cached.slice(-4)) out.push(clip(l, 400));
  const keepShown = keeps.slice(-MAX_KEEP_LINES);
  for (const l of keepShown) out.push(clip(l));
  if (failed && failures.length === 0) {
    // A failure with no recognisable error line: show the end of the transcript, never nothing.
    out.push("(no error line recognised; the last lines of the transcript:)");
    for (const l of lines.slice(-12)) out.push(clip(l));
  }
  return out;
}

const VALUE_FLAGS = ["--client", "--clients", "--machine", "--profile", "--hold", "--mission", "--priority", "--tier", "--only", "--budget"];

// The words of a command line, without flags and the values of flags that take one.
export function positional(args) {
  const out = [];
  for (let i = 0; i < args.length; i++) {
    if (VALUE_FLAGS.includes(args[i])) { i++; continue; }
    if (!args[i].startsWith("--")) out.push(args[i]);
  }
  return out;
}

// Which invocations are quiet. null = print as always (a lookup, a status, a short command).
export function policy(script, args) {
  const name = basename(script);
  const [a, b] = positional(args);
  if (name === "cli.mjs") {
    // pack new|check|explain print their own short answer (tools/pack/); only the Workshop bundle is heavy.
    if (a === "pack" && ["new", "check", "explain"].includes(b)) return null;
    if (["validate", "test", "e2e", "build", "pack", "ci", "dev", "look", "shots", "audit"].includes(a)) return a;
    if (a === "session" && ["up", "down", "restart", "wait", "join"].includes(b)) return `session-${b}`;
    if (a === "wt" && ["gate", "ship", "e2e"].includes(b)) return `wt-${b}`;
    if (a === "asset" && ["run", "anim", "states", "kit", "sound", "dialogue"].includes(b)) return `asset-${b}`;
    return null;
  }
  if (name === "harness.mjs") {
    if (["flow", "shot", "chains", "journey", "town", "pillar", "play", "launch", "kit"].includes(a)) return `harness-${a}`;
    if (a === "report" && ["journey", "town"].includes(b)) return `harness-report-${b}`;
    return null;
  }
  if (name === "session.mjs") {
    // A proof verb is a file in agent-box/proofs/.
    if (a && /^[a-z][a-z0-9-]*$/.test(a) && existsSync(join(dirname(fileURLToPath(import.meta.url)), "..", "..", "agent-box", "proofs", `${a}.mjs`))) return a;
    if (a === "reset") return "session-reset";
    return null;
  }
  return null;
}

function repoRoot(script) {
  let dir = dirname(resolve(script));
  for (let i = 0; i < 4; i++) {
    if (existsSync(join(dir, "tools", "cli.mjs"))) return dir;
    dir = dirname(dir);
  }
  return process.cwd();
}

function pruneLogs(dir) {
  try {
    const now = Date.now();
    for (const f of readdirSync(dir)) {
      const p = join(dir, f);
      if (now - statSync(p).mtimeMs > LOG_KEEP_MS) rmSync(p, { force: true });
    }
  } catch {}
}

function stamp() {
  const d = new Date();
  const p = (n, w = 2) => String(n).padStart(w, "0");
  return `${d.getFullYear()}${p(d.getMonth() + 1)}${p(d.getDate())}-${p(d.getHours())}${p(d.getMinutes())}${p(d.getSeconds())}`;
}

// Entry point, awaited by tools/lib/quiet-gate.mjs before the real script loads.
export async function gate() {
  const script = process.argv[1];
  if (!script) return;
  const args = process.argv.slice(2);
  const verbose = args.includes("--verbose") || process.env.LF_VERBOSE === "1";
  if (verbose) {
    process.argv = process.argv.filter((a, i) => i < 2 || a !== "--verbose");
    process.env.LF_QUIET_CHILD = "1";
    process.env.LF_OUTPUT = "full"; // a command with a compact default checks this
    return;
  }
  if (process.env.LF_QUIET_CHILD) return;
  const label = policy(script, args);
  if (!label) return;
  const root = repoRoot(script);
  const dir = join(root, "tmp", "logs");
  mkdirSync(dir, { recursive: true });
  pruneLogs(dir);
  const logPath = join(dir, `${label}-${stamp()}.log`);
  const fd = openSync(logPath, "w");
  const started = Date.now();
  const code = await new Promise((done) => {
    const child = spawn(process.execPath, [script, ...args], {
      stdio: ["inherit", fd, fd],
      env: { ...process.env, LF_QUIET_CHILD: "1", LF_OUTPUT: "log" },
      windowsHide: true,
    });
    child.on("error", (e) => {
      writeSync(fd, `quiet-run: could not start ${script}: ${e.message}\n`);
      done(1);
    });
    child.on("exit", (c, sig) => done(typeof c === "number" ? c : sig ? 1 : 1));
  });
  try { closeSync(fd); } catch {}
  let text = "";
  try { text = readFileSync(logPath, "utf8"); } catch {}
  const shown = logPath.startsWith(root) ? logPath.slice(root.length + 1) : logPath;
  for (const l of summarize({ label, code, text, logPath: shown, seconds: (Date.now() - started) / 1000 })) console.log(l);
  process.exit(code);
}

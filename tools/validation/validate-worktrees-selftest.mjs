// Proves validate-worktrees.ps1 survives other agents pruning worktrees while it scans (`lead prune-safe`,
// `wt prune`): a worktree that vanished mid-scan is skipped (it was an ERROR "registered at missing path",
// plus "fatal: not a git repository" spam for half-removed folders), and a transient packed-refs read error
// ("couldn't read .git/packed-refs: Permission denied") is retried once instead of failing the check.
// A `git` shim first on PATH injects both: on the first `for-each-ref` it removes the `gone` slug from
// state.json (the prune) and fails with the packed-refs error (the transient read).
// inputs: tools/validation/validate-worktrees.ps1, tools/validation/validate-worktrees-selftest.mjs
// red-proof: 2026-11-15, with the retry removed from Invoke-Git "a transient packed-refs error is retried once" failed (1 call);
// with Test-StillRegistered always true "a worktree pruned mid-scan is not an error" failed; both restored.

import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { safeRemoveTree } from "../link-safety.mjs";

const failures = [];
const check = (ok, name) => {
  if (!ok) failures.push(name);
};

if (process.platform !== "win32") {
  console.log("validate-worktrees-selftest: OK (skipped: the check is PowerShell and runs on Windows)");
  process.exit(0);
}

const ps1 = resolve(dirname(new URL(import.meta.url).pathname.replace(/^\/([A-Za-z]:)/, "$1")), "validate-worktrees.ps1");
const realGit = spawnSync("where", ["git"], { encoding: "utf8", windowsHide: true }).stdout.split(/\r?\n/).find((l) => /git\.exe$/i.test(l.trim()))?.trim();

const base = mkdtempSync(join(tmpdir(), "lf-vwt-"));
try {
  const fx = join(base, "main");
  mkdirSync(fx);
  spawnSync("git", ["init", "-q", fx], { windowsHide: true });
  const hub = join(fx, "tmp", "wt");
  mkdirSync(hub, { recursive: true });
  const ghost = join(base, "ghost"); // a half-removed worktree: the folder is there, its .git is not
  mkdirSync(join(ghost, "node_modules"), { recursive: true });
  const state = {
    worktrees: {
      gone: { path: join(base, "gone"), branch: "ws/gone", ports: { gamePort: 1, a2sPort: 2 }, status: "active" },
      ghost: { path: ghost, branch: "ws/ghost", ports: { gamePort: 3, a2sPort: 4 }, status: "active" },
    },
  };
  const stateFile = join(hub, "state.json");
  const writeState = () => writeFileSync(stateFile, JSON.stringify(state));

  const shimDir = join(base, "shim");
  mkdirSync(shimDir);
  writeFileSync(join(shimDir, "git.cmd"), '@node "%~dp0git-shim.mjs" %*\r\n');
  writeFileSync(
    join(shimDir, "git-shim.mjs"),
    `import { spawnSync } from "node:child_process";
import { appendFileSync, existsSync, readFileSync, writeFileSync } from "node:fs";
const args = process.argv.slice(2);
const log = process.env.LF_SHIM_LOG;
if (log) appendFileSync(log, args.join(" ") + "\\n");
const flag = process.env.LF_SHIM_FLAG;
if (flag && args.includes("for-each-ref") && !existsSync(flag)) {
  writeFileSync(flag, "1");
  const s = JSON.parse(readFileSync(process.env.LF_SHIM_STATE, "utf8"));
  delete s.worktrees.gone; // the prune lands between the scan's first read and its loop
  writeFileSync(process.env.LF_SHIM_STATE, JSON.stringify(s));
  process.stderr.write("fatal: couldn't read " + process.cwd() + "/.git/packed-refs: Permission denied\\n");
  process.exit(128);
}
const r = spawnSync(process.env.LF_REAL_GIT, args, { stdio: "inherit", windowsHide: true });
process.exit(r.status ?? 1);
`,
  );

  const run = (withShim, tag) => {
    writeState();
    const log = join(base, `git-${tag}.log`);
    const flag = join(base, `flag-${tag}`);
    const env = { ...process.env, LF_SHIM_LOG: log, LF_SHIM_STATE: stateFile, LF_REAL_GIT: realGit };
    if (withShim) {
      env.PATH = `${shimDir};${process.env.PATH ?? process.env.Path}`;
      env.LF_SHIM_FLAG = flag;
    }
    const r = spawnSync("powershell.exe", ["-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-File", ps1], { cwd: fx, env, encoding: "utf8", windowsHide: true, timeout: 120000 });
    const calls = existsSync(log) ? readFileSync(log, "utf8").split(/\r?\n/).filter(Boolean) : [];
    return { code: r.status, out: `${r.stdout}${r.stderr}`, calls };
  };

  // control: no prune, no transient error: the missing path is still a finding, the ghost folder a warning
  const c = run(false, "control");
  check(c.code === 1 && /registered at missing path/.test(c.out), `control: a registered missing path still fails the check (exit ${c.code}: ${c.out.slice(0, 200)})`);
  check(/WARN\s+worktree 'ghost' .* is not a git worktree/.test(c.out), "a registered folder with no .git is a warning, not a crash");
  check(!/fatal: not a git repository/.test(c.out), "no 'fatal: not a git repository' reaches the output");

  const s = run(true, "shim");
  check(s.calls.filter((l) => l.includes("for-each-ref")).length >= 2, `a transient packed-refs error is retried once (for-each-ref calls: ${s.calls.filter((l) => l.includes("for-each-ref")).length})`);
  check(!/ERROR .*gone/.test(s.out), "a worktree pruned mid-scan is not an error");
  check(s.code === 0 && /validate-worktrees: OK/.test(s.out), `the check passes (exit ${s.code}: ${s.out.slice(0, 300)})`);
  check(!/packed-refs/.test(s.out), "the transient error is not printed");
} finally {
  safeRemoveTree(base);
}

if (failures.length) {
  console.log(`validate-worktrees-selftest: FAILED\n  ${failures.join("\n  ")}`);
  process.exit(1);
}
console.log("validate-worktrees-selftest: OK (vanished worktree skipped, packed-refs retried once, ghost folder warned)");

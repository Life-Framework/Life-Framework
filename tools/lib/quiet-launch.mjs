// tools/lib/quiet-launch.mjs - start a tool process where the user never sees it.
//
// Non-rendering runs (headless Workbench build, script validation, the test
// server) go to the private Win32 desktop "LF-Agents" through quiet-launch.ps1:
// no window on the user's screen, no focus steal. Console tools spawned from
// node get windowsHide so no console flashes either (see hidden()).
//
// LF_SHOW_WINDOWS=1 turns it off: processes start normally and visibly.
// So does an ssh session (SSH_CONNECTION, e.g. a run on the test box): it has
// no interactive window station, creating LF-Agents there kills the server
// before it logs a line, and nothing it starts reaches a screen anyway.

import { spawn, spawnSync } from "node:child_process";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const HELPER = join(dirname(fileURLToPath(import.meta.url)), "quiet-launch.ps1");
const PWSH = join(process.env.SystemRoot || "C:\\Windows", "System32", "WindowsPowerShell", "v1.0", "powershell.exe");

export function showWindows() {
  return process.env.LF_SHOW_WINDOWS === "1" || !!process.env.SSH_CONNECTION || process.platform !== "win32";
}

// Spawn options for a console tool: never pop a console window.
export function hidden(opts = {}) {
  return { windowsHide: true, ...opts };
}

function helperArgs(exe, args, opts, mode) {
  const spec = { exe, args, cwd: opts.cwd || "", stdout: opts.stdout || "", stderr: opts.stderr || "" };
  const b64 = Buffer.from(JSON.stringify(spec), "utf8").toString("base64");
  return ["-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-WindowStyle", "Hidden",
    "-Command", `. '${HELPER.replace(/'/g, "''")}'; Invoke-LfQuietSpec '${b64}' '${mode}'`];
}

// Like child_process.spawn(exe, args, {cwd, env}) but on the LF-Agents desktop.
// The returned child is the helper: its exit code is the tool's exit code, and
// killing it kills the tool (kill-on-close job). The tool's own stdio is not
// piped; pass opts.stdout / opts.stderr file paths to capture it.
export function quietSpawn(exe, args, opts = {}) {
  if (showWindows()) return spawn(exe, args, { cwd: opts.cwd, env: opts.env });
  return spawn(PWSH, helperArgs(exe, args, opts, "wait"), {
    cwd: opts.cwd,
    env: opts.env,
    stdio: ["ignore", "pipe", "pipe"],
    windowsHide: true,
  });
}

// Start a long-lived process on the LF-Agents desktop and return its pid.
export function quietSpawnDetached(exe, args, opts = {}) {
  if (showWindows()) {
    const child = spawn(exe, args, { cwd: opts.cwd, env: opts.env, detached: true, stdio: "ignore" });
    child.unref();
    return child.pid;
  }
  const res = spawnSync(PWSH, helperArgs(exe, args, opts, "detach"), {
    cwd: opts.cwd,
    env: opts.env,
    encoding: "utf8",
    windowsHide: true,
  });
  const pid = Number.parseInt((res.stdout || "").trim().split(/\r?\n/).pop(), 10);
  if (res.status !== 0 || !pid) throw new Error(`quiet-launch failed: ${(res.stderr || res.stdout || "").trim()}`);
  return pid;
}

// Processes with a window on LF-Agents: [{ pid, name, started }].
export function listHidden() {
  if (process.platform !== "win32") return [];
  const res = spawnSync(PWSH, ["-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-WindowStyle", "Hidden",
    "-Command", `. '${HELPER.replace(/'/g, "''")}'; @(Get-LfQuietProcesses) | ConvertTo-Json -Compress`], { encoding: "utf8", windowsHide: true });
  const text = (res.stdout || "").trim();
  if (!text) return [];
  const parsed = JSON.parse(text);
  return Array.isArray(parsed) ? parsed : [parsed];
}

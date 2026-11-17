// Run a tools\cli command again in this machine's signed-in desktop session, from an ssh session.
// ssh is session 0, which has no GPU device: a Workbench started there fails "Couldn't build device
// list for any adapter" and hangs. Same route as `ci --wb-only` on a pool machine (cli.mjs
// ciWorkbenchInSession): a one-shot scheduled task (run-interactive.ps1); the transcript and exit
// code come back through files. LF_WB_INTERACTIVE=1 marks the inner run.
import { existsSync, mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { spawnSync } from "node:child_process";
import { gpuRoute, requireDesktopSession } from "../../lib/gpu-session.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const PWSH = join(process.env.SystemRoot || "C:\\Windows", "System32", "WindowsPowerShell", "v1.0", "powershell.exe");

// True when a Workbench started from this process would land in session 0.
export const needsDesktopSession = (env = process.env) => gpuRoute({ env }) === "desktop-session";

// Fail fast: prints the one line and returns false when there is no usable desktop session on this machine.
export function desktopSessionOrSay(say = console.log, require = requireDesktopSession) {
  try {
    require();
    return true;
  } catch (e) {
    say(`FAIL  ${e.message}`);
    return false;
  }
}

// Runs `node tools\cli.mjs <args>` in the desktop session; its exit code, or 3 when nobody is signed in.
export function cliInDesktopSession(root, args, { name = "run", waitSec = 1800, require = requireDesktopSession } = {}) {
  if (!desktopSessionOrSay(console.log, require)) return 3;
  const dir = join(root, "tmp", "wb-session");
  mkdirSync(dir, { recursive: true });
  const [log, codeFile, ps1, pidFile] = ["log", "code", "ps1", "pid"].map((e) => join(dir, `${name}.${e}`));
  for (const f of [log, codeFile, pidFile]) rmSync(f, { force: true });
  const q = (s) => `'${String(s).replace(/'/g, "''")}'`;
  writeFileSync(ps1, [
    `Set-Content -LiteralPath ${q(pidFile)} -Value $PID`,
    "$env:LF_WB_INTERACTIVE = '1'",
    `Set-Location -LiteralPath ${q(root)}`,
    `& ${q(process.execPath)} tools\\cli.mjs ${args.map(q).join(" ")} *> ${q(log)}`,
    `Set-Content -LiteralPath ${q(codeFile)} -Value $LASTEXITCODE`,
  ].join("\n"));
  const t0 = Date.now();
  const r = spawnSync(PWSH, ["-NoProfile", "-ExecutionPolicy", "Bypass", "-File", join(HERE, "run-interactive.ps1"),
    "-Exe", "powershell.exe", "-WorkDir", root, "-WaitSec", String(waitSec), "-Arguments", `-NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -File "${ps1}"`],
  { cwd: root, encoding: "utf8", windowsHide: true, timeout: (waitSec + 120) * 1000 });
  process.stdout.write((r.stdout || "") + (r.stderr || ""));
  // The task's wait can end before its run does (lf-cloud, 2026-11-14): then wait on the PowerShell.
  if (!existsSync(codeFile) && existsSync(pidFile)) {
    const pid = Number(readFileSync(pidFile, "utf8").trim());
    const left = Math.max(0, waitSec - Math.round((Date.now() - t0) / 1000));
    if (Number.isInteger(pid) && pid > 0 && left > 0) spawnSync(PWSH, ["-NoProfile", "-Command", `Wait-Process -Id ${pid} -Timeout ${left} -ErrorAction SilentlyContinue`], { windowsHide: true, timeout: (left + 30) * 1000 });
  }
  if (existsSync(log)) {
    const buf = readFileSync(log); // Windows PowerShell's *> writes UTF-16 LE with a BOM
    process.stdout.write(buf[0] === 0xff && buf[1] === 0xfe ? buf.subarray(2).toString("utf16le") : buf.toString("utf8"));
  }
  const code = existsSync(codeFile) ? Number(readFileSync(codeFile, "utf8").trim()) : NaN;
  if (!Number.isInteger(code)) {
    console.log("FAIL  the desktop-session run left no exit code (is a user signed in on this machine?)");
    return 3;
  }
  return code;
}

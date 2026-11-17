// tools\cli desktop-helper setup|status|remove <machine> | --local
// The logon helper that keeps a pool machine's desktop session ready for GPU tools (Workbench, game clients):
// a scheduled task "LF-DesktopHelper", At log on for the desktop user, running hidden in that user's session.
// It asks the display to stay awake and writes a heartbeat (C:\LF\desktop\gpu-session.json) that `pool status`
// reads. ssh runs in session 0, which has no display output; every GPU launch goes through the signed-in desktop
// session instead (tools/lib/gpu-session.mjs, docs/setup.md "Desktop helper").
export const help = "desktop-helper setup|status|remove <machine>|--local   the At-log-on helper that keeps a pool machine's desktop session ready for GPU tools; idempotent (docs/setup.md 'Desktop helper')";

import { mkdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { spawnSync } from "node:child_process";
import { HELPER_TASK, HEARTBEAT_FILE, PS_GPU_PROBE, PS_READ_HEARTBEAT, desktopHelperScript, desktopLine, heartbeatState, parseProbe, setupScript } from "../lib/gpu-session.mjs";

const REMOTE_DIR = "C:/LF/desktop";
const HELPER_PATH = `${REMOTE_DIR}/desktop-helper.ps1`;
const PWSH = join(process.env.SystemRoot || "C:\\Windows", "System32", "WindowsPowerShell", "v1.0", "powershell.exe");

// Pure: { verb, machine, local } from the argv after `desktop-helper`, or { error }.
export function parseArgs(args) {
  const verb = args[0];
  if (!["setup", "status", "remove"].includes(verb)) return { error: "usage: desktop-helper setup|status|remove <machine>|--local" };
  const local = args.includes("--local");
  const machine = args.slice(1).find((a) => !a.startsWith("-")) ?? null;
  if (!local && !machine) return { error: `desktop-helper ${verb}: name a pool machine (tools\\cli pool status) or pass --local to run on this machine` };
  return { verb, machine, local };
}

// A runner for one machine: run(psScript) -> { status, stdout, stderr }; put(localFile, remotePath) -> boolean.
function runnerFor(root, o, { machines, tb }) {
  if (o.local) {
    return {
      name: "this machine",
      run: (ps) => spawnSync(PWSH, ["-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-EncodedCommand", Buffer.from(ps, "utf16le").toString("base64")], { encoding: "utf8", windowsHide: true, timeout: 120000 }),
      put: (file, remote) => {
        const r = spawnSync(PWSH, ["-NoProfile", "-NonInteractive", "-Command", `New-Item -ItemType Directory -Force (Split-Path '${remote}') | Out-Null; Copy-Item -LiteralPath '${file}' -Destination '${remote}' -Force`], { encoding: "utf8", windowsHide: true });
        return r.status === 0;
      },
    };
  }
  const m = machines.find((x) => x.name.toLowerCase() === o.machine.toLowerCase() || x.host?.toLowerCase() === o.machine.toLowerCase());
  if (!m || m.via !== "ssh") return { error: `desktop-helper: "${o.machine}" is not an ssh pool machine (tools\\cli pool status)` };
  return {
    name: m.name,
    run: (ps) => tb.sshPs(m.host, ps, { timeout: 120000, connectTimeout: 8 }),
    put: (file, remote) => {
      tb.sshPs(m.host, `New-Item -ItemType Directory -Force ${tb.psQuote(remote.replace(/\/[^/]+$/, ""))} | Out-Null`, { timeout: 30000 });
      const r = tb.scp([file, `${m.host}:${remote}`]);
      if (r.status !== 0) process.stderr.write(r.stderr ?? "");
      return r.status === 0;
    },
  };
}

function readState(run, name) {
  const r = run(`${PS_GPU_PROBE}\n${PS_READ_HEARTBEAT}\n$t = Get-ScheduledTask -TaskName '${HELPER_TASK}' -ErrorAction SilentlyContinue\nif ($t) { [Console]::Out.WriteLine('lf-task: ' + $t.State) } else { [Console]::Out.WriteLine('lf-task: missing') }`);
  const out = r.stdout ?? "";
  return { probe: parseProbe(out), hb: (/^lf-gpu-hb: (.*)$/m.exec(out) ?? [])[1] ?? null, task: (/^lf-task: (\S+)/m.exec(out) ?? [])[1] ?? "unknown", ok: r.status === 0 };
}

export async function main(args, { root }) {
  const o = parseArgs(args);
  if (o.error) {
    console.log(o.error);
    return 2;
  }
  const pool = await import("../remote/pool.mjs");
  const tb = await import("../remote/testbox.mjs");
  const rn = runnerFor(root, o, { machines: o.local ? [] : pool.loadMachines(root), tb });
  if (rn.error) {
    console.log(rn.error);
    return 2;
  }
  if (o.verb === "status") {
    const st = readState(rn.run, rn.name);
    if (!st.ok) {
      console.log(`FAIL  ${rn.name} did not answer`);
      return 3;
    }
    console.log(`${rn.name}: ${desktopLine(rn.name, st.probe, st.hb)}; task ${HELPER_TASK}: ${st.task}`);
    return st.probe && st.probe.user ? 0 : 1;
  }
  if (o.verb === "remove") {
    const r = rn.run(`Stop-ScheduledTask -TaskName '${HELPER_TASK}' -ErrorAction SilentlyContinue; Unregister-ScheduledTask -TaskName '${HELPER_TASK}' -Confirm:$false -ErrorAction SilentlyContinue; [Console]::Out.WriteLine('removed ${HELPER_TASK}')`);
    process.stdout.write(r.stdout ?? "");
    return r.status === 0 ? 0 : 1;
  }
  // setup: the helper script, the task, a start now, then wait for the first heartbeat.
  const dir = join(root, "tmp", "desktop-helper");
  mkdirSync(dir, { recursive: true });
  const file = join(dir, "desktop-helper.ps1");
  writeFileSync(file, desktopHelperScript(), "ascii");
  if (!rn.put(file, HELPER_PATH)) {
    console.log(`FAIL  could not copy the helper to ${rn.name}:${HELPER_PATH}`);
    return 3;
  }
  const reg = rn.run(`${setupScript({ helperPath: HELPER_PATH.replace(/\//g, "\\") })}\n$s = Get-ScheduledTask -TaskName '${HELPER_TASK}'\nif ($s.State -ne 'Running') { Start-ScheduledTask -TaskName '${HELPER_TASK}' }`);
  process.stdout.write(reg.stdout ?? "");
  if (reg.status !== 0) {
    console.log(`FAIL  registering ${HELPER_TASK} on ${rn.name}: ${String(reg.stderr ?? "").trim().split("\n")[0] || `exit ${reg.status}`}`);
    return 1;
  }
  // The helper writes its first heartbeat within a few seconds when a desktop session is up.
  let st;
  const t0 = Date.now();
  do {
    st = readState(rn.run, rn.name);
    if (st.hb && heartbeatState(st.hb).helper) break;
    await new Promise((r) => setTimeout(r, 3000));
  } while (Date.now() - t0 < 30000);
  console.log(`${rn.name}: ${desktopLine(rn.name, st.probe, st.hb)}; task ${HELPER_TASK}: ${st.task}`);
  console.log(`desktop-helper setup: ${HELPER_TASK} runs At log on for the desktop user; heartbeat ${HEARTBEAT_FILE}`);
  return st.hb && heartbeatState(st.hb).helper ? 0 : 1;
}

// tools/validation/gpu-session-selftest.mjs
// inputs: tools/lib/gpu-session.mjs, tools/lib/interactive-launch.mjs, tools/lib/wb-gate.mjs, tools/lib/wb-route.mjs, tools/remote/desktop/in-session.mjs, tools/remote/pool.mjs, tools/commands/desktop-helper.mjs, tools/validation/gpu-session-selftest.mjs
//
// Offline proof of the GPU-session routing (lf-testbox, 2026-11-17: after a reboot every Workbench and client started
// over ssh died with "RENDER (E): Couldn't build device list for any adapter"; ssh is session 0 and has no display output):
//   * the routing choice: over ssh a GPU launch goes through the desktop session, anywhere else it starts directly;
//     which commands are re-run in the desktop session (needsDesktopRerun);
//   * the fail-fast: no signed-in desktop session, or a disconnected one, is ONE clear line ("lf-testbox has no signed-in
//     desktop session: log in once") thrown before anything is registered or started, from the interactive launch, the
//     whole-CLI re-run and the pool router, which never waits for such a machine;
//   * the pool: chooseGate skips a machine with no GPU session (pin and "all of them" are "none" at once), parseWbState
//     and parseRemoteState read the probe, `pool status` has a desktop line;
//   * the logon helper: the scheduled task script is At log on, idempotent (-Force), the helper script holds one instance
//     and writes a heartbeat; every generated PowerShell parses (Windows only).
// red-proof: 2026-11-17, three breaks, each restored: gpuRoute always "direct" failed 7 cases ("over ssh a GPU launch goes
// through the desktop session" ...); chooseGate ignoring state.gpu failed 5 ("a machine with no GPU session is never
// chosen", "pinned to a machine with no GPU session: none, no wait" ...); verdict not looking at the session state failed
// "a disconnected session is not usable".

import { spawnSync } from "node:child_process";
import { existsSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import * as gs from "../lib/gpu-session.mjs";
import { interactiveNeeded, interactiveSpawnDetached } from "../lib/interactive-launch.mjs";
import { chooseGate, parseWbState } from "../lib/wb-gate.mjs";
import { pickWorkbenchMachine } from "../lib/wb-route.mjs";
import { cliInDesktopSession, desktopSessionOrSay, needsDesktopSession } from "../remote/desktop/in-session.mjs";
import { parseRemoteState } from "../remote/pool.mjs";
import { parseArgs } from "../commands/desktop-helper.mjs";

let failed = 0;
const check = (name, ok, detail = "") => {
  if (!ok) {
    failed++;
    console.log(`FAIL  ${name}${detail ? `: ${detail}` : ""}`);
  }
};

const ssh = { platform: "win32", env: { SSH_CONNECTION: "10.0.0.1 1 10.0.0.2 22" } };
const LINE = "lf-testbox has no signed-in desktop session: log in once";
const nobody = { user: "", session: -1, state: -1, locked: false, monitors: 0, error: "" };
const active = { user: "PC1\\jaspe", session: 1, state: 0, locked: false, monitors: 2, error: "" };
const disconnected = { ...active, state: 4 };

// ---- the routing choice
check("over ssh a GPU launch goes through the desktop session", gs.gpuRoute(ssh) === "desktop-session");
check("not over ssh it starts directly", gs.gpuRoute({ platform: "win32", env: {} }) === "direct");
check("inside the desktop session (LF_WB_INTERACTIVE=1) it starts directly", gs.gpuRoute({ platform: "win32", env: { ...ssh.env, LF_WB_INTERACTIVE: "1" } }) === "direct");
check("LF_SHOW_WINDOWS=1 starts directly", gs.gpuRoute({ platform: "win32", env: { ...ssh.env, LF_SHOW_WINDOWS: "1" } }) === "direct");
check("not Windows: direct", gs.gpuRoute({ platform: "linux", env: ssh.env }) === "direct");
{
  const keep = { ssh: process.env.SSH_CONNECTION, wb: process.env.LF_WB_INTERACTIVE, show: process.env.LF_SHOW_WINDOWS };
  delete process.env.LF_WB_INTERACTIVE;
  delete process.env.LF_SHOW_WINDOWS;
  process.env.SSH_CONNECTION = "x";
  const overSsh = interactiveNeeded();
  const inSession = needsDesktopSession();
  process.env.LF_WB_INTERACTIVE = "1";
  const inner = interactiveNeeded();
  for (const [k, v] of [["SSH_CONNECTION", keep.ssh], ["LF_WB_INTERACTIVE", keep.wb], ["LF_SHOW_WINDOWS", keep.show]]) {
    if (v === undefined) delete process.env[k];
    else process.env[k] = v;
  }
  if (process.platform === "win32") {
    check("interactiveNeeded follows gpuRoute over ssh", overSsh === true);
    check("needsDesktopSession follows gpuRoute over ssh", inSession === true);
    check("interactiveNeeded is false inside the desktop session", inner === false);
  }
}
check("ui-image is re-run in the desktop session", gs.needsDesktopRerun(["ui-image", "a.svg", "x"]));
check("asset run is re-run", gs.needsDesktopRerun(["asset", "run", "slug"]));
check("asset status is not", !gs.needsDesktopRerun(["asset", "status"]));
check("pack (the Workshop bundle) is re-run, pack new/check/explain are not", gs.needsDesktopRerun(["pack"]) && !gs.needsDesktopRerun(["pack", "check"]) && !gs.needsDesktopRerun(["pack", "new", "x"]));
check("call wb_* is re-run, call api_search is not", gs.needsDesktopRerun(["call", "wb_read_props", "{}"]) && !gs.needsDesktopRerun(["call", "api_search", "{}"]));
check("look, ci, build and test route themselves", !["look", "ci", "build", "test", "rdb", "session"].some((c) => gs.needsDesktopRerun([c])));

// ---- the verdict and the one line
check("nobody signed in: not ok, with the one clear line", gs.verdict(nobody, "lf-testbox").ok === false && gs.verdict(nobody, "lf-testbox").line === LINE, gs.verdict(nobody, "lf-testbox").line);
check("a disconnected session is not usable", gs.verdict(disconnected, "lf-testbox").ok === false && /Disconnected/.test(gs.verdict(disconnected, "lf-testbox").line));
check("an active session is ok", gs.verdict(active, "lf-testbox").ok === true);
check("a locked session still renders: ok with a note", gs.verdict({ ...active, locked: true }, "m").ok === true && /locked/.test(gs.verdict({ ...active, locked: true }, "m").note));
check("a probe that did not answer is unknown, never a refusal", gs.verdict(null, "m").ok === null && gs.verdict({ ...nobody, error: "boom" }, "m").ok === null);
check("explorer in session 0 only is nobody", gs.verdict({ ...active, session: 0 }, "m").ok === false);

// ---- the fail-fast
{
  let probes = 0;
  const probe = (p) => () => {
    probes++;
    return p;
  };
  const direct = gs.requireDesktopSession({ ...{ platform: "win32", env: {} }, probe: probe(nobody), label: "lf-testbox" });
  check("a direct route needs no check and never probes", direct.skipped === true && probes === 0);
  let err = null;
  try {
    gs.requireDesktopSession({ ...ssh, probe: probe(nobody), label: "lf-testbox" });
  } catch (e) {
    err = e;
  }
  check("over ssh with nobody signed in: NoDesktopSession with the one line", err?.code === gs.NO_SESSION_CODE && err.message === LINE, err?.message);
  check("... and it is a single line", err && !/\n/.test(err.message));
  err = null;
  try {
    gs.requireDesktopSession({ ...ssh, probe: probe(disconnected), label: "lf-testbox" });
  } catch (e) {
    err = e;
  }
  check("over ssh with a disconnected session: refused", err?.code === gs.NO_SESSION_CODE, String(err?.message));
  check("over ssh with an active session: ok", gs.requireDesktopSession({ ...ssh, probe: probe(active), label: "lf-testbox" }).ok === true);
  check("an unreadable probe never blocks a launch", gs.requireDesktopSession({ ...ssh, probe: probe(null), label: "lf-testbox" }).ok === true);
}
{
  // The interactive launch asks first: with no session it throws before registering any task.
  let threw = null;
  try {
    interactiveSpawnDetached("C:\\nope\\x.exe", [], { cwd: "C:\\nope", marker: "m", require: () => gs.requireDesktopSession({ ...ssh, probe: () => nobody, label: "lf-testbox" }) });
  } catch (e) {
    threw = e;
  }
  check("interactiveSpawnDetached throws NoDesktopSession before starting anything", threw?.code === gs.NO_SESSION_CODE && threw.message === LINE, String(threw?.message));
  // The whole-CLI re-run returns 3 at once, one line, and writes nothing.
  const root = mkdtempSync(join(tmpdir(), "lf-gpu-"));
  const said = [];
  const keep = console.log;
  console.log = (...a) => said.push(a.join(" "));
  let code;
  try {
    code = cliInDesktopSession(root, ["ui-image", "a.svg", "x"], { require: () => { throw new gs.NoDesktopSession(LINE); } });
  } finally {
    console.log = keep;
  }
  check("cliInDesktopSession with no session: exit 3 and the one line", code === 3 && said.length === 1 && said[0] === `FAIL  ${LINE}`, JSON.stringify({ code, said }));
  check("... and it touched no run folder", !existsSync(join(root, "tmp")));
  rmSync(root, { recursive: true, force: true });
  const out = [];
  check("desktopSessionOrSay: false and says the line", desktopSessionOrSay((l) => out.push(l), () => { throw new gs.NoDesktopSession(LINE); }) === false && out[0] === `FAIL  ${LINE}`);
  check("desktopSessionOrSay: true when there is a session", desktopSessionOrSay(() => {}, () => ({ ok: true })) === true);
}

// ---- the pool: choosing a machine
const rem = (name, state) => ({ name, caps: ["workbench"], multiplier: 1, state });
const okState = { freeMb: 9000, wbAlive: 0, wbLimit: 1 };
const noGpu = { ...okState, gpu: { ok: false, line: LINE, note: "nobody signed in" } };
const noLocal = { hasWb: false, busy: 0, queued: 0, limit: 1, freeMb: 9000, multiplier: 1 };
const withLocal = { ...noLocal, hasWb: true };
{
  const both = [rem("lf-testbox", noGpu), rem("lf-cloud", okState)];
  check("a machine with no GPU session is never chosen", chooseGate({ local: noLocal, remotes: both, mode: "remote" }).where === "lf-cloud");
  const only = chooseGate({ local: noLocal, remotes: [rem("lf-testbox", noGpu)], mode: "remote" });
  check("the only Workbench machine has no GPU session, no cap here: none at once, not wait", only.where === "none" && only.why === LINE, JSON.stringify(only));
  const pin = chooseGate({ local: noLocal, remotes: both, mode: "remote", machine: "lf-testbox" });
  check("pinned to a machine with no GPU session: none, no wait", pin.where === "none" && pin.why === LINE, JSON.stringify(pin));
  check("a pin on a machine with a session still works", chooseGate({ local: noLocal, remotes: both, mode: "remote", machine: "lf-cloud" }).where === "lf-cloud");
  check("with a cap here the run falls back to this PC", chooseGate({ local: withLocal, remotes: [rem("lf-testbox", noGpu)], mode: "remote" }).where === "local");
  check("an unknown GPU state (older holder) is not held against the machine", chooseGate({ local: noLocal, remotes: [rem("lf-testbox", okState)], mode: "remote" }).where === "lf-testbox");
  // The router never waits for such a machine: sleep would throw.
  const choose = () => ({ remotes: [rem("lf-testbox", noGpu)], c: chooseGate({ local: noLocal, remotes: [rem("lf-testbox", noGpu)], mode: "remote" }) });
  const lines = [];
  const p = await pickWorkbenchMachine("unused", { purpose: "look", budgetS: 3600, choose, log: (l) => lines.push(l), sleep: async () => { throw new Error("waited"); } });
  check("pickWorkbenchMachine fails fast with the one line and never sleeps", !!p.error && p.error.includes(LINE), JSON.stringify(p));
}
{
  const text = `lf-gpu: ${JSON.stringify(nobody)}\nlf-wb: 0 7000\n`;
  const st = parseWbState(text, 1, "lf-testbox");
  check("parseWbState reads the probe into gpu", st?.gpu?.ok === false && st.gpu.line === LINE, JSON.stringify(st));
  check("parseWbState without a probe has no gpu key", !("gpu" in parseWbState("lf-wb: 0 7000", 1, "m")));
  const rs = parseRemoteState(`lf-pool: ram 8000 16000\nlf-gpu: ${JSON.stringify(active)}\nlf-gpu-hb: ${JSON.stringify({ at: 1, user: "x" })}\n`);
  check("parseRemoteState keeps the probe and the heartbeat", rs.gpuProbe?.session === 1 && /"at":1/.test(rs.heartbeat ?? ""), JSON.stringify(rs));
}
{
  const now = 1_000_000_000_000;
  check("a fresh heartbeat is a live helper", gs.heartbeatState(JSON.stringify({ at: now - 4000 }), { now }).helper === true);
  check("a stale heartbeat is not", gs.heartbeatState(JSON.stringify({ at: now - 120000 }), { now }).helper === false);
  check("no heartbeat file is not", gs.heartbeatState("", { now }).helper === false);
  check("pool status line: no session", /^NO GPU SESSION: .*log in once$/.test(gs.desktopLine("lf-testbox", nobody, null)));
  check("pool status line: ok with the helper", /^GPU session ok: .*session 1, Active.*logon helper alive/.test(gs.desktopLine("lf-testbox", active, JSON.stringify({ at: now - 2000 }), { now })), gs.desktopLine("lf-testbox", active, JSON.stringify({ at: now - 2000 }), { now }));
  check("pool status line: ok without the helper says how to set it up", /desktop-helper setup/.test(gs.desktopLine("lf-testbox", active, null)));
  check("machineLabel maps the box's hostname to its pool name", gs.machineLabel("PC1") === "lf-testbox" && gs.machineLabel("nobody-here") === "nobody-here");
}

// ---- the logon helper
{
  const setup = gs.setupScript({ user: "PC1\\jaspe" });
  check("the task is At log on for the user", /New-ScheduledTaskTrigger -AtLogOn -User \$user/.test(setup) && /'PC1\\jaspe'/.test(setup));
  check("the task is interactive (the desktop session)", /-LogonType Interactive/.test(setup));
  check("setup is idempotent: the task is replaced with -Force", /Register-ScheduledTask -TaskName 'LF-DesktopHelper'.* -Force/.test(setup));
  check("one instance at a time", /MultipleInstances IgnoreNew/.test(setup));
  const helper = gs.desktopHelperScript();
  check("the helper holds one instance (a mutex)", /System\.Threading\.Mutex/.test(helper));
  check("the helper keeps the display awake", /SetThreadExecutionState\(0x80000003\)/.test(helper));
  check("the helper writes the heartbeat file", helper.includes("C:\\LF\\desktop\\gpu-session.json"));
  check("the helper is plain ASCII (PowerShell 5.1 reads a BOM-less file as ANSI)", /^[\x09\x0a\x0d\x20-\x7e]*$/.test(helper));
  check("desktop-helper takes a verb and a machine", parseArgs(["setup", "lf-testbox"]).machine === "lf-testbox" && parseArgs(["status", "--local"]).local === true);
  check("desktop-helper without a machine says what to pass", /name a pool machine/.test(parseArgs(["setup"]).error));
  check("desktop-helper rejects another verb", !!parseArgs(["frobnicate"]).error);
  // Every generated PowerShell parses (the real parser; Windows only).
  if (process.platform === "win32") {
    const dir = mkdtempSync(join(tmpdir(), "lf-gpu-ps-"));
    const scripts = { probe: gs.PS_GPU_PROBE, heartbeat: gs.PS_READ_HEARTBEAT, setup, helper };
    for (const [name, text] of Object.entries(scripts)) {
      const file = join(dir, `${name}.ps1`);
      writeFileSync(file, text);
      const r = spawnSync("powershell.exe", ["-NoProfile", "-NonInteractive", "-Command", `$e = $null; [void][System.Management.Automation.Language.Parser]::ParseFile('${file}', [ref]$null, [ref]$e); if ($e) { $e | ForEach-Object { $_.Message }; exit 1 }`], { encoding: "utf8", windowsHide: true, timeout: 60000 });
      check(`generated PowerShell parses: ${name}`, r.status === 0, (r.stdout || r.stderr || "").trim().split("\n")[0]);
    }
    rmSync(dir, { recursive: true, force: true });
  }
}

if (failed) {
  console.log(`gpu-session-selftest: ${failed} failed`);
  process.exit(1);
}
console.log("gpu-session-selftest: ok");

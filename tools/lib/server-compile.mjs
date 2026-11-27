// tools/lib/server-compile.mjs - compile the game scripts on the dedicated server under the session defines.
//
// Workbench's ScriptEditor -validate (validate-scripts.ps1) and the suite (LF_AUTOTEST) are not the build a
// session boots: `session up --mission playtest|everon|island` starts the server with -scrDefine
// LF_SESSION_IDENTITY and -scrDefine LF_HARNESS. On 2026-11-27 (#5180) a long LF_Test_SoldSignFace.Run crashed
// that compiler alone ("Compiling Game scripts", then `ENGINE (F): Crashed`, an access violation with no SCRIPT (E)
// line), while Workbench validate, even with -scrDefine LF_HARNESS, and the suite compiled it. Every harness session
// then failed to boot and the main guard stayed green.
//
// This boots the dedicated server, hidden, with the session's mission, addons and defines, waits for the Game
// module to finish compiling (`Module: Game; ...`) or to crash, and kills it. About 3 to 10 s. The main guard's
// compile step (`ci --wb-only --compile-only`) runs it after the Workbench compile.

import { existsSync, mkdirSync, readdirSync, readFileSync, statSync } from "node:fs";
import { dirname, join } from "node:path";

export const SESSION_DEFINES = ["LF_SESSION_IDENTITY", "LF_HARNESS"];

// The server command line. `mission` is a {GUID}path header, `addonsDirs` the -addonsDir entries, `addons` the -addons list.
export function serverCompileArgs({ mission, addonsDirs, addons, profile, defines = SESSION_DEFINES, port = 2999, a2sPort = 17999 }) {
  return [
    "-server", mission,
    "-addonsDir", addonsDirs.filter(Boolean).join(","),
    "-addons", addons.join(","),
    "-profile", profile,
    "-logLevel", "normal",
    ...defines.flatMap((d) => ["-scrDefine", d]),
    "-disableCrashReporter", "-noBackend",
    "-addr", `127.0.0.1:${port}`, "-a2sPort", String(a2sPort),
  ];
}

// The verdict of a console.log so far: null while the Game module is still compiling.
//   { verdict: "ok" }                    the Game module compiled
//   { verdict: "error", lines }          SCRIPT (E) lines and `Can't compile`
//   { verdict: "crash", lines }          the engine crashed (during the compile, or before the Game module line)
export function judgeServerCompile(text) {
  const s = String(text ?? "");
  const lines = s.split(/\r?\n/);
  const errors = lines.filter((l) => /SCRIPT\s+\(E\)/.test(l)).map((l) => l.trim());
  if (/Module: Game;/.test(s)) return errors.length ? { verdict: "error", lines: errors } : { verdict: "ok" };
  if (/ENGINE\s+\(F\):\s*Crashed/.test(s) || /Application crashed!/.test(s)) {
    const at = lines.filter((l) => /Compiling Game scripts|Crashed/.test(l)).map((l) => l.trim());
    return { verdict: "crash", lines: [...errors, ...at] };
  }
  if (/Can't compile/.test(s)) return { verdict: "error", lines: errors };
  return null;
}

// One line for each verdict. A crash and an error say "does not compile" (main-guard.mjs COMPILE_ERROR reads it).
export function verdictLine(v, defines = SESSION_DEFINES) {
  v = v ?? { verdict: "env" };
  const tag = defines.map((d) => `-scrDefine ${d}`).join(" ");
  if (v.verdict === "ok") return `PASS  server-compile: Game scripts compile on the dedicated server (${tag})`;
  if (v.verdict === "crash") return `FAIL  server-compile: Enforce Script does not compile on the dedicated server (${tag}): the script compiler crashed with no SCRIPT (E) line. Bisect the newest script change with a session boot; a long method is a known cause (#5180)`;
  if (v.verdict === "error") return `FAIL  server-compile: Enforce Script does not compile on the dedicated server (${tag})`;
  return `ENV   server-compile: ${v.why ?? "no verdict"}`;
}

function newestConsole(profile, since) {
  const dir = join(profile, "logs");
  if (!existsSync(dir)) return null;
  const runs = readdirSync(dir).filter((d) => d.startsWith("logs_")).sort();
  for (let i = runs.length - 1; i >= 0; i--) {
    const f = join(dir, runs[i], "console.log");
    if (existsSync(f) && statSync(f).mtimeMs >= since - 2000) return f;
  }
  return null;
}

// Boot, judge, kill. spawnFn(exe, args, {cwd}) returns a child with kill(). Resolves
// { verdict: "ok"|"error"|"crash"|"env", lines, log, why }.
export function runServerCompile({ exe, args, profile, spawnFn, timeoutMs = 180000, pollMs = 300 }) {
  if (!existsSync(exe)) return Promise.resolve({ verdict: "env", why: `no dedicated server at ${exe}`, lines: [] });
  mkdirSync(profile, { recursive: true });
  const t0 = Date.now();
  const child = spawnFn(exe, args, { cwd: dirname(exe) });
  let exited = false;
  child.on?.("exit", () => { exited = true; });
  return new Promise((resolveRun) => {
    let exitSeenAt = 0;
    const timer = setInterval(() => {
      const log = newestConsole(profile, t0);
      const v = log ? judgeServerCompile(readFileSync(log, "utf8")) : null;
      let out = v ? { ...v, log } : null;
      if (!out && exited) {
        exitSeenAt ||= Date.now();
        // The log can trail the exit by a moment; past that, a server that quit with no verdict is the machine.
        if (Date.now() - exitSeenAt > 3000) out = { verdict: "env", why: `the server exited before the Game module compiled (log ${log ?? "none"})`, lines: [], log };
      }
      if (!out && Date.now() - t0 > timeoutMs) out = { verdict: "env", why: `no verdict after ${Math.round(timeoutMs / 1000)} s (log ${log ?? "none"})`, lines: [], log };
      if (!out) return;
      clearInterval(timer);
      try { child.kill(); } catch {}
      resolveRun({ lines: [], ...out });
    }, pollMs);
  });
}

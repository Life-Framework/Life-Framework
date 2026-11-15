// tools/lib/wb-launch-health.mjs - why a headless Workbench that started is not getting anywhere.
//
// A refresh Workbench (-wbModule=ResourceManager -exitAfterInit) writes the rdb ~3 s after launch and
// is then supposed to exit. When it does not, the engine's watchdog drops FREEZE_*.mdmp into
// <profile>/logs/<run>/ after ~64 s ("Application hangs (generated dump and waiting)"), and the
// console.log of that run stays empty. On lf-cloud, 2026-11-15, Steam came back from a reboot signed out
// (HKCU\Software\Valve\Steam\ActiveProcess\ActiveUser = 0, "cached creds not available" in Steam's
// connection_log.txt): every refresh froze, and the ssh path sat out its 600 s for each addon while holding
// the build lock and a Workbench slot, which stalled every gate and session on the box.
// stallReason names that state within ~65 s; validation/wb-launch-health-selftest.mjs proves it.

import { spawnSync } from "node:child_process";
import { existsSync, readdirSync, statSync } from "node:fs";
import { join } from "node:path";

// The newest FREEZE_*.mdmp under <profile>/logs/*/ written at or after sinceMs, or null.
export function findFreezeDump(profile, sinceMs = 0) {
  const logs = join(profile, "logs");
  if (!existsSync(logs)) return null;
  let best = null;
  for (const run of readdirSync(logs, { withFileTypes: true })) {
    if (!run.isDirectory()) continue;
    const dir = join(logs, run.name);
    let files = [];
    try {
      files = readdirSync(dir);
    } catch {}
    for (const f of files) {
      if (!/^FREEZE_.*\.mdmp$/i.test(f)) continue;
      try {
        const m = statSync(join(dir, f)).mtimeMs;
        if (m >= sinceMs && (!best || m > best.m)) best = { path: join(dir, f), m };
      } catch {}
    }
  }
  return best ? best.path : null;
}

// Pure: `reg query ... /v ActiveUser` text -> the account id (0 = nobody signed in), or null when unreadable.
export function parseActiveUser(text) {
  const m = /ActiveUser\s+REG_DWORD\s+0x([0-9a-f]+)/i.exec(String(text));
  return m ? parseInt(m[1], 16) : null;
}

// true / false, or null when it cannot tell (not Windows, no Steam key): never a reason to block a run.
export function steamSignedIn(run = spawnSync) {
  if (process.platform !== "win32") return null;
  try {
    const r = run("reg", ["query", "HKCU\\Software\\Valve\\Steam\\ActiveProcess", "/v", "ActiveUser"], { encoding: "utf8", windowsHide: true, timeout: 10000 });
    const id = parseActiveUser(r.stdout);
    return id === null ? null : id !== 0;
  } catch {
    return null;
  }
}

// Pure: the reason a started Workbench is stuck without its job done, or "" while it may still be working.
// freezeDump: findFreezeDump's result; steam: steamSignedIn's.
export function stallReason({ freezeDump, steam, host = "" }) {
  if (!freezeDump) return "";
  const on = host ? ` on ${host}` : "";
  const why = steam === false ? `Steam is not signed in${on}: sign in to Steam there (the Steam window is minimised to the tray)` : "cause unknown; read the dump's log folder";
  return `Workbench froze before registering the rdb (engine freeze dump written); ${why}`;
}

export function selftest() {
  const fails = [];
  if (parseActiveUser("    ActiveUser    REG_DWORD    0x0\r\n") !== 0) fails.push("ActiveUser 0x0 not read as 0");
  if (parseActiveUser("    ActiveUser    REG_DWORD    0x267b4e9") !== 0x267b4e9) fails.push("a signed-in account id not read");
  if (parseActiveUser("ERROR: The system was unable to find the specified registry key") !== null) fails.push("a missing key is not null");
  if (steamSignedIn(() => ({ stdout: "    ActiveUser    REG_DWORD    0x0\r\n" })) !== (process.platform === "win32" ? false : null)) fails.push("steamSignedIn did not report signed out");
  if (stallReason({ freezeDump: null, steam: false })) fails.push("no dump still gave a reason");
  if (!/Steam is not signed in on lf-cloud/.test(stallReason({ freezeDump: "x", steam: false, host: "lf-cloud" }))) fails.push("signed-out Steam not named");
  if (/Steam is not signed in/.test(stallReason({ freezeDump: "x", steam: true }))) fails.push("a signed-in Steam was blamed");
  return fails;
}

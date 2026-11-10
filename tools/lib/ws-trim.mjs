// Working-set trim for processes a session started. EmptyWorkingSet (psapi) moves a process's
// resident pages to the standby list: they still sit in RAM until another program needs them, but
// Windows counts them as available (os.freemem, the pool's RAM gate), and the process faults back
// only the pages it touches again. A session client waiting for the next harness command touches a
// small part of its 1 GB; trimming it is what Windows does itself under pressure, done before the
// pool refuses a session for want of RAM. Only pids recorded in a session.json are ever trimmed.

import { spawnSync } from "node:child_process";

export function trimScript(pids) {
  const ids = pids.filter((p) => Number.isInteger(p) && p > 0).join(",");
  if (!ids) return "";
  return [
    "Add-Type -Namespace LF -Name Ws -MemberDefinition '[DllImport(\"psapi.dll\")] public static extern bool EmptyWorkingSet(System.IntPtr h);'",
    `foreach ($i in @(${ids})) { $p = Get-Process -Id $i -ErrorAction SilentlyContinue; if ($p) { [void][LF.Ws]::EmptyWorkingSet($p.Handle) } }`,
  ].join("; ");
}

// Trims the pids; returns how many were asked (not how many succeeded: a pid that died is skipped).
export function trimProcs(pids, pwsh = "powershell.exe") {
  const ps = trimScript(pids);
  if (!ps) return 0;
  spawnSync(pwsh, ["-NoProfile", "-NonInteractive", "-Command", ps], { encoding: "utf8", windowsHide: true });
  return pids.length;
}

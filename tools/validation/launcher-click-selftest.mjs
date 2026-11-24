// Clicks each button of the playtest launcher's choice window (tools/release/launcher/play.ps1
// inputs: tools/**, agent-box/**, .githooks/**
// -ClickTest) and checks the pick comes back. playtest-m2-2 shipped a launcher whose buttons threw
// "The property 'Tag' cannot be found" on the first real click; the -ShotTo screenshot never clicks.
// red-proof (#4793 flake): the old window-based click (Shown event + PerformClick) timed out at 30 s on a loaded runner; with a 1 s timeout it fails every run (ETIMEDOUT). Now no window is shown.
// red-proof: with the click handler restored to `$form.Tag = $result` it printed
// "solo: no pick (picked=)", with no window on screen.
import { spawnSync } from "node:child_process";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

if (process.platform !== "win32") {
  console.log("launcher-click-selftest: skipped (Windows only)");
  process.exit(0);
}
const script = join(dirname(fileURLToPath(import.meta.url)), "..", "release", "launcher", "play.ps1");
const failures = [];
for (const pick of ["solo", "join"]) {
  const r = spawnSync("powershell", ["-NoProfile", "-ExecutionPolicy", "Bypass", "-File", script, "-ClickTest", pick], {
    encoding: "utf8",
    timeout: 120000, // hang guard only: the click is synchronous, so a slow runner cannot fail it (#4793)
    windowsHide: true,
  });
  const out = `${r.stdout || ""}${r.stderr || ""}`;
  if (r.error) failures.push(`${pick}: ${r.error.message}`);
  else if (!out.includes(`picked=${pick}`)) failures.push(`${pick}: no pick (${out.trim().split(/\r?\n/)[0] || `exit ${r.status}`})`);
}
if (failures.length) {
  for (const f of failures) console.error(`launcher-click-selftest: ${f}`);
  process.exit(1);
}
console.log("launcher-click-selftest: ok (solo and join clicks return their pick)");

// tools/validation/host-box-selftest.mjs
// inputs: tools/lib/host-box.mjs, tools/host/commands/box.mjs, tools/host/commands/autostart.mjs, tools/cli.mjs, tools/lib/addons.mjs
//
// Proves the rented-box mode (#1642). A rented box is a plain `git clone`, which is a main checkout, so
// `host start` and `host autostart install` used to refuse there. This builds a throwaway main checkout
// (a `git init` dir with a copy of tools/ and agent-box/, no addons/, no game server) and runs the real
// CLI in it:
//   - `host start` refuses and names `host box on`;
//   - with `LF_HOST_BOX=1`, or after `host box on` (a `.lf-host-box` marker), the refusal is gone and
//     `host start` stops with ONE `FAIL host start:` line that names each missing piece (addons/ and
//     ENFUSION_SERVER_PATH), not a stack trace from the launch-addons scan;
//   - `host autostart install --dry-run` works under the marker (a scheduled task carries no env);
//   - `host box off` brings the refusal back; `host box check` lists the same missing pieces.
// red-proof: before tools/lib/host-box.mjs existed this stopped at the import: "Cannot find module
// '.../tools/lib/host-box.mjs'"; and without the guard in lib/addons.mjs the box run died with
// "ENOENT: no such file or directory, scandir '<clone>/addons'" (2026-11-15).

import { cpSync, existsSync, mkdirSync, mkdtempSync, writeFileSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { tmpdir } from "node:os";
import { dirname, join, resolve, sep } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const { BOX_MARKER, boxMode, missingForHostStart } = await import("../lib/host-box.mjs");
const { safeRemoveTree } = await import("../link-safety.mjs");

let failed = 0;
function fail(msg) {
  console.log(`FAIL  ${msg}`);
  failed++;
}

// --- the pure parts
const scratch = mkdtempSync(join(tmpdir(), "lf-host-box-"));
try {
  const clone = join(scratch, "clone");
  mkdirSync(clone, { recursive: true });
  if (boxMode(clone, {}).on) fail("box mode is on with no env and no marker");
  if (!boxMode(clone, { LF_HOST_BOX: "1" }).on) fail("LF_HOST_BOX=1 did not turn box mode on");
  if (boxMode(clone, { LF_HOST_BOX: "0" }).on) fail("LF_HOST_BOX=0 turned box mode on");
  writeFileSync(join(clone, BOX_MARKER), "");
  if (!boxMode(clone, {}).on) fail("the marker file did not turn box mode on");
  const none = missingForHostStart(join(scratch, "empty"), { exe: join(scratch, "no-server", "ArmaReforgerServer.exe") });
  if (none.length !== 2 || !/addons/.test(none[0]) || !/ENFUSION_SERVER_PATH/.test(none[1])) fail(`missingForHostStart on an empty clone: ${JSON.stringify(none)}`);

  // --- the real CLI in a throwaway main checkout
  const main = join(scratch, "main");
  mkdirSync(main, { recursive: true });
  const skip = (src) => !src.includes("node_modules") && !src.includes(`${sep}tools${sep}mcp`);
  cpSync(join(ROOT, "tools"), join(main, "tools"), { recursive: true, filter: skip });
  cpSync(join(ROOT, "agent-box"), join(main, "agent-box"), { recursive: true, filter: skip });
  const git = spawnSync("git", ["init", "-q", main], { encoding: "utf8", windowsHide: true });
  if (git.status !== 0) fail(`git init failed: ${git.stderr}`);

  const env = { ...process.env, LF_HOST_DIR: join(scratch, "hostdir"), ENFUSION_SERVER_PATH: join(scratch, "no-server") };
  delete env.LF_HOST_BOX;
  const cli = (args, extra = {}) => {
    const r = spawnSync(process.execPath, [join(main, "tools", "cli.mjs"), ...args], { cwd: main, encoding: "utf8", timeout: 120000, windowsHide: true, env: { ...env, ...extra } });
    return { code: r.status, out: `${r.stdout ?? ""}${r.stderr ?? ""}` };
  };

  const plain = cli(["host", "start"]);
  if (plain.code === 0 || !/REFUSE/.test(plain.out)) fail(`host start in a main checkout did not refuse: ${plain.out.trim()}`);
  else if (!/host box on/.test(plain.out)) fail(`the refusal does not name 'host box on': ${plain.out.trim()}`);

  const viaEnv = cli(["host", "start"], { LF_HOST_BOX: "1" });
  const failLines = viaEnv.out.split(/\r?\n/).filter((l) => /^FAIL\s+host start:/.test(l));
  if (/REFUSE/.test(viaEnv.out)) fail(`LF_HOST_BOX=1 still refused: ${viaEnv.out.trim()}`);
  else if (viaEnv.code === 0 || failLines.length !== 1) fail(`want exactly one 'FAIL host start:' line, exit 1; got exit ${viaEnv.code}: ${viaEnv.out.trim()}`);
  else if (!/addons/.test(failLines[0]) || !/ENFUSION_SERVER_PATH/.test(failLines[0])) fail(`the FAIL line does not name both missing pieces: ${failLines[0]}`);
  if (/ENOENT|at .*\.mjs:\d+/.test(viaEnv.out)) fail(`a stack trace reached the operator: ${viaEnv.out.trim()}`);

  const check = cli(["host", "box", "check"]);
  if (check.code === 0 || !/addons/.test(check.out) || !/ENFUSION_SERVER_PATH/.test(check.out)) fail(`host box check did not list the missing pieces (exit ${check.code}): ${check.out.trim()}`);

  const on = cli(["host", "box", "on"]);
  if (on.code !== 0 || !existsSync(join(main, BOX_MARKER))) fail(`host box on did not write the marker (exit ${on.code}): ${on.out.trim()}`);
  const status = cli(["host", "box", "status"]);
  if (!/box mode: on/.test(status.out)) fail(`host box status after on: ${status.out.trim()}`);
  const viaMarker = cli(["host", "start"]);
  if (/REFUSE/.test(viaMarker.out) || !/^FAIL\s+host start:/m.test(viaMarker.out)) fail(`the marker did not lift the refusal: ${viaMarker.out.trim()}`);

  const auto = cli(["host", "autostart", "install", "--dry-run"]);
  if (auto.code !== 0 || /REFUSE/.test(auto.out) || !/dry-run/.test(auto.out)) fail(`autostart install --dry-run under the marker (exit ${auto.code}): ${auto.out.trim()}`);

  const off = cli(["host", "box", "off"]);
  if (off.code !== 0 || existsSync(join(main, BOX_MARKER))) fail(`host box off left the marker (exit ${off.code}): ${off.out.trim()}`);
  const again = cli(["host", "autostart", "install", "--dry-run"]);
  if (again.code === 0 || !/REFUSE/.test(again.out)) fail(`autostart did not refuse again after box off: ${again.out.trim()}`);
} finally {
  safeRemoveTree(scratch);
}

if (failed) {
  console.log(`host-box-selftest: ${failed} case(s) failed`);
  process.exit(1);
}
console.log("host-box-selftest: OK (refusal names host box on; env and marker lift it; one error line names the missing pieces; autostart dry-run; off restores)");

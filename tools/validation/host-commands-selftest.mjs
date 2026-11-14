// tools/validation/host-commands-selftest.mjs
// inputs: tools/cli.mjs, tools/host/**
//
// Proves `tools\cli host <sub>` falls through to tools/host/commands/<sub>.mjs
// (export `help` and `main`) when <sub> is not built in, and that `host help`
// lists those commands. The fixture `hello.mjs` lives in a temp commands dir
// that LF_HOST_COMMANDS_DIR points the CLI at, so no real file is touched.
// red-proof: before the fallthrough existed `host hello` printed the usage line
// and exited 1: "FAIL host hello did not run the fixture (exit 1)" (2026-11-14).

import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const CLI = join(ROOT, "tools", "cli.mjs");

const dir = mkdtempSync(join(tmpdir(), "lf-host-commands-"));
let failed = 0;

function fail(msg) {
  console.log(`FAIL  ${msg}`);
  failed++;
}

function runHost(args) {
  return spawnSync(process.execPath, [CLI, "host", ...args], {
    cwd: ROOT,
    encoding: "utf8",
    timeout: 60000,
    windowsHide: true,
    env: { ...process.env, LF_HOST_COMMANDS_DIR: dir },
  });
}

try {
  mkdirSync(dir, { recursive: true });
  writeFileSync(
    join(dir, "hello.mjs"),
    [
      'export const help = "hello [name]   fixture subcommand for the host-commands selftest";',
      "export async function main(args) {",
      '  console.log(`hello-fixture ran args=${args.join(",")}`);',
      "  return 0;",
      "}",
      "",
    ].join("\n"),
  );
  // A file that is not a valid subcommand name must be ignored.
  writeFileSync(join(dir, "README.md"), "not a command\n");

  const run = runHost(["hello", "world"]);
  const out = `${run.stdout ?? ""}${run.stderr ?? ""}`;
  if (run.status !== 0) fail(`host hello did not run the fixture (exit ${run.status})`);
  else if (!out.includes("hello-fixture ran args=world")) fail(`host hello ran but the fixture line is missing: ${out.trim()}`);

  const help = runHost(["help"]);
  const helpOut = `${help.stdout ?? ""}${help.stderr ?? ""}`;
  if (help.status !== 0) fail(`host help exited ${help.status}`);
  else if (!helpOut.includes("hello [name]   fixture subcommand")) fail(`host help does not list the fixture: ${helpOut.trim()}`);
  else if (!/start\|stop\|status/.test(helpOut)) fail("host help lost the built-in usage line");

  const bad = runHost(["nope-not-there"]);
  if (bad.status === 0) fail("an unknown host subcommand must exit non-zero");
} finally {
  rmSync(dir, { recursive: true, force: true });
}

if (failed) {
  console.log(`host-commands-selftest: ${failed} case(s) failed`);
  process.exit(1);
}
console.log("host-commands-selftest: OK (host <sub> falls through to tools/host/commands, host help lists it)");

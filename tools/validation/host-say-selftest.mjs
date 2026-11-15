// tools/validation/host-say-selftest.mjs
// inputs: tools/cli.mjs, tools/host/say.mjs, tools/host/commands/say.mjs
//
// Proves `tools\cli host say "<text>"` writes LifeFramework/host_say.txt in the host profile (a
// fixture dir via LF_HOST_DIR, never a real host), refuses an empty message and writes nothing,
// cuts text past 200 characters, and keeps one line.
// red-proof: before tools/host/commands/say.mjs existed `host say` printed the usage line and
// exited 1: "FAIL host say did not write the file" (2026-11-15).

import { existsSync, mkdtempSync, readFileSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, resolve } from "node:path";
import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const CLI = join(ROOT, "tools", "cli.mjs");
const dir = mkdtempSync(join(tmpdir(), "lf-host-say-"));
const FILE = join(dir, "profile", "LifeFramework", "host_say.txt");
let failed = 0;

function fail(msg) {
  console.log(`FAIL  ${msg}`);
  failed++;
}

function say(...args) {
  return spawnSync(process.execPath, [CLI, "host", "say", ...args], {
    cwd: ROOT,
    encoding: "utf8",
    timeout: 60000,
    windowsHide: true,
    env: { ...process.env, LF_HOST_DIR: dir },
  });
}

try {
  const ok = say("Hotfix restart in 10 minutes");
  if (ok.status !== 0 || !existsSync(FILE)) fail(`host say did not write the file (exit ${ok.status}): ${ok.stdout}${ok.stderr}`);
  else if (readFileSync(FILE, "utf8") !== "Hotfix restart in 10 minutes\n") fail(`host say wrote the wrong text: ${JSON.stringify(readFileSync(FILE, "utf8"))}`);
  rmSync(FILE, { force: true });

  for (const empty of [[], [""], ["   "]]) {
    const r = say(...empty);
    if (r.status === 0) fail(`host say with ${JSON.stringify(empty)} must refuse (exit 0)`);
    if (existsSync(FILE)) fail(`host say with ${JSON.stringify(empty)} left a file behind`);
  }

  const long = say("x".repeat(300));
  const written = existsSync(FILE) ? readFileSync(FILE, "utf8") : "";
  if (long.status !== 0 || written !== `${"x".repeat(200)}\n`) fail(`text over 200 characters was not cut to 200 (length ${written.trim().length})`);
  rmSync(FILE, { force: true });

  say("two", "words\nand a second line");
  if (existsSync(FILE) && readFileSync(FILE, "utf8") !== "two words and a second line\n") fail("a multi-line message was not folded to one line");
} finally {
  rmSync(dir, { recursive: true, force: true });
}

if (failed) {
  console.log(`host-say-selftest: ${failed} case(s) failed`);
  process.exit(1);
}
console.log("host-say-selftest: OK (host say writes the file, refuses empty, cuts at 200, one line)");

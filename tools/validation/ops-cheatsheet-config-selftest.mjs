// tools/validation/ops-cheatsheet-config-selftest.mjs
// inputs: docs/ops/session-cheatsheet.md, tools/host/commands/config.mjs
//
// Proves the cheatsheet's Before table lists `host config apply` (#1635): on a host that never
// ran it there is no server.json, so `host players` times out and `host ready` says NO-GO. The
// line must exist as a real command (not marked planned), sit above `host drift`, `host ready`
// and `host start`, and say what a missing server.json looks like.
// red-proof: 2026-11-15, on the cheatsheet before the line was added this printed
// "FAIL the Before table has no `tools\cli host config apply` line" and exited 1.

import { existsSync, readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
let failed = 0;
function fail(msg) {
  console.log(`FAIL  ${msg}`);
  failed++;
}

const md = readFileSync(join(ROOT, "docs", "ops", "session-cheatsheet.md"), "utf8");
if (!existsSync(join(ROOT, "tools", "host", "commands", "config.mjs"))) fail("tools/host/commands/config.mjs is gone: this selftest has nothing to prove");

const before = /^## Before\n([\s\S]*?)^## /m.exec(md)?.[1] ?? "";
const lines = before.split("\n");
const at = (re) => lines.findIndex((l) => re.test(l));
const apply = at(/host config apply/);
if (apply < 0) fail("the Before table has no `tools\\cli host config apply` line");
else {
  if (/planned/.test(lines[apply])) fail("`host config apply` is marked planned but the command exists");
  if (!/server\.json/.test(lines[apply])) fail("the `host config apply` line does not say what it writes (server.json)");
  for (const [name, re] of [["host drift", /host drift/], ["host ready", /host ready/], ["host start", /host start/]]) {
    const other = at(re);
    if (other >= 0 && other < apply) fail(`\`${name}\` comes before \`host config apply\` in the Before table`);
  }
}
if (!/host config apply/.test(/^## During\n([\s\S]*?)^## /m.exec(md)?.[1] ?? "")) fail("the During table's `host players` line does not point at `host config apply` for a timeout");

if (failed) {
  console.log(`SUMMARY ops-cheatsheet-config-selftest fail ${failed}`);
  process.exit(1);
}
console.log("SUMMARY ops-cheatsheet-config-selftest ok");

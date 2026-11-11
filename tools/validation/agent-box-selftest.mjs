// Runs the agent-box selftests that need no game: the harness command lines and
// screen-text parsing, the session addressing (a command never reaches another
// checkout's session), the journey judges and report, and the placement audit's checks.
// red-proof: with session.mjs sessionAddress ignoring the recorded root this
// printed "another checkout's session must be refused" and exited 1.
import { spawnSync } from "node:child_process";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "../..");
const runs = [
  ["harness", ["agent-box/harness.mjs", "selftest"]],
  ["session", ["agent-box/session.mjs", "selftest"]],
  ["journey", ["agent-box/journey.mjs", "selftest"]],
  ["place-audit", ["agent-box/place-audit.mjs", "selftest"]],
];
let failed = 0;
for (const [name, args] of runs) {
  const r = spawnSync(process.execPath, args.map((a, i) => (i === 0 ? join(ROOT, a) : a)), { cwd: ROOT, encoding: "utf8", windowsHide: true });
  if (r.status !== 0) {
    failed++;
    console.log(`agent-box-selftest: ${name} FAILED`);
    process.stdout.write(`${r.stdout || ""}${r.stderr || ""}`);
  }
}
if (failed) process.exit(1);
console.log("agent-box-selftest: OK (harness, session, journey, place-audit)");

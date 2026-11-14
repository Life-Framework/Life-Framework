// tools\cli config-gate [--static | --sessions | --full]: the config harness as the gate and the release run it (#301).
//   --static    parse + validate over every fixture, offline, ~1 s (always part of the gate)
//   --sessions  the game stages: the shared-server client stage (#300), then the boot stage (#299)
//               when tools/config/boot-run.mjs exists; each boots through the pool and queues like a session
//   --full      static, then sessions (the release run); fails on the first failing part's result
// Quiet: one line per part; a part's own output is printed only when it fails.
import { existsSync } from "node:fs";
import { spawnSync } from "node:child_process";
import { join } from "node:path";

export const help = "config-gate [--static|--sessions|--full]   the config harness the way wt gate and release run it: static always, game sessions on demand";

const sh = (root, args) => spawnSync(process.execPath, args, { cwd: root, encoding: "utf8", windowsHide: true, maxBuffer: 1 << 26, timeout: 20 * 60000 });
const session = (root, args) => sh(root, [join(root, "agent-box", "session.mjs"), ...args]);

function part(label, r) {
  if (r.status === 0) { console.log(`PASS  config harness: ${label}`); return 0; }
  process.stdout.write((r.stdout ?? "") + (r.stderr ?? ""));
  console.log(`FAIL  config harness: ${label} (exit ${r.status})`);
  return 1;
}

export function runStatic(root) {
  return part("static (parse + validate)", sh(root, [join(root, "tools", "config", "harness.mjs"), "--stage", "all"]));
}

// Client stage: one shared server, every fixture with client reads applied in turn.
export function runClientStage(root) {
  const t0 = Date.now();
  let code = 1;
  try {
    const up = session(root, ["up", "--clients", "1", "--wait"]);
    if (up.status !== 0) return part("client stage, session up", up);
    code = part("client stage (shared server)", session(root, ["config-harness-proof"]));
  } finally {
    session(root, ["down"]);
  }
  console.log(`      client stage took ${Math.round((Date.now() - t0) / 1000)} s`);
  return code;
}

// Boot stage (#299): a fresh dedicated server per boot fixture. Present once that PR lands; absent, it is a no-op hook.
export async function runBootStage(root) {
  const f = join(root, "tools", "config", "boot-run.mjs");
  if (!existsSync(f)) { console.log("SKIP  config harness: boot stage not present (tools/config/boot-run.mjs, #299)"); return 0; }
  const { runBootStage: run } = await import(new URL(`file:///${f.split("\\").join("/")}`).href);
  return run() === 0 ? 0 : 1;
}

export async function runSessions(root) {
  const c = runClientStage(root);
  const b = await runBootStage(root);
  return c === 0 && b === 0 ? 0 : 1;
}

export async function main(args, { root } = {}) {
  root = root ?? process.cwd();
  const full = args.includes("--full");
  if (args.includes("--static") && !full) return runStatic(root);
  if (args.includes("--sessions") && !full) return runSessions(root);
  const s = runStatic(root);
  if (s !== 0) return 1;
  return runSessions(root);
}

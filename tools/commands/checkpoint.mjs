// tools\cli checkpoint <hour> [--client <n>]: load a progression checkpoint (agent-box/checkpoints/hour-N.json) on the
// running session's character. A thin alias for `node agent-box/harness.mjs --client <n> send checkpoint <hour>`:
// agents typed `tools\cli checkpoint` and `harness.mjs checkpoint 5` (2026-11-25 round, tooling note 1) and got the
// harness usage instead. `session up` mirrors the checkpoints folder into the server profile (#4867).
import { spawnSync } from "node:child_process";
import { existsSync, readdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

export const help = "checkpoint <hour> [--client <n>]   load agent-box/checkpoints/hour-N.json on the session character (harness send checkpoint)";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");

export function hours(dir = join(ROOT, "agent-box", "checkpoints")) {
  try {
    return readdirSync(dir).map((n) => /^hour-(\d+)\.json$/.exec(n)?.[1]).filter(Boolean).map(Number).sort((a, b) => a - b);
  } catch {
    return [];
  }
}

// The harness argv for `checkpoint <args>`, or { error }. Exported for the selftest.
export function harnessArgs(args, known = hours()) {
  let client = null;
  const words = [];
  for (let i = 0; i < args.length; i++) {
    if (args[i] === "--client") { client = args[++i]; continue; }
    words.push(args[i]);
  }
  const hour = words[0];
  if (hour === undefined || !/^\d+$/.test(hour)) return { error: `usage: tools\\cli checkpoint <hour> [--client <n>]   (hours: ${known.join(", ") || "none found"})` };
  if (known.length && !known.includes(Number(hour))) return { error: `no agent-box/checkpoints/hour-${hour}.json (hours: ${known.join(", ")})` };
  if (client !== null && !/^\d+$/.test(String(client))) return { error: `--client takes a number, got '${client}'` };
  return { argv: [...(client !== null ? ["--client", String(client)] : []), "send", "checkpoint", hour, ...words.slice(1)] };
}

export async function main(args) {
  const a = harnessArgs(args);
  if (a.error) {
    console.log(a.error);
    return 1;
  }
  const harness = join(ROOT, "agent-box", "harness.mjs");
  if (!existsSync(harness)) {
    console.log(`FAIL  ${harness} is missing`);
    return 1;
  }
  console.log(`checkpoint: node agent-box/harness.mjs ${a.argv.join(" ")}`);
  const r = spawnSync(process.execPath, [harness, ...a.argv], { cwd: ROOT, stdio: "inherit", windowsHide: true });
  return r.status ?? 1;
}

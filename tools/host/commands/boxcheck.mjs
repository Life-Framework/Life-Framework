// `tools\cli host boxcheck [--ssh <pool machine|ssh host>] [--server-path <dir>]` (#1496). The logic
// is tools/host/boxcheck.mjs. Reads cores, clock, RAM, free disk and the Arma Reforger Server
// install of this machine, or of a pool machine over ssh (read only, bounded 45 s), compares them
// with the 40-player spec in docs/ops/rented-box.md step 2, prints one line per item and a verdict.
// Exit 0 on pass, 1 on a failed item or a machine that could not be read.
import { readFileSync } from "node:fs";
import { hostname } from "node:os";
import { join } from "node:path";
import { compare, DEFAULT_SERVER_DIR, localFacts, parseRemote, remoteScript, SPEC } from "../boxcheck.mjs";

export const help = `boxcheck [--ssh <pool machine>] [--server-path <dir>]   measure this machine or a pool machine against the 40-player box spec (${SPEC.cores}+ cores, ${SPEC.ramGb} GB RAM, ${SPEC.diskGb} GB free, server installed); exits 1 on a miss`;

const TAG = "[LFDebug:HostBoxcheck]";
const SSH_TIMEOUT_MS = 45000;

function flag(args, name) {
  const i = args.indexOf(name);
  return i >= 0 ? args[i + 1] : undefined;
}

// A pool machine name (tools/pool.json) gives its ssh host; any other word is used as an ssh host.
function sshHostOf(root, name) {
  try {
    const pool = JSON.parse(readFileSync(join(root, "tools", "pool.json"), "utf8"));
    const m = (pool.machines ?? []).find((x) => x.name === name);
    if (m && (m.via === "local" || (m.hostname && m.hostname.toLowerCase() === hostname().toLowerCase()))) return { local: true };
    if (m && m.host) return { host: m.host };
  } catch {
    // no pool file: treat the word as a host
  }
  return { host: name };
}

async function localServerDir(root, args) {
  let dir = flag(args, "--server-path") || process.env.ENFUSION_SERVER_PATH;
  if (!dir) {
    try {
      const { localEnv } = await import("../../lib/local-config.mjs");
      dir = localEnv(root).ENFUSION_SERVER_PATH;
    } catch {
      dir = undefined;
    }
  }
  return String(dir || DEFAULT_SERVER_DIR).replace(/[\\/]ArmaReforgerServer\.exe$/i, "");
}

export async function main(args, ctx) {
  if ((args.includes("--ssh") && !flag(args, "--ssh")) || (args.includes("--server-path") && !flag(args, "--server-path"))) {
    console.error(`${TAG} --ssh needs a pool machine or ssh host, --server-path needs a folder`);
    return 1;
  }
  const machine = flag(args, "--ssh");
  let facts = ctx.facts; // injected by the selftest
  let label = "this machine";

  if (!facts && machine) {
    label = machine;
    const target = sshHostOf(ctx.root, machine);
    const dir = flag(args, "--server-path") || DEFAULT_SERVER_DIR;
    if (target.local) {
      // the pool machine is this PC: no ssh, read it directly
      facts = localFacts(await localServerDir(ctx.root, args));
    } else try {
      const tb = await import("../../remote/testbox.mjs");
      const res = tb.sshPs(target.host, remoteScript(dir, tb.psQuote), { connectTimeout: 8, timeout: SSH_TIMEOUT_MS });
      if (res.error || res.status !== 0) {
        console.error(`${TAG} could not read ${machine}: ${res.error ? res.error.message : `ssh exit ${res.status}`} ${String(res.stderr || "").trim().split(/\r?\n/)[0]}`.trimEnd());
        return 1;
      }
      facts = parseRemote(res.stdout, dir);
    } catch (e) {
      console.error(`${TAG} could not read ${machine}: ${e.message}`);
      return 1;
    }
  } else if (!facts) {
    facts = localFacts(await localServerDir(ctx.root, args));
  }

  const r = compare(facts);
  console.log(`${TAG} box spec for 40 players (docs/ops/rented-box.md step 2), measured on ${label}`);
  for (const i of r.items) console.log(`${TAG} ${i.item.padEnd(7)} ${i.status.toUpperCase().padEnd(4)} ${i.text}`);
  if (r.ok) console.log(`${TAG} verdict: PASS, ${label} meets the spec`);
  else console.log(`${TAG} verdict: FAIL, ${r.failed} of ${r.items.filter((i) => i.status !== "info").length} checks missed`);
  return r.ok ? 0 : 1;
}

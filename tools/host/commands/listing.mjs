// `tools\cli host listing [address:a2sport] [--remote 0grind] [--version V] [--map M] [--open] [--config path]`:
// asks the running server what the server browser shows (A2S_INFO) and compares name, max
// players with server/flagship/server.json. Fails when the name is not exactly
// "Life Framework | Everon 1989 (Alpha)". The password is not checked: Reforger's A2S does not report it
// ("password: not reported by A2S"). --version and --map add those checks (server.json has neither).
import { join } from "node:path";
import { parseAddress, TIMEOUT_MS } from "../a2s.mjs";
import { loadExpected, passwordLine, runListing, udpExchange } from "../listing.mjs";
import { FLAGSHIP_0GRIND } from "../remote-0grind.mjs";

export const help =
  "listing [address:a2sport] [--remote 0grind] [--version V] [--map M]   compare what the server browser shows (name, map, max players) with server/flagship/server.json (the password is not reported by A2S); --remote 0grind asks the rented box the alpha runs on";

// The rented boxes `--remote` knows, by name: "host:a2sport".
const REMOTES = { "0grind": `${FLAGSHIP_0GRIND.address}:${FLAGSHIP_0GRIND.a2sPort}` };

/** The address to ask: a typed address wins, else the named remote's A2S address, else undefined (the local host). */
export function listingTarget(words, remote) {
  if (words[0]) return words[0];
  if (!remote) return undefined;
  if (!REMOTES[remote]) throw new Error(`unknown remote '${remote}'; known: ${Object.keys(REMOTES).join(", ")}`);
  return REMOTES[remote];
}

export async function main(args, ctx = {}) {
  const words = [];
  const opts = {};
  let remote = null;
  let configPath = join(ctx.root ?? process.cwd(), "server", "flagship", "server.json");
  for (let i = 0; i < args.length; i++) {
    const a = args[i];
    if (a === "--open") continue; // accepted and ignored: the password is no longer checked
    else if (a === "--version") opts.version = args[++i];
    else if (a === "--map") opts.map = args[++i];
    else if (a === "--config") configPath = args[++i];
    else if (a === "--remote") remote = args[++i];
    else words.push(a);
  }
  let addr;
  let expected;
  try {
    addr = parseAddress(listingTarget(words, remote));
    expected = loadExpected(configPath);
  } catch (e) {
    console.log(`[LFHost:listing] ${e.message}`);
    return 1;
  }
  const r = await runListing({ exchange: udpExchange(addr.host, addr.port, TIMEOUT_MS), expected, ...opts });
  if (r.info) console.log(`[LFHost:listing] ${addr.host}:${addr.port} name='${r.info.name}' map='${r.info.map}' players=${r.info.players}/${r.info.maxPlayers} ${passwordLine()} version='${r.info.version}'`);
  for (const p of r.problems) console.log(`[LFHost:listing] FAIL ${p}`);
  if (r.ok) console.log("[LFHost:listing] ok");
  return r.ok ? 0 : 1;
}

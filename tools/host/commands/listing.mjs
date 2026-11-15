// `tools\cli host listing [address:a2sport] [--version V] [--map M] [--open] [--config path]`:
// asks the running server what the server browser shows (A2S_INFO) and compares name, max
// players and the password flag with server/flagship/server.json. Fails when the name is not
// exactly "Life Framework | Everon 1989 (Alpha)" or the password flag is off. --open waives the
// password requirement (after the alpha); --version and --map add those checks (server.json has neither).
import { join } from "node:path";
import { parseAddress, TIMEOUT_MS } from "../a2s.mjs";
import { loadExpected, runListing, udpExchange } from "../listing.mjs";

export const help = "listing [address:a2sport] [--version V] [--map M] [--open]   compare what the server browser shows with server/flagship/server.json";

export async function main(args, ctx = {}) {
  const words = [];
  const opts = {};
  let configPath = join(ctx.root ?? process.cwd(), "server", "flagship", "server.json");
  for (let i = 0; i < args.length; i++) {
    const a = args[i];
    if (a === "--open") opts.allowOpen = true;
    else if (a === "--version") opts.version = args[++i];
    else if (a === "--map") opts.map = args[++i];
    else if (a === "--config") configPath = args[++i];
    else words.push(a);
  }
  let addr;
  let expected;
  try {
    addr = parseAddress(words[0]);
    expected = loadExpected(configPath);
  } catch (e) {
    console.log(`[LFHost:listing] ${e.message}`);
    return 1;
  }
  const r = await runListing({ exchange: udpExchange(addr.host, addr.port, TIMEOUT_MS), expected, ...opts });
  if (r.info) console.log(`[LFHost:listing] ${addr.host}:${addr.port} name='${r.info.name}' map='${r.info.map}' players=${r.info.players}/${r.info.maxPlayers} password=${r.info.password ? "on" : "off"} version='${r.info.version}'`);
  for (const p of r.problems) console.log(`[LFHost:listing] FAIL ${p}`);
  if (r.ok) console.log("[LFHost:listing] ok");
  return r.ok ? 0 : 1;
}

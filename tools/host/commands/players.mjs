// `tools\cli host players [address:a2sport]`: asks a running server for its name, map and
// player count over A2S_INFO (3 s timeout). Default address 127.0.0.1:17777. Operator tool:
// the session runbook uses it to see whether anyone is on before a restart.
import { parseAddress, queryInfo, TIMEOUT_MS } from "../a2s.mjs";
import { passwordLine } from "../listing.mjs";

export const help = "players [address:a2sport]   A2S query: server name, map, players and max players (default 127.0.0.1:17777)";

export function formatInfo(info) {
  const lines = [
    `name:    ${info.name}`,
    `map:     ${info.map}`,
    `players: ${info.players}/${info.maxPlayers}`,
  ];
  // Reforger's A2S never reports the password: the visibility byte is 0 on a passworded server, so
  // "none (public)" would be a lie. A wrong password is refused live.
  lines.push(passwordLine());
  return lines.join("\n");
}

export async function main(args) {
  let addr;
  try {
    addr = parseAddress(args[0]);
  } catch (e) {
    console.log(`[LFHost:players] ${e.message}`);
    return 1;
  }
  try {
    const info = await queryInfo(addr.host, addr.port, { timeoutMs: TIMEOUT_MS });
    console.log(`[LFHost:players] ${addr.host}:${addr.port}`);
    console.log(formatInfo(info));
    return 0;
  } catch (e) {
    console.log(`[LFHost:players] ${e.message}`);
    return 1;
  }
}

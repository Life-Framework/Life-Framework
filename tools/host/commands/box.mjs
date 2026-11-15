// `tools\cli host box on|off|status|check` (#1642). The logic is tools/lib/host-box.mjs.
//
// Box mode is for a rented machine whose clone is a plain `git clone` (a main checkout): it lifts the
// main-checkout refusal of `host start|watch|autostart`. `on` writes the `.lf-host-box` marker in the repo
// root (git-ignored), `off` removes it. `check` lists what this clone still lacks.
import { existsSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { localEnv } from "../../lib/local-config.mjs";
import { BOX_MARKER, boxMode, missingForHostStart, optionalPieces } from "../../lib/host-box.mjs";

export const help = "box on|off|status|check   rented-box mode: on writes .lf-host-box so `host start|watch|autostart` run in a plain clone (the main checkout), off removes it, check lists what this clone lacks (addons/, the game server, website/ for announce)";

const say = (m) => console.log(`[LFDebug:HostBox] ${m}`);

function serverExePath(ctx) {
  const configured = process.env.ENFUSION_SERVER_PATH || localEnv(ctx.root).ENFUSION_SERVER_PATH || "C:/Program Files (x86)/Steam/steamapps/common/Arma Reforger Server";
  return /\.exe$/i.test(configured) ? configured : join(configured, "ArmaReforgerServer.exe");
}

export async function main(args, ctx) {
  const sub = args[0];
  const marker = join(ctx.root, BOX_MARKER);
  if (sub === "on") {
    writeFileSync(marker, "rented-box mode: `tools\\cli host box off` removes this file\n");
    say(`box mode on: ${BOX_MARKER} written in ${ctx.root}; host start|watch|autostart now run in this clone`);
    return 0;
  }
  if (sub === "off") {
    if (existsSync(marker)) rmSync(marker, { force: true });
    say(`box mode off${boxMode(ctx.root, process.env).on ? " for the file; LF_HOST_BOX is still set in the environment" : ""}`);
    return 0;
  }
  if (sub === "status") {
    const m = boxMode(ctx.root, process.env);
    say(m.on ? `box mode: on (${m.why})` : "box mode: off (the main-checkout refusal applies; `host box on` lifts it on a rented box)");
    return 0;
  }
  if (sub === "check") {
    const exe = serverExePath(ctx);
    const missing = missingForHostStart(ctx.root, { exe });
    for (const m of missing) say(`MISSING ${m}`);
    for (const o of optionalPieces(ctx.root)) say(`optional ${o}`);
    if (!missing.length) say("this clone has what `host start` needs");
    return missing.length ? 1 : 0;
  }
  say("usage: host box on|off|status|check");
  return 1;
}

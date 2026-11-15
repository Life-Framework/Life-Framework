// `tools\cli host password rotate|show [<host dir>]` (#1415). The logic is tools/host/password.mjs.
import { rotatePassword, showPassword } from "../password.mjs";

export const help = "password rotate|show [<host dir>] [--repo dir]   rotate writes a new readable join password to the host's secrets.json, re-applies server.json (host config apply) and prints the invite line; show prints the current one; the host must be stopped to rotate";

const RESTART = "the join password is only enforced by Bohemia's backend (#1520): on a backend-registered server a change takes effect after a restart (tools\\cli host stop, then start)";

export async function main(args, ctx) {
  const [sub, ...rest] = args;
  if (sub !== "rotate" && sub !== "show") {
    console.error("usage: tools\\cli host password rotate|show [<host dir>] [--repo <dir>]");
    return 1;
  }
  let repoDir;
  let dir = ctx.hostDir;
  for (let i = 0; i < rest.length; i++) {
    if (rest[i] === "--repo") repoDir = rest[++i];
    else if (rest[i].startsWith("--")) {
      console.error(`[LFDebug:HostPassword] unknown flag ${rest[i]}`);
      return 1;
    } else dir = rest[i];
  }
  if (sub === "show") {
    const r = showPassword(dir);
    if (!r.ok) {
      console.error(`[LFDebug:HostPassword] ${r.reason}`);
      return 1;
    }
    console.log(r.password);
    return 0;
  }
  const r = rotatePassword(dir, repoDir ? { repoDir } : {});
  if (!r.ok) {
    console.error(`[LFDebug:HostPassword] refused: ${r.reason}`);
    return 1;
  }
  for (const w of r.warnings) console.log(`[LFDebug:HostPassword] ${w}`);
  console.log(`[LFDebug:HostPassword] rotated: ${r.written.length} file(s) written (secrets.json, server.json, config.json)`);
  console.log(`[LFDebug:HostPassword] ${RESTART}`);
  console.log(`[LFDebug:HostPassword] invite: ${r.invite}`);
  return 0;
}

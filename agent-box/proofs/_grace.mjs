import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { ROOT, sessionPaths } from "../session.mjs";

// A robbery proof needs the new-player grace period off: a fresh island account is in its 30 minute grace (the alpha
// default), and the server answers a vault, till or van robbery with `#LF-Grace_Refused` and logs "is in its grace
// period". island-cop-proof does this for itself; the vault, heist and door proofs used to pass only when a cop proof had
// run first in the same session (2026-11-27). Writes the flagship owner config with `grace.minutes` set where the server
// reads it, then asks a client to reload it. \param on (n, spec, timeout) the proof's own sender.
export function graceOff(on, dir, wait, minutes = 0) {
  const flagship = JSON.parse(readFileSync(join(ROOT, "server", "flagship", "config.json"), "utf8"));
  flagship.grace = { minutes };
  const ownerFile = join(sessionPaths(dir).server, "profile", "LifeFramework", "config.json");
  mkdirSync(dirname(ownerFile), { recursive: true });
  writeFileSync(ownerFile, JSON.stringify(flagship, null, 2));
  on(1, { verb: "configreload" }, 10000);
  wait(3500);
}

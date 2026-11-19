// tools/validation/town-nightlights-selftest.mjs
// inputs: tools/town/**, docs/everon/places/**, addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers/Lights_*.layer
//
// Runs tools/town/nightlights.selftest.mjs (card #3534) so `tools\cli validate` covers it: the Saint-Philippe night-light layer matches its
// generator and holds the card's counts (25 to 40 window glows, 10 to 15 lamp pieces).
// red-proof: see tools/town/nightlights.selftest.mjs.
import { spawnSync } from "node:child_process";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const r = spawnSync(process.execPath, [join(ROOT, "tools", "town", "nightlights.selftest.mjs")], { cwd: ROOT, encoding: "utf8" });
process.stdout.write(r.stdout ?? "");
process.stderr.write(r.stderr ?? "");
process.exit(r.status ?? 1);

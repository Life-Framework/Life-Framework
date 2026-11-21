// tools/validation/town-street-life-selftest.mjs
// inputs: tools/town/street-life.mjs, tools/town/street-life.selftest.mjs, tools/town/terrain/saint-philippe-street-life.json, addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers/Place_saint-philippe_street-life.layer
//
// Runs tools/town/street-life.selftest.mjs (card #3880) so `tools\cli validate` covers it: every street-life bench stands on ground the
// server probed, level under the seat and not on the entrance stairs.
// red-proof: see tools/town/street-life.selftest.mjs.
import { spawnSync } from "node:child_process";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const r = spawnSync(process.execPath, [join(ROOT, "tools", "town", "street-life.selftest.mjs")], { cwd: ROOT, encoding: "utf8" });
process.stdout.write(r.stdout ?? "");
process.stderr.write(r.stderr ?? "");
process.exit(r.status ?? 1);

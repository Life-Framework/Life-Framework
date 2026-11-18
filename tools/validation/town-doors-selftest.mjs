// tools/validation/town-doors-selftest.mjs
// inputs: tools/town/**, docs/everon/places/**, addons/LF_Everon1989/Worlds/MainWorld/MainWorld_Layers/**
//
// Runs tools/town/doors.selftest.mjs (card #3202) so `tools\cli validate` covers it: `town doors` fails an enterable building with no
// purpose row or no carrier in the world, and the committed Saint-Philippe place lists its 130 buildings.
// red-proof: see tools/town/doors.selftest.mjs.
import { spawnSync } from "node:child_process";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const r = spawnSync(process.execPath, [join(ROOT, "tools", "town", "doors.selftest.mjs")], { cwd: ROOT, encoding: "utf8" });
process.stdout.write(r.stdout ?? "");
process.stderr.write(r.stderr ?? "");
process.exit(r.status ?? 1);

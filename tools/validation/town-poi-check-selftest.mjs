// tools/validation/town-poi-check-selftest.mjs
// inputs: tools/town/**, tools/commands/town.mjs, addons/*/Configs/Map/LF_MapPois*.conf
//
// Runs tools/town/poi-check.selftest.mjs (card M17, #2245) so `tools\cli validate` covers it: a town template slot names its
// POI kind (or "none"), and `town map --check` fails a slot whose front kind has no POI row.
// red-proof: see tools/town/poi-check.selftest.mjs.
import { spawnSync } from "node:child_process";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const r = spawnSync(process.execPath, [join(ROOT, "tools", "town", "poi-check.selftest.mjs")], { cwd: ROOT, encoding: "utf8" });
process.stdout.write(r.stdout ?? "");
process.stderr.write(r.stderr ?? "");
process.exit(r.status ?? 1);

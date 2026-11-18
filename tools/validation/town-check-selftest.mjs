// tools/validation/town-check-selftest.mjs
// inputs: tools/town/**, tools/perf/building-count.mjs
//
// Runs tools/town/check.selftest.mjs (card #2937) so `tools\cli validate` covers it: `town check` fails a fitted slot whose kind has a
// marker and no working component (a shop counter with no LF_ShopComponent, a jail with no cell).
// red-proof: see tools/town/check.selftest.mjs.
import { spawnSync } from "node:child_process";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const r = spawnSync(process.execPath, [join(ROOT, "tools", "town", "check.selftest.mjs")], { cwd: ROOT, encoding: "utf8" });
process.stdout.write(r.stdout ?? "");
process.stderr.write(r.stderr ?? "");
process.exit(r.status ?? 1);

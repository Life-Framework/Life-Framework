// tools/validation/town-vegetation-selftest.mjs
// inputs: tools/town/**, tools/validation/town-vegetation-selftest.mjs
//
// Runs tools/town/vegetation.selftest.mjs (#4724) so `tools\cli validate` covers `town vegetation`: the clearing's boxes, which scanned trees and
// bushes fall in them, the kit's boxed clear entries and the fit's carriers.
// red-proof: see tools/town/vegetation.selftest.mjs.
import { spawnSync } from "node:child_process";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const r = spawnSync(process.execPath, [join(ROOT, "tools", "town", "vegetation.selftest.mjs")], { cwd: ROOT, encoding: "utf8" });
process.stdout.write(r.stdout ?? "");
process.stderr.write(r.stderr ?? "");
process.exit(r.status ?? 1);

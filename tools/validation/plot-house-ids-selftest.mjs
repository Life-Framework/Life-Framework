// tools/validation/plot-house-ids-selftest.mjs - validate-plot-house-ids (#2783).
// inputs: tools/validation/validate-plot-house-ids.mjs, tools/lib/plot-house-ids.mjs, tools/validation/plot-house-ids-selftest.mjs
//
//   - the readers: plot keys from a conf, house ids from an LF_HouseManagerComponent only (a key component's id is not a house);
//   - a temp tree whose pack conf names a plot like a core house identifier fails with both names and files;
//   - the same tree with the plot renamed passes.
//
// red-proof: 2026-11-18 with collisions() returning [] the "colliding plot key fails" check printed FAIL.
import { spawnSync } from "node:child_process";
import { mkdirSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { collisions, houseIds, plotKeys } from "../lib/plot-house-ids.mjs";
import { scratchDir } from "../lib/scratch.mjs";

let failed = 0;
const check = (ok, msg) => {
  console.log(`${ok ? "PASS" : "FAIL"}  ${msg}`);
  if (!ok) failed++;
};

const CONF = (id) => `LF_PropertyConfig {\n m_aPlots {\n  LF_PropertyPlotDef "{B0A2000000000031}" {\n   m_sPlotId "${id}"\n   m_iPrice 5000\n  }\n }\n}\n`;
const LAYER = `GenericEntity house : "{0123456789ABCDEF}Prefabs/H.et" {
 components {
  LF_HouseManagerComponent "{AAAAAAAAAAAAAAAA}" {
   m_bStartLocked 1
   m_sDebugIdentifier "H1"
  }
  LF_KeyComponent "{BBBBBBBBBBBBBBBB}" {
   m_sDebugIdentifier "K1"
  }
  LF_LockComponent "{CCCCCCCCCCCCCCCC}" {
   m_sDebugIdentifier "D1"
  }
 }
}
`;

check(plotKeys(CONF("plot-a")).map((p) => `${p.id}@${p.line}`).join() === "plot-a@4", "plotKeys reads the key and its line");
check(houseIds(LAYER).map((h) => `${h.id}@${h.line}`).join() === "H1@5,D1@11", "houseIds reads the house manager's and a door's identifier, not the key component's");
check(houseIds('LF_HouseManagerComponent {\n m_sDebugIdentifier "P1"\n}\n').map((h) => h.id).join() === "P1", "a prefab component with no GUID header is read");
check(collisions([{ file: "c", id: "H1", line: 1 }], [{ file: "l", id: "H1", line: 2 }]).length === 1, "colliding plot key fails");
check(collisions([{ file: "c", id: "plot-a", line: 1 }], [{ file: "l", id: "H1", line: 2 }]).length === 0, "distinct ids pass");

const validator = join(dirname(fileURLToPath(import.meta.url)), "validate-plot-house-ids.mjs");
const dir = scratchDir("lf-plot-house-ids-");
const put = (rel, text) => {
  mkdirSync(dirname(join(dir, rel)), { recursive: true });
  writeFileSync(join(dir, rel), text);
};
put("addons/LifeFramework/Worlds/W/W_Layers/a.layer", LAYER);
put("addons/LifeFramework/Configs/Property/LF_Property.conf", CONF("plot-a"));
const run = () => spawnSync(process.execPath, [validator], { env: { ...process.env, LF_PLOTHOUSE_ROOT: dir }, encoding: "utf8" });
const ok = run();
check(ok.status === 0, `a clean tree passes (${ok.stdout.trim()})`);
put("addons/LF_Everon1989/Configs/Property/LF_Property.conf", CONF("H1"));
const bad = run();
check(bad.status === 1 && /LF_Everon1989\/Configs\/Property\/LF_Property\.conf:4 plot key "H1" equals the house identifier "H1" set at addons\/LifeFramework\/Worlds\/W\/W_Layers\/a\.layer:5/.test(bad.stdout), `a pack plot named like a core house fails, naming both (${bad.stdout.trim()})`);
put("addons/LF_Everon1989/Configs/Property/LF_Property.conf", CONF("H1-plot"));
check(run().status === 0, "renaming the plot passes");

const real = spawnSync(process.execPath, [validator], { encoding: "utf8" });
check(real.status === 0, `the repo has no colliding plot key (${real.stdout.trim().split("\n").pop()})`);

console.log(failed ? `plot-house-ids-selftest: ${failed} FAIL` : "plot-house-ids-selftest: OK");
process.exit(failed ? 1 : 0);

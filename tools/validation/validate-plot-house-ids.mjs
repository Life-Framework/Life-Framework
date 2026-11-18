// No plot key in a Configs/Property conf equals a house identifier (#2783).
// inputs: addons/**/Configs/Property/*.conf, addons/**/*.layer, addons/**/*.et, tools/lib/plot-house-ids.mjs
//
// LF_Properties keys houses and plots in one table and a house row wins, so a plot key equal to a house id
// would sell that house at the plot price. Players cannot trigger it; a config mistake can. The runtime refuses
// the plot at config load (LF_PropertyConfig.RefuseHouseCollisions); this check fails the PR before it gets there.
// Core and pack are read together: both load on one server. Identifiers come from `m_sDebugIdentifier` on an
// LF_HouseManagerComponent or an LF_LockComponent (a door) in any layer or prefab.
//
// LF_PLOTHOUSE_ROOT points the scan at another tree (the selftest).
// red-proof: 2026-11-18, tools/validation/plot-house-ids-selftest.mjs plants a plot key equal to a house id in a
// temp tree: this exits 1 with `FAIL ...LF_Property.conf:4 plot key "H1" equals the house identifier`.
import { existsSync, readdirSync, readFileSync, statSync } from "node:fs";
import { dirname, join, relative } from "node:path";
import { fileURLToPath } from "node:url";
import { collisions, houseIds, plotKeys } from "../lib/plot-house-ids.mjs";

const root = process.env.LF_PLOTHOUSE_ROOT || join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const rel = (p) => relative(root, p).split("\\").join("/");

function walk(dir, test, out) {
  for (const name of readdirSync(dir)) {
    if (name === "node_modules" || name === ".git") continue;
    const p = join(dir, name);
    if (statSync(p).isDirectory()) walk(p, test, out);
    else if (test(name, p)) out.push(p);
  }
  return out;
}

const addons = join(root, "addons");
const plots = [];
const houses = [];
let confs = 0;
if (existsSync(addons)) {
  for (const addon of readdirSync(addons)) {
    if (!statSync(join(addons, addon)).isDirectory()) continue;
    const propDir = join(addons, addon, "Configs", "Property");
    if (existsSync(propDir)) {
      for (const f of walk(propDir, (n) => n.endsWith(".conf"), [])) {
        confs++;
        for (const k of plotKeys(readFileSync(f, "utf8"))) plots.push({ file: rel(f), ...k });
      }
    }
    for (const f of walk(join(addons, addon), (n) => n.endsWith(".layer") || n.endsWith(".et"), [])) {
      const text = readFileSync(f, "utf8");
      if (!text.includes("LF_HouseManagerComponent") && !text.includes("LF_LockComponent")) continue;
      for (const h of houseIds(text)) houses.push({ file: rel(f), ...h });
    }
  }
}

const bad = collisions(plots, houses);
if (bad.length) {
  console.log(bad.map((b) => `FAIL ${b}`).join("\n"));
  console.log(`validate-plot-house-ids: ${bad.length} collision(s). Rename the plot key (m_sPlotId) and the plot signs that name it (#2783).`);
  process.exit(1);
}
console.log(`validate-plot-house-ids: OK (${plots.length} plot key(s) in ${confs} conf(s), ${houses.length} house identifier(s))`);

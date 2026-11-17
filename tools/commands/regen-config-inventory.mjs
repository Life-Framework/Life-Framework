// tools\cli regen-config-inventory: rewrite docs/config-inventory.md from the Feature/ scan.
import { join } from "node:path";
import { buildInventory, writeDoc, counts } from "../config/inventory.mjs";

export const help = "regen-config-inventory   rewrite docs/config-inventory.md (sorted rows, no timestamp)";

export async function main(args, { root } = {}) {
  const r = root || process.cwd();
  const inv = buildInventory({
    addonDir: join(r, "addons", "LifeFramework"),
    featureDir: join(r, "addons", "LifeFramework", "Scripts", "Game", "Feature"),
  });
  writeDoc(inv, join(r, "docs", "config-inventory.md"));
  const n = counts(inv);
  console.log(`regen-config-inventory: docs/config-inventory.md written (${n.roots} roots, ${n.constants} constants, ${n.tunable} tunable)`);
  return 0;
}

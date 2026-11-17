// tools/buildings/general-store/slots.mjs - the general store's shelf points, written into its layer entity.
//
//   node tools/buildings/general-store/slots.mjs           rewrite the store lines of Shop_GeneralStore_Debug
//   node tools/buildings/general-store/slots.mjs --check   exit 1 when the layer differs from what the spec gives
//
// The general store is the vanilla ShopHouse_E_2I01t (pack prefab GeneralStore.et); its store is the core
// DebugWorld counter entity, not a store prefab (spec.json layer_store.why). The shelf points come from the
// building pipeline's own storePlan over the pack furniture prefab (fixtures by `pick`, boards lowest first,
// as `tools\cli building` prints them), then move into the counter's space: the counter stands at
// layer_store.counter_at in the building, both unturned, so a point is building point minus counter_at.
// Fixtures and goods fixtures.json does not hold yet are measured here from the models (tools\cli measure).
import { readFileSync, writeFileSync } from "node:fs";
import { join, dirname } from "node:path";
import { fileURLToPath } from "node:url";
import { storePlan, FIXTURES, ITEMS } from "../pipeline/store-slots.mjs";
import { boardsOf, itemOf, itemKey } from "../pipeline/fixtures.mjs";
import { layerFile } from "../../lib/addons.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const ROOT = join(HERE, "..", "..", "..");
const spec = JSON.parse(readFileSync(join(HERE, "spec.json"), "utf8"));
const ls = spec.layer_store;
const num = (n) => String(+(+n).toFixed(3));

async function measured() {
  const fixtures = { ...FIXTURES };
  const items = { ...ITEMS };
  const missingF = [...new Set(ls.lines.map((l) => l.fixture))].filter((f) => !fixtures[f]);
  const missingI = [...new Set(ls.lines.map((l) => l.item))].filter((i) => !items[itemKey(i)]);
  if (!missingF.length && !missingI.length) return { fixtures, items };
  const { measureOne } = await import("../../commands/measure.mjs");
  for (const name of missingF) fixtures[name] = { boards: boardsOf((await measureOne(ROOT, name, { minSize: 0.05 })).surfaces) };
  for (const ref of missingI) items[itemKey(ref)] = itemOf((await measureOne(ROOT, ref, { minSize: 0.05 })).bounds);
  return { fixtures, items };
}

export async function plan() {
  const addon = join(ROOT, "addons", spec.addon);
  const furniture = readFileSync(join(addon, spec.prefab_dir, `${spec.vanilla.furniture}.et`), "utf8");
  const { fixtures, items } = await measured();
  const p = storePlan({ key: ls.key, lines: ls.lines }, furniture, fixtures, items);
  // The till is the core layer's own entity (Shop_GeneralStore_Till), not a furniture child.
  const problems = p.problems.filter((x) => !/^the export placed no /.test(x));
  if (problems.length) throw new Error("store plan:\n  " + problems.join("\n  "));
  const at = ls.counter_at;
  const local = (v) => [v[0] - at[0], v[1] - at[1], v[2] - at[2]];
  return {
    lines: p.lines.map((l) => ({ ...l, slots: l.slots.map(local) })),
    center: [ls.boundary.center[0] - at[0], 0, ls.boundary.center[2] - at[2]],
    half: ls.boundary.half,
  };
}

// The m_aLines block and boundary rows of the layer entity, GUIDs of existing lines kept by item.
export function rewrite(layer, pl) {
  const start = layer.indexOf(` ${ls.entity} {`);
  if (start < 0) throw new Error(`${ls.entity} not in ${ls.layer}`);
  const storeAt = layer.indexOf("LF_StoreComponent", start);
  const linesAt = layer.indexOf("    m_aLines {\n", storeAt);
  const linesEnd = layer.indexOf("\n    }\n", linesAt) + "\n    }\n".length;
  const old = layer.slice(linesAt, linesEnd);
  const guids = new Map([...old.matchAll(/LF_StoreLine "(\{[0-9A-F]{16}\})" \{\n\s+m_ItemPrefab "([^"]+)"/g)].map((m) => [m[2], m[1]]));
  const fresh = (i) => `{7C1A5E30B92D4F${(0x10 + i).toString(16).toUpperCase().padStart(2, "0")}}`;
  const body = pl.lines.flatMap((l, i) => [
    `     LF_StoreLine "${guids.get(l.item) ?? fresh(i)}" {`,
    `      m_ItemPrefab "${l.item}"`,
    `      m_iMaxStock ${l.max}`,
    `      m_iRestockAmount ${l.restock}`,
    `      m_sSlots "${l.slots.map((s) => s.map(num).join(" ")).join(" | ")}"`,
    "     }",
  ]);
  let out = layer.slice(0, linesAt) + ["    m_aLines {", ...body, "    }"].join("\n") + "\n" + layer.slice(linesEnd);
  const tail = out.indexOf("m_iRestockSeconds", storeAt);
  const seg = out.slice(storeAt, tail);
  const seg2 = seg
    .replace(/m_vBoundaryCenter [^\n]+/, `m_vBoundaryCenter ${pl.center.map(num).join(" ")}`)
    .replace(/m_vBoundaryHalf [^\n]+/, `m_vBoundaryHalf ${pl.half.map(num).join(" ")}`);
  return out.slice(0, storeAt) + seg2 + out.slice(tail);
}

if (process.argv[1] === fileURLToPath(import.meta.url)) {
  const file = layerFile(ROOT, ls.layer);
  const layer = readFileSync(file, "utf8");
  const pl = await plan();
  const next = rewrite(layer, pl);
  for (const l of pl.lines) console.log(`  ${l.item.replace(/^.*\//, "").padEnd(28)} ${String(l.slots.length).padStart(3)} shelf point(s), max ${l.max}, restock ${l.restock}`);
  const total = pl.lines.reduce((n, l) => n + l.slots.length, 0);
  if (process.argv.includes("--check")) {
    console.log(next === layer ? `SUMMARY general-store-slots ok ${total} points` : `SUMMARY general-store-slots fail: ${ls.layer} differs from the spec; run node tools/buildings/general-store/slots.mjs`);
    process.exit(next === layer ? 0 : 1);
  }
  writeFileSync(file, next);
  console.log(`SUMMARY general-store-slots wrote ${pl.lines.length} line(s), ${total} point(s) into ${ls.entity}`);
}

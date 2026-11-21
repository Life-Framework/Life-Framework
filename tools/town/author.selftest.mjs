// tools/town/author.selftest.mjs: `town author` and `town author extract` on tools/town/fixtures/stamp/ (card #3881). No game.
//   - extract finds the one vanilla building in a grid layer (the cell fence is not it) and writes rows in its frame: offset,
//     yaw and height relative to the building entity, no grid coordinate;
//   - each row gets an attach target, the kit a `stamp` block with the building prefab, lanes from the entrance and a clear list;
//   - extracting a layer that sits in an authoring bay archives it as tools/town/kits/sources/<kit>.layer, points the kit at it and
//     releases the bay (grid free); --keep-bay and a normal grid cell leave both alone;
//   - `author --from <place>/<slot>` builds around the island building's prefab and refuses a slot that takes another kit;
//   - extract keeps each row's component config and GUID (card #3931): read from the layer row, else carried from the same row of the
//     kit already on disk (by id, a shared id suffix or the one prefab), with that kit's source files kept as `also`; --fresh skips it;
//   - `author --margin m` reaches `grid place --bay` and a bad margin is refused.
// red-proof: 2026-11-21, with extractToKit never releasing (`const release = false`) this printed "FAIL extract releases the bay of an authoring claim (freed )" and two more; restored, OK.
// red-proof (#3931, 2026-11-21): carryConfig skipped printed "FAIL the bare counter took the previous counter's config"; configOf without the guid
//   printed "FAIL the layer row's component guid is kept"; gridPlaceArgs without the margin printed "FAIL --margin reaches grid place"; restored, OK.
// red-proof: 2026-11-21, authorExtract without the height rebase printed "FAIL counter offset is building-local (got 2,0,3)"; restored, OK.
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { checkKit, entitiesOf, parseBlocks } from "./kit.mjs";
import { authorExtract, extractToKit, findBuilding, gridPlaceArgs, planAuthor } from "./author.mjs";
import { existsSync, mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { scratchDir } from "../lib/scratch.mjs";
import { CELLS_DIR, PACK_GRID_LAYERS } from "../lib/world-zones.mjs";

const HERE = dirname(fileURLToPath(import.meta.url));
const FIX = join(HERE, "fixtures", "stamp");
const LAYER = join(FIX, "Grid_stamp_fixture.layer");
const HOUSE = "{38A5F3E4578087AB}Prefabs/Structures/Houses/Town/House_Town_E_2I01/House_Town_E_2I01.et";
const fails = [];
const ok = (c, m) => {
  if (!c) fails.push(m);
};
const near = (a, b) => Math.abs(a - b) < 0.0015;

if (process.argv[1] && pathToFileURL(resolve(process.argv[1])).href === import.meta.url) {
ok(findBuilding(entitiesOf(parseBlocks(readFileSync(LAYER, "utf8")))).name === "Grid_stamp_fixture_building", "the building, not the fence, is the origin");
const kit = authorExtract(LAYER, "Shop_*", { kit: "fixture-shop", clear: ["{AAAAAAAAAAAAAAAA}Prefabs/Props/Furniture/Broom_01.et"] });
ok(checkKit(kit).length === 0, `the kit checks (${checkKit(kit).join("; ")})`);
const row = (id) => kit.rows.find((r) => r.id === id);
const c = row("counter");
ok(c && near(c.offset[0], 2) && near(c.offset[1], 0.2) && near(c.offset[2], 3) && near(c.yaw, 0), `counter offset is building-local (got ${c?.offset},${c?.yaw})`);
const s = row("shelf");
ok(s && near(s.offset[0], -3) && near(s.offset[2], 1) && near(s.yaw, 90), `shelf offset is building-local (got ${s?.offset},${s?.yaw})`);
ok(kit.rows.every((r) => !/fence|building/i.test(r.id)), "neither the fence nor the building is a row");
ok(kit.rows.every((r) => /^(floor|wall|(shelf|counter|table):[a-z0-9-]+)$/.test(r.attach)), "every row names an attach target");
ok(kit.stamp?.prefab === HOUSE && kit.building?.vanilla === HOUSE && kit.source.frame === "building-local", "the stamp block names the building prefab");
ok(kit.stamp.lanes.some((l) => l.to === "counter" && l.width === 1.2) && kit.stamp.lanes.some((l) => l.to === "shelf"), "lanes run from the entrance to the counter and each shelf");
ok(kit.stamp.clear.length === 1 && kit.stamp.clear[0].prefab && !("until" in kit.stamp.clear[0]), "the clear list names the vanilla clutter the stamp hides (no \"until\" any more)");
ok(!JSON.stringify(kit.rows).includes("1003"), "no grid coordinate in the rows");

const dirs = { placesDir: join(FIX, "places"), templatesDir: join(FIX, "templates"), kitsDir: join(FIX, "no-kits") };
const pa = planAuthor("fixture-shop", { ...dirs, from: "stampville/shop-b" });
ok(pa.prefab === HOUSE && pa.slug === "author-fixture-shop" && !pa.seed, `--from builds around the island building's prefab (got ${pa.prefab})`);
let refused = "";
try {
  planAuthor("bakery", { ...dirs, from: "stampville/shop-a" });
} catch (e) {
  refused = e.message;
}
ok(/takes kit fixture-shop/.test(refused), `a slot of another kit is refused (${refused})`);

// ---- config and component guids survive an extract (#3931)
// the Shop_Shelf entity (not Shop_Shelf_Back) gets a component block before its closing brace
const GUID_LAYER = (() => {
  const NL = String.fromCharCode(10);
  const ls = readFileSync(LAYER, "utf8").split(NL);
  const at = ls.findIndex((l) => l.includes("Shop_Shelf :"));
  const end = ls.findIndex((l, i) => i > at && l === "}");
  ls.splice(end, 0, " components {", '  LF_StoreComponent "{ABCDEF0123456789}" {', '   m_sOutletId "hardware"', "  }", " }");
  return ls.join(NL);
})();
const scratch = scratchDir("lf-author-");
const layerWithConfig = join(scratch, "Grid_stamp_fixture.layer");
writeFileSync(layerWithConfig, GUID_LAYER);
const fromLayer = authorExtract(layerWithConfig, "Shop_*", { kit: "fixture-shop" });
const shelfCfg = fromLayer.rows.find((r) => r.id === "shelf")?.config?.[0];
ok(shelfCfg?.guid === "ABCDEF0123456789" && shelfCfg.set.m_sOutletId === '"hardware"', `the layer row's component guid is kept (got ${JSON.stringify(shelfCfg)})`);

// the grid cell holds bare props; the kit on disk has the config under older ids
const till = (id, prefab, cfg) => ({ id, prefab, role: "till", verb: "on floor", offset: [0, 0, 0], yaw: 0, floor: "f0", config: cfg });
const cfgOf = (outlet) => [{ component: "LF_StoreComponent", guid: "ABCDEF0123456789", set: { m_sOutletId: `"${outlet}"` }, blocks: ["m_aLines"] }];
const bare = authorExtract(LAYER, "Shop_*", { kit: "fixture-shop" });
const counterPrefab = bare.rows.find((r) => r.id === "counter").prefab;
const prev = {
  source: { file: "tools/town/kits/sources/old.layer", also: ["tools/town/kits/sources/old-2.layer"] },
  rows: [till("old-counter", counterPrefab, cfgOf("hardware")), till("shelf", "{00000000000000AA}Prefabs/Other.et", cfgOf("shelf-outlet"))],
};
const kept = authorExtract(LAYER, "Shop_*", { kit: "fixture-shop", prev });
const kc = kept.rows.find((r) => r.id === "counter")?.config;
ok(kc?.[0]?.guid === "ABCDEF0123456789" && kc[0].set.m_sOutletId === '"hardware"', `the bare counter took the previous counter's config (got ${JSON.stringify(kc)})`);
ok(kept.rows.find((r) => r.id === "shelf")?.config?.[0]?.set.m_sOutletId === '"shelf-outlet"', "an equal id carries too");
ok(kept.carry.carried.length === 2 && kept.carry.bare.join() === "shelf-back", `the carry reports who took config and who is bare (${JSON.stringify(kept.carry)})`);
ok(kept.source.also?.join() === "tools/town/kits/sources/old.layer,tools/town/kits/sources/old-2.layer", "the previous kit's sources stay as also");
const fresh = authorExtract(LAYER, "Shop_*", { kit: "fixture-shop", prev: null });
ok(fresh.rows.every((r) => !r.config) && !fresh.source.also, "without a previous kit (--fresh) no row takes config");
ok(JSON.stringify(prev.rows[0].config) === JSON.stringify(cfgOf("hardware")), "the previous kit is not changed");

// ---- --margin reaches grid place
const plan = { slug: "author-x", prefab: HOUSE };
ok(gridPlaceArgs(plan, { margin: "1.5" }).join(" ") === `grid place author-x ${HOUSE} --pack --bay --margin 1.5`, "--margin reaches grid place");
ok(gridPlaceArgs(plan).join(" ") === `grid place author-x ${HOUSE} --pack --bay`, "no --margin leaves grid place its default");
let badMargin = "";
try {
  gridPlaceArgs(plan, { margin: "wide" });
} catch (e) {
  badMargin = e.message;
}
ok(/not a number of metres/.test(badMargin), "a bad margin is refused");
// extract releases the authoring bay it came from (the kit is the source of truth) and keeps the layer beside the kit
{
  const root = scratchDir("lf-author-bay-");
  const put = (rel, body) => {
    mkdirSync(dirname(join(root, rel)), { recursive: true });
    writeFileSync(join(root, rel), body);
  };
  const layerText = readFileSync(LAYER, "utf8");
  const claim = (slug, extra) => put(`${CELLS_DIR}/${slug}.json`, JSON.stringify({ slug, cell: "c25r0", span: [2, 2], layer: `Grid_${slug.replace(/-/g, "_")}.layer`, rows: 0, created: "2026-11-21", pack: true, ...extra }));
  const layerOf = (slug) => join(root, PACK_GRID_LAYERS, `Grid_${slug.replace(/-/g, "_")}.layer`);
  const quiet = console.log;
  const run = (slug, a) => {
    const freed = [];
    console.log = () => {};
    try {
      const code = extractToKit(root, layerOf(slug), "Shop_*", { kit: "fixture-shop", ...a }, { free: (s) => (freed.push(s), 0) });
      return { code, freed };
    } finally {
      console.log = quiet;
    }
  };
  put(`${PACK_GRID_LAYERS}/Grid_author_fixture.layer`, layerText);
  claim("author-fixture", { authoring: true });
  let r = run("author-fixture", {});
  const kitFile = join(root, "tools", "town", "kits", "fixture-shop.json");
  const kit = existsSync(kitFile) ? JSON.parse(readFileSync(kitFile, "utf8")) : null;
  ok(r.code === 0 && r.freed.join() === "author-fixture", `extract releases the bay of an authoring claim (freed ${r.freed})`);
  ok(kit?.source.file === "tools/town/kits/sources/fixture-shop.layer" && existsSync(join(root, kit.source.file)), "the kit names the archived layer as its source, and it exists");
  ok(existsSync(join(root, "tools/town/kits/sources/fixture-shop.layer")) && readFileSync(join(root, "tools/town/kits/sources/fixture-shop.layer"), "utf8").includes(layerText), "the archive keeps the whole grid layer");
  put(`${PACK_GRID_LAYERS}/Grid_author_keep.layer`, layerText);
  claim("author-keep", { authoring: true });
  r = run("author-keep", { "keep-bay": true });
  ok(r.code === 0 && r.freed.length === 0, "--keep-bay keeps the bay");
  put(`${PACK_GRID_LAYERS}/Grid_plain_cell.layer`, layerText);
  claim("plain-cell", { cell: "c3r3", span: undefined });
  r = run("plain-cell", {});
  ok(r.code === 0 && r.freed.length === 0, "a normal grid cell is never released by extract");
  put(`${PACK_GRID_LAYERS}/Grid_author_elsewhere.layer`, layerText);
  claim("author-elsewhere", { authoring: true });
  r = run("author-elsewhere", { out: join(root, "scratch-kit.json") });
  ok(r.code === 0 && r.freed.length === 0, "a kit written outside tools/town/kits (--out) does not release the bay");
}

if (fails.length) {
  for (const f of fails) console.log(`FAIL ${f}`);
  process.exit(1);
}
console.log("town author selftest: OK");
}

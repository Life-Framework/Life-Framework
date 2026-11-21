// tools/town/author.selftest.mjs: `town author` and `town author extract` on tools/town/fixtures/stamp/ (card #3881). No game.
//   - extract finds the one vanilla building in a grid layer (the cell fence is not it) and writes rows in its frame: offset,
//     yaw and height relative to the building entity, no grid coordinate;
//   - each row gets an attach target, the kit a `stamp` block with the building prefab, lanes from the entrance and a clear list;
//   - `author --from <place>/<slot>` builds around the island building's prefab and refuses a slot that takes another kit.
// red-proof: 2026-11-21, authorExtract without the height rebase printed "FAIL counter offset is building-local (got 2,0,3)"; restored, OK.
import { dirname, join, resolve } from "node:path";
import { fileURLToPath, pathToFileURL } from "node:url";
import { checkKit, entitiesOf, parseBlocks } from "./kit.mjs";
import { authorExtract, findBuilding, planAuthor } from "./author.mjs";
import { readFileSync } from "node:fs";

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
ok(kit.stamp.clear.length === 1 && kit.stamp.clear[0].until === "#3882", "the clear list keeps the vanilla clutter named");
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

if (fails.length) {
  for (const f of fails) console.log(`FAIL ${f}`);
  process.exit(1);
}
console.log("town author selftest: OK");
}

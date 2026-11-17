// tools/validation/town-kit-selftest.mjs
// inputs: tools/town/**, tools/validation/town-kit-selftest.mjs
//
// Proves `town kit extract` (tools/town/kit.mjs, card #2203) on a fixture layer and a fixture store prefab, then
// checks that the committed kits in tools/town/kits/ parse and resolve. rays: no game files, no server.
// Each block names the failure it guards.
//
// red-proof: 2026-11-16, run before kit.mjs existed and with one line broken per guard, FAIL seen, then restored:
//   (before kit.mjs existed)               -> every case failed: the import found no module
//   toLocal returning the world offset      -> FAIL round trip: rows come back metres off the fixture
//   yaw not subtracted from the origin yaw  -> FAIL yaw: the counter faces the wrong way
//   configOf copying nested blocks          -> FAIL config: the stock line lands in the kit
//   checkKit not comparing the meta GUID    -> FAIL prefab: a wrong GUID on a repo prefab passes
//   floors dropped for a prefab             -> FAIL building: a two-storey store has one floor
import { existsSync, readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";
import { applyKit, checkKit, entitiesOf, extractKit, leakedWorldVector, loadKits, parseBlocks } from "../town/kit.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..", "..");
const FX = join(ROOT, "tools", "town", "fixtures");
let failed = 0;
const ok = (c, m) => { if (!c) { failed++; console.log(`FAIL ${m}`); } };

const layer = join(FX, "kit-layer.layer");
const abs = entitiesOf(parseBlocks(readFileSync(layer, "utf8")));
const b = abs.find((e) => e.name === "Fx_Building");
const kit = extractKit(layer, "Fx_*", { kit: "fixture", origin: "Fx_Building", exclude: "Fx_Building", system: "test" });

// round trip: apply the kit back at the building and land within 2 cm of the fixture
const back = applyKit(kit, b.coords[0], b.coords[2], b.yaw, 0.2); // a layer kit is rebased to its lowest row (0.2 here)
const want = abs.filter((e) => /^Fx_(Counter|Till|Clerk|Sign|Crate_A|Crate_B|Bag)$/.test(e.name));
ok(kit.rows.length === want.length, `round trip: ${kit.rows.length} rows, fixture group has ${want.length}`);
for (const e of want) {
  const r = back.find((x) => x.prefab === e.prefab && Math.abs(x.coords[0] - e.coords[0]) < 0.02 && Math.abs(x.coords[2] - e.coords[2]) < 0.02);
  ok(r, `round trip: ${e.name} does not come back within 2 cm`);
  if (r) {
    ok(Math.abs(r.coords[1] - e.coords[1]) < 0.02, `round trip: ${e.name} height is off`);
    const dy = ((((r.yaw - e.yaw) % 360) + 540) % 360) - 180;
    ok(Math.abs(dy) < 0.02, `yaw: ${e.name} faces ${r.yaw}, fixture ${e.yaw}`);
  }
}
ok(!kit.rows.some((r) => /PolePlate/.test(r.prefab)), "group: Elsewhere_Prop is outside the Fx_* group");

// no absolute coordinate left
const numbers = [];
JSON.stringify(kit, (k, v) => { if (typeof v === "number") numbers.push(v); return v; });
const worldVals = abs.flatMap((e) => [e.coords[0], e.coords[2]]).filter((v) => Math.abs(v) > 100);
ok(!JSON.stringify(kit).includes('"coords"'), "absolute: the kit carries a coords field");
ok(kit.rows.every((r) => Math.hypot(r.offset[0], r.offset[2]) < 20), "absolute: a row sits far from the origin");
ok(!numbers.some((n) => worldVals.some((w) => Math.abs(n - w) < 0.01)), "absolute: a world coordinate is left in the kit");
ok(leakedWorldVector({ rows: [{ id: "x", config: [{ component: "C", set: { m_vPos: "500.5 0 300.25" } }] }] }, [[500.5, 0, 300.25]]).length === 1, "absolute: a world vector in a config scalar is not flagged");

// roles and verbs
const row = (n) => kit.rows.find((r) => r.name === n);
ok(row("Fx_Counter").role === "counter" && row("Fx_Till").role === "till" && row("Fx_Clerk").role === "npc" && row("Fx_Sign").role === "sign", "role: counter, till, npc, sign are read from the prefab");
ok(row("Fx_Till").verb === `on ${row("Fx_Counter").id}`, `verb: the till sits on its counter, got ${row("Fx_Till").verb}`);
ok(row("Fx_Sign").verb === "against wall", "verb: a sign is against a wall");
ok(row("Fx_Crate_B").scale === 1.2 && row("Fx_Crate_A").prefab === row("Fx_Crate_B").prefab, "group: $grp instances come through with their scale");
ok(row("Fx_Bag").children?.length === 1 && row("Fx_Bag").children[0].offset[1] === 0.15, "children: a nested entity keeps its parent-local offset");

// config is referenced, not copied
const tillCfg = row("Fx_Till").config?.find((c) => c.component === "LF_StoreComponent");
ok(tillCfg && tillCfg.set.m_sStoreKey === '"fx_store"' && tillCfg.blocks.includes("m_aLines"), "config: the store key is kept and the stock block is named");
ok(!JSON.stringify(kit).includes("Items/Food/Apple") && !JSON.stringify(kit).includes("m_iMaxStock"), "config: the stock lines are copied into the kit");

// prefabs resolve
ok(checkKit(kit).length === 0, `prefab: the fixture kit fails check: ${checkKit(kit).join("; ")}`);
const wrong = JSON.parse(JSON.stringify(kit));
wrong.rows[0].prefab = wrong.rows[0].prefab.replace(/^\{[0-9A-F]{16}\}/, "{0123456789ABCDEF}");
ok(checkKit(wrong).some((m) => /prefab/.test(m)), "prefab: a wrong GUID is not caught");
const bare = JSON.parse(JSON.stringify(kit));
bare.rows[0].prefab = "Prefabs/Props/Commercial/CounterShop_01.et";
ok(checkKit(bare).some((m) => /not \{GUID\}path/.test(m)), "prefab: a bare path is not caught");
const drug = loadKits().find((k) => k.kit.kit === "drugstore").kit;
const repoWrong = JSON.parse(JSON.stringify(drug));
const pr = repoWrong.rows.find((r) => /LF_PharmacyCounter/.test(r.prefab));
pr.prefab = pr.prefab.replace(/^{[0-9A-F]{16}}/, "{0123456789ABCDEF}");
ok(checkKit(repoWrong).some((m) => /meta GUID/.test(m)), "prefab: a wrong GUID on a repo prefab (its .meta GUID) is not caught");
const unknown = JSON.parse(JSON.stringify(kit));
unknown.rows[0].verb = "on nothing";
ok(checkKit(unknown).some((m) => /verb/.test(m)), "verb: a verb naming a missing row is not caught");

// a whole-building kit (the format a building fit-out such as the police station uses)
const store = extractKit(join(FX, "kit-store.et"), "*", { kit: "fixture-store", storeRef: "{BBBBBBBBBBBBBBBB}Prefabs/Buildings/Fixture/FixtureShop.et" });
ok(store.scope === "building" && store.building?.mode === "override" && /FixtureShop_Base/.test(store.building.vanilla), "building: scope and the vanilla building it overrides are recorded");
ok(store.anchoring.floors.length === 2 && store.rows.find((r) => /Bed_01/.test(r.prefab)).floor === "f1", `building: a two-storey store has floors ${store.anchoring.floors.map((f) => f.id)}`);
ok(store.anchoring.footprint.source === "store-boundary" && store.anchoring.footprint.min[0] === -2, "building: the footprint is the store's own boundary");
ok(store.store.config.find((c) => c.component === "LF_StoreComponent").blocks.includes("m_aLines") && !JSON.stringify(store).includes("m_sSlots"), "config: slots and stock stay in the source prefab");
const badFloor = JSON.parse(JSON.stringify(store));
badFloor.rows[0].floor = "f9";
ok(checkKit(badFloor).some((m) => /floor/.test(m)), "building: a row on an unlisted floor is not caught");

// the committed kits
const kits = loadKits();
ok(kits.length >= 8, `kits: ${kits.length} committed, the card wants 8`);
for (const name of ["general-store", "drugstore", "gun-store", "clothing-store", "bank-vault", "clinic", "fence-stall", "supermarket"]) {
  ok(kits.some((k) => k.kit.kit === name), `kits: ${name} is missing`);
}
for (const { file, kit: k } of kits) {
  const errs = checkKit(k);
  ok(errs.length === 0, `kits: ${file}: ${errs.join("; ")}`);
  ok(k.rows.every((r) => !("coords" in r)), `kits: ${file} carries coords`);
  if (k.scope === "building") ok(k.store?.prefab && existsSync(join(ROOT, k.source.file)), `kits: ${k.kit} names its store prefab`);
}

if (failed) { console.log(`town-kit-selftest: ${failed} case(s) failed`); process.exit(1); }
console.log(`town-kit-selftest: OK (fixture round trip, whole-building format, ${kits.length} committed kits)`);

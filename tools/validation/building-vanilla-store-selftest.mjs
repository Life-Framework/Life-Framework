// Proves offline the store step for a building made from a vanilla one (the gun store): a spec with a
// `vanilla` block instead of an ArchBuilder export, its prefab checks, weapon-rack slots read from the
// vanilla arsenal racks, display-only rack lines (a model, never an item), placed shop fittings, a counter
// sold over an item-catalogue outlet, the permit-counter proof steps and the harness `licence` verb.
// inputs: tools/buildings/pipeline/building.mjs, tools/buildings/pipeline/store-slots.mjs, tools/buildings/pipeline/fixtures.mjs, tools/buildings/pipeline/fixtures.json, tools/buildings/pipeline/scenario.mjs, tools/buildings/pipeline/proof.mjs, agent-box/harness.mjs, tools/validation/building-vanilla-store-selftest.mjs
// red-proof: 2026-11-15, one line broken at a time, each restored after its FAIL was seen:
//   displayPoints without the rack's yaw (angles[1] = s.angles[1])   -> FAIL display: a rack turned 90 turns its guns with it
//   storePrefab listing display lines in m_aShopItems (allow over display) -> FAIL display: a display copy is a plain model
//   runStages without the VANILLA_SKIPS branch                      -> FAIL vanilla: generate to import are not needed
//   permitSteps granting the licence before the first order          -> FAIL permit: the refused order comes before the licence
// red-proof: 2026-11-15 (folded piles):
//   pilePoints with every copy at p[1] (no k * height)              -> FAIL pile: a pile stands on its shelf point, one item height per copy
//   storePrefab without the m_iStack row                            -> FAIL pile: the prefab writes the yaw as each point's fourth number and m_iStack
import { mkdirSync, mkdtempSync, rmSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join } from "node:path";
import { pathToFileURL } from "node:url";
import { run, specProblems, vanillaPrefabProblems, VANILLA_SKIPS } from "../buildings/pipeline/building.mjs";
import { arsenalMask, rackSlotsOf, slotKind } from "../buildings/pipeline/fixtures.mjs";
import { buildingFiles, storePlan, storePrefab } from "../buildings/pipeline/store-slots.mjs";
import { expandScenario, permitProblems, scenarioProblems } from "../buildings/pipeline/scenario.mjs";
import { placedPrefabs } from "../buildings/pipeline/proof.mjs";
import { buildLine } from "../../agent-box/harness.mjs";

const BASE = "{9064E65F34097C8F}Prefabs/Structures/Commercial/Shops/ShopHouse_E_2l01/ShopHouse_E_2I01t_Base.et";
const RACK = "{12BFA4BF5A703816}Prefabs/Props/Commercial/LF_WeaponRackCabinet_Display.et";
const COUNTER = "{F7EEC900E44C3CCA}Prefabs/Props/Commercial/LF_GunCounter.et";
const TILL = "{AAAAAAAAAAAAAAAA}Prefabs/Props/Commercial/CashierShop_01.et";
const RIFLE = "{AAAAAAAAAAAAAAA1}Prefabs/Weapons/Rifles/M14/Rifle_M21.et";
const APPLE = "{C9D661E5B0714711}Prefabs/Items/Food/Apple.et";

export async function main() {
  let failed = 0;
  const ok = (cond, what) => {
    if (!cond) failed++;
    console.log(`${cond ? "ok  " : "FAIL"}  ${what}`);
  };

  // ---- spec (building.mjs)
  const spec = {
    slug: "gun-store",
    name: "GunStore",
    concept: "a village gunsmith",
    red_line: "pack",
    addon: "LF_Everon1989",
    prefab_dir: "Prefabs/Buildings/GunStore",
    vanilla: { base: BASE, building: "ShopHouse_E_2I01t_gunstore", furniture: "Furniture/ShopHouse_E_2I01t_furniture_gunstore_01" },
  };
  ok(specProblems(spec).length === 0, `vanilla: a spec with a vanilla base and no archbuilder block passes (${specProblems(spec).join("; ")})`);
  ok(specProblems({ ...spec, vanilla: { ...spec.vanilla, base: "Prefabs/Structures/X.et" } }).some((p) => /vanilla\.base/.test(p)), "vanilla: a base without its {GUID} is refused");
  ok(specProblems({ ...spec, archbuilder: { archetype: "TOWNHOUSE", seed: 1 } }).some((p) => /not both/.test(p)), "vanilla: archbuilder and vanilla together are refused");
  ok(specProblems({ ...spec, vanilla: { ...spec.vanilla, building: "x.et" } }).some((p) => /vanilla\.building/.test(p)), "vanilla: the building is a file name without .et");
  const f = buildingFiles(spec);
  ok(f.building === "Prefabs/Buildings/GunStore/ShopHouse_E_2I01t_gunstore.et" && f.furniture === "Prefabs/Buildings/GunStore/Furniture/ShopHouse_E_2I01t_furniture_gunstore_01.et" && f.base === BASE, "vanilla: the building and furniture are our files in prefab_dir, the base is the vanilla one");
  const arch = buildingFiles({ name: "Supermarket", prefab_dir: "Prefabs/Buildings/Supermarket", store: { prefab: "Supermarket_Store" } });
  ok(arch.furniture === "Prefabs/Buildings/Supermarket/Furniture/Supermarket_furniture_01.et" && arch.store === "Prefabs/Buildings/Supermarket/Supermarket_Store.et", "archbuilder: the exported files keep their paths");
  ok(placedPrefabs({ ...spec, store: { prefab: "GunStore_Store" } }).join() === "Prefabs/Buildings/GunStore/GunStore_Store.et,Prefabs/Buildings/GunStore/ShopHouse_E_2I01t_gunstore.et", "proof: the placed prefab of a vanilla-based store is found by its file");
  const door = '  GenericEntity : "{79F622DEBB73D581}Prefabs/Structures/BuildingParts/Doors/DoorSet_Village_E_01_L_EXT_COV_B_brown.et" {\n   ID "1"\n  }\n';
  ok(!vanillaPrefabProblems({ ...spec, budget: { min_doors: 1 } }, `SCR_DestructibleBuildingEntity : "${BASE}" {\n ID "2"\n {\n${door} }\n}\n`).problems.length, "vanilla: our prefab inheriting the base with an added street door passes");
  ok(vanillaPrefabProblems(spec, 'SCR_DestructibleBuildingEntity : "{0000000000000000}Prefabs/Structures/Other.et" {\n}\n').problems.some((p) => /inherits/.test(p)), "vanilla: a prefab that does not inherit vanilla.base is refused");

  // run(): no ArchBuilder on this machine, and none needed.
  const tmp = mkdtempSync(join(tmpdir(), "lf-vanilla-"));
  try {
    mkdirSync(join(tmp, "tools", "buildings", "gun-store"), { recursive: true });
    writeFileSync(join(tmp, "tools", "buildings", "gun-store", "spec.json"), JSON.stringify(spec));
    const log = console.log;
    console.log = () => {};
    let state;
    try {
      await run(tmp, "gun-store", { from: "generate", hooks: { archBuilderSetup: () => ({ ok: false, why: "no ArchBuilder (test)" }) } });
      state = JSON.parse((await import("node:fs")).readFileSync(join(tmp, "tmp", "buildings", "gun-store", "state.json"), "utf8"));
    } finally {
      console.log = log;
    }
    ok([...VANILLA_SKIPS].every((s) => state.stages[s]?.ok && state.stages[s]?.vanilla) && state.stages.prefab?.ok === false, "vanilla: generate to import are not needed (ok, no ArchBuilder asked); the prefab stage checks our files");
  } finally {
    rmSync(tmp, { recursive: true, force: true });
  }

  // ---- rack slots (fixtures.mjs), in the shape of the vanilla WeaponRackCabinetArsenal_Base.et
  const slot = (at, ang, types) => `   {\n    ID "X"\n    components {\n     Hierarchy "{5584D66370FC286B}" {\n     }\n    }\n    coords ${at}\n    angles ${ang}\n    m_eSupportedItemTypes ${types}\n    m_eSupportedItemModes 12\n   }\n`;
  const rackText = `GenericEntity {\n ID "1"\n {\n  $grp SCR_WeaponRackSlotEntity {\n${slot("0.633 0.665 0.266", "80 180 180", 306)}${slot("-1.023 1.169 0.216", "0 30 90", "PISTOL")}${slot("0.024 0.665 0.266", "80 180 180", 306)}${slot("-1.023 0.139 0.216", "0 30 90", 520)}  }\n }\n}\n`;
  const slots = rackSlotsOf(rackText);
  ok(slots.length === 4 && slots[0].kind === "long" && slots[0].at[0] === 0.024 && slots[1].at[0] === 0.633 && slots[2].kind === "pistol" && slots[3].kind === "other", `racks: slots are read from the SCR_WeaponRackSlotEntity group, long guns first, left to right (${JSON.stringify(slots.map((s) => s.kind))})`);
  ok(slots[0].angles.join() === "80,180,180", "racks: a long-gun slot keeps the vanilla upright angles");
  ok(slotKind(arsenalMask("306")) === "long" && slotKind(arsenalMask("PISTOL")) === "pistol" && slotKind(arsenalMask("520")) === "other" && slotKind(arsenalMask("RIFLE | PISTOL")) === "long", "racks: SCR_EArsenalItemType masks name the slot kind");

  // ---- store: fittings, display lines, counters (store-slots.mjs)
  const RACKS = { LF_WeaponRackCabinet_Display: { prefab: RACK, slots } };
  const FX = { ShelfShop_01: { boards: [{ y: 0.82, x: [-1.2, 1.2], z: [0.05, 0.39], clear: 0.36 }] } };
  const IT = { "Prefabs/Weapons/Rifles/M14/Rifle_M21.et": { size: [0.07, 0.19, 1.1], center: [0, 0.27], mesh: "{0123456789ABCDEF}Assets/Weapons/Rifles/M14/M21_body.xob" }, "Prefabs/Items/Food/Apple.et": { size: [0.08, 0.07, 0.08], center: [0, 0] } };
  const opts = { racks: RACKS, shopGuid: (ref) => (ref === COUNTER ? "6EA70364DEB62E72" : null), outlets: new Set(["gun_counter"]) };
  const store = {
    key: "gun_store",
    fixtures: [
      { prefab: RACK, at: [2, 0.1, 3], yaw: 90 },
      { prefab: TILL, at: [0, 1.04, -1], yaw: 180 },
    ],
    counters: [{ prefab: COUNTER, at: [0, 0.1, -1], yaw: 180, outlet: "gun_counter" }],
    lines: [{ item: RIFLE, display: true, fixture: "LF_WeaponRackCabinet_Display", pick: [0], slots: [0, 1] }],
  };
  const plan = storePlan(store, "", FX, IT, opts);
  ok(!plan.problems.length && plan.display.length === 1 && plan.display[0].points.length === 2 && plan.lines.length === 0, `display: two long-gun slots of the placed rack show the rifle, and it is no stock line (${plan.problems.join("; ")})`);
  const p0 = plan.display[0].points[0];
  ok(Math.abs(p0.at[0] - (2 + 0.266)) < 1e-3 && Math.abs(p0.at[2] - (3 - 0.024)) < 1e-3 && p0.angles[1] === -90, `display: a rack turned 90 turns its guns with it (${JSON.stringify(p0)})`);
  const text = storePrefab("gun-store", store, plan, "{1111111111111111}Prefabs/Buildings/GunStore/ShopHouse_E_2I01t_gunstore.et");
  ok(text === storePrefab("gun-store", store, storePlan(store, "", FX, IT, opts), "{1111111111111111}Prefabs/Buildings/GunStore/ShopHouse_E_2I01t_gunstore.et"), "store: the same spec writes the same prefab text");
  const allow = /m_aShopItems \{\n((?: {4}[^\n]*\n)*) {3}\}/.exec(text)?.[1] ?? "x";
  ok(!allow.includes("Rifle_M21") && !/LF_StoreLine "[^"]*" \{\n {5}m_ItemPrefab "[^"]*Rifle_M21/.test(text) && (text.match(/MeshObject "\{[0-9A-F]{16}\}" \{\n {5}Object "\{0123456789ABCDEF\}Assets\/Weapons\/Rifles\/M14\/M21_body\.xob"/g) || []).length === 2, "display: a display copy is a plain model (MeshObject only), not a shelf item and not on the till's list");
  ok(/ {2}GenericEntity : "\{12BFA4BF5A703816\}Prefabs\/Props\/Commercial\/LF_WeaponRackCabinet_Display\.et" \{\n {3}ID "[0-9A-F]{16}"\n {3}coords 2 0\.1 3\n {3}angleY 90\n/.test(text), "fittings: store.fixtures are children of the store prefab at their spot");
  ok(/ {2}GenericEntity : "\{F7EEC900E44C3CCA\}Prefabs\/Props\/Commercial\/LF_GunCounter\.et" \{\n {3}ID "[0-9A-F]{16}"\n {3}components \{\n {4}LF_ShopComponent "\{6EA70364DEB62E72\}" \{\n {5}m_sOutletId "gun_counter"\n/.test(text), "counter: the counter child overrides its own LF_ShopComponent (by GUID) with the outlet");
  ok(plan.tills.length === 1 && /LF_StoreTillAction/.test(text), "store: a till placed by store.fixtures is a till");
  ok(storePlan({ ...store, counters: [{ ...store.counters[0], outlet: "candy_counter" }] }, "", FX, IT, opts).problems.some((p) => /no item catalogue offer names the outlet candy_counter/.test(p)), "counter: an outlet with no catalogue offers is refused");
  ok(storePlan({ ...store, counters: [{ ...store.counters[0], prefab: TILL }] }, "", FX, IT, opts).problems.some((p) => /no LF_ShopComponent/.test(p)), "counter: a prefab without LF_ShopComponent cannot be a counter");
  ok(storePlan({ ...store, lines: [{ ...store.lines[0], fixture: "ShelfShop_01" }] }, "", FX, IT, opts).problems.some((p) => /no rack slots for ShelfShop_01/.test(p)), "display: a display line needs a measured rack");
  ok(storePlan({ ...store, lines: [{ ...store.lines[0], slots: [12] }] }, "", FX, IT, opts).problems.some((p) => /not slot 12/.test(p)), "display: a slot past the rack's long-gun slots is refused");
  ok(storePlan({ ...store, lines: [{ ...store.lines[0], item: APPLE }] }, "", FX, IT, opts).problems.some((p) => /no measured model/.test(p)), "display: an item with no measured model is refused");
  ok(storePlan({ ...store, fixtures: [] }, "", FX, IT, opts).problems.every((p) => !/to pay at/.test(p)), "store: a store of display lines and counters needs no till");
  const shelfFurn = 'GenericEntity : "{28411B0C79460DF8}Prefabs/Structures/Core/BuildingFurniture_base.et" {\n {\n  GenericEntity : "{B7669B7F8B7923F6}Prefabs/Props/Commercial/ShelfShop_01.et" {\n   ID "0000000000000003"\n   coords 1 0.1 1\n  }\n }\n}\n';
  ok(storePlan({ ...store, fixtures: [store.fixtures[0]], lines: [...store.lines, { item: APPLE, fixture: "ShelfShop_01", pick: [0], boards: [0] }] }, shelfFurn, FX, IT, opts).problems.some((p) => /to pay at/.test(p)), "store: self-serve lines still need a till");

  // ---- folded piles (the clothing store): line "stack": N
  const tillFix = { prefab: TILL, at: [0, 0, 0] };
  const flat = storePlan({ ...store, lines: [{ item: APPLE, fixture: "ShelfShop_01", pick: [0], boards: [0], per_board: 2, rows: 1 }], fixtures: [tillFix], counters: [] }, shelfFurn, FX, IT, opts);
  const piled = storePlan({ ...store, lines: [{ item: APPLE, fixture: "ShelfShop_01", pick: [0], boards: [0], per_board: 2, rows: 1, stack: 4 }], fixtures: [tillFix], counters: [] }, shelfFurn, FX, IT, opts);
  const pl = piled.lines[0];
  ok(!piled.problems.length && pl.slots.length === 8 && pl.max === 8 && pl.stack === 4, `pile: two shelf points in piles of four are eight points, eight in stock (${piled.problems.join("; ")})`);
  const base = flat.lines[0]?.slots[0] ?? [0, 0, 0];
  ok(pl.slots.slice(0, 4).every((p, k) => p[0] === base[0] && p[2] === base[2] && Math.abs(p[1] - (base[1] + k * 0.07)) < 0.001), "pile: a pile stands on its shelf point, one item height (0.07) per copy, bottom first");
  ok(pl.slots.every((p) => p.length === 4 && Math.abs(p[3]) <= 8) && new Set(pl.slots.map((p) => p[3])).size > 1, "pile: every copy has a twist of at most 8 degrees, and they differ");
  const piledText = storePrefab("gun-store", store, piled, "{1111111111111111}Prefabs/Buildings/GunStore/ShopHouse_E_2I01t_gunstore.et");
  ok(/m_sSlots "[^"|]+ [^"|]+ [^"|]+ -?[\d.]+ \|/.test(piledText) && /\n {5}m_iStack 4\n/.test(piledText), "pile: the prefab writes the yaw as each point's fourth number and m_iStack");
  ok(!/m_iStack/.test(storePrefab("gun-store", store, flat, "{1111111111111111}Prefabs/Buildings/GunStore/ShopHouse_E_2I01t_gunstore.et")), "pile: a line with no stack writes no m_iStack");
  ok(storePlan({ ...store, lines: [{ item: APPLE, fixture: "ShelfShop_01", pick: [0], boards: [0], stack: 6 }], fixtures: [tillFix], counters: [] }, shelfFurn, FX, IT, opts).problems.some((p) => /a pile of 6 .* over the/.test(p)), "pile: a pile taller than the air under the board above is refused");

  // ---- permit counter proof (scenario.mjs) and the harness verb
  const permit = { licence: "FIREARM_PERMIT", look: [0, 1, -1], from: [0, 0, -1.5], rows: [0, 2], items: ["Handgun_M9", "Magazine_9x19_M9_15rnd_Ball"] };
  const sc = expandScenario({ steps: [{ shot: "street", look: [0, 0, 0], from: [0, 0, -5] }, { permit }], log: [{ re: "x" }] });
  const verbs = sc.steps.map((s) => s.do?.verb ?? Object.keys(s)[0]);
  const lic = verbs.indexOf("licence");
  const pays = sc.steps.map((s, i) => (s.do?.widget === "PayButton" ? i : -1)).filter((i) => i >= 0);
  ok(pays.length === 2 && pays[0] < lic && pays[1] > lic, "permit: the refused order comes before the licence, the paid one after");
  ok(sc.steps.some((s) => s.do?.verb === "ui" && s.expect === "Not without a") && sc.steps.filter((s) => s.do?.verb === "click" && /^Item\dRow_Action$/.test(s.do.widget)).length === 4, "permit: both orders take the listed rows, and the refusal slip must name the missing licence");
  ok(sc.log.length === 5 && sc.log[0].re === "x" && sc.log.some((l) => l.re.includes("lacks NEEDS_LICENCE")) && sc.log.some((l) => l.re === "bought item=\\S*/Magazine_9x19_M9_15rnd_Ball\\.et"), "permit: the server log must show the refusal, the grant and each item bought");
  ok(!scenarioProblems(sc).length && permitProblems({ ...permit, rows: [] }).length === 1 && permitProblems({ licence: "firearm" }).length === 4, "permit: a complete permit step passes; missing rows, items or a bad licence name are named");
  ok(buildLine(7, { verb: "licence", name: "FIREARM_PERMIT" }) === "7 licence FIREARM_PERMIT", "harness: licence <NAME> is one line");
  let threw = false;
  try {
    buildLine(7, { verb: "licence", name: "firearm permit" });
  } catch {
    threw = true;
  }
  ok(threw, "harness: a licence name that is not an enum name is refused before it is sent");

  if (failed) {
    console.log(`building-vanilla-store-selftest: ${failed} FAILED`);
    return 1;
  }
  console.log("building-vanilla-store-selftest: OK");
  return 0;
}

if (process.argv[1] && import.meta.url === pathToFileURL(process.argv[1]).href) process.exit(await main());

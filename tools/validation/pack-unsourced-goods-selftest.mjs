// tools/validation/pack-unsourced-goods-selftest.mjs
//
// Proves validate-pack-unsourced-goods fails on each place a player can meet an item of a rock the island does not
// place, and passes on the same data once the rock is placed or the pack row is a removal of the core row.
// red-proof: with check() reduced to `return []` this printed "FAIL a buyer-demand row for a copper bar is
// flagged" and exited 1 (2026-11-27); restored, it passes.
// inputs: imports

import { check, miningFamilies, placedCorePrefabs, unsourcedFamilies, unsourcedMachines } from "./validate-pack-unsourced-goods.mjs";

const fails = [];
const ok = (cond, msg) => { if (!cond) fails.push(msg); };

const meta = (fam, kind, guid) => ({ file: `addons/LifeFramework/Prefabs/Resources/Mining/${fam}${kind}.et.meta`, text: `MetaFileClass {\n Name "{${guid}}Prefabs/Resources/Mining/${fam}${kind}.et"\n}\n` });
const families = miningFamilies([
  meta("Iron", "Ore", "2C571D6F7C05E095"), meta("Iron", "Nugget", "6853E2E2AE878611"), meta("Iron", "Bar", "BCD1E8CD7F975074"),
  meta("Copper", "Ore", "C7FC93FD15F0574C"), meta("Copper", "Nugget", "1881C4F2BD43E3E1"), meta("Copper", "Bar", "577A665F1662E7AD"),
  meta("Coal", "Ore", "AAAAAAAAAAAAAAA1"), meta("Coal", "Lump", "C2AE9B7E62F0369F"),
  { file: "addons/LifeFramework/Prefabs/Resources/Mining/MinableOre.et.meta", text: 'Name "{BBBBBBBBBBBBBBB1}Prefabs/Resources/Mining/MinableOre.et"\n' },
]);
ok(families.has("Copper") && families.has("Iron"), "metals with an ore and a nugget are families");
ok(!families.has("Minable") && !families.has("Coal"), "a base prefab and a nugget-less family are not metals");

const island = `GenericEntity : "{2C571D6F7C05E095}Prefabs/Resources/Mining/IronOre.et" {\n}\n`;
const unsourced = unsourcedFamilies(families, island);
ok(unsourced.has("Copper") && !unsourced.has("Iron"), "copper has no placed ore, iron has");
ok(unsourcedFamilies(families, island + `GenericEntity : "{C7FC93FD15F0574C}Prefabs/Resources/Mining/CopperOre.et" {\n}\n`).size === 0, "placing the copper ore sources the family");

const chains = `LF_ResourceMachineDef "{A1}" {\n m_sMachineId "refinery-copper"\n m_InputPrefab "{1881C4F2BD43E3E1}Prefabs/Resources/Mining/CopperNugget.et"\n}\nLF_ResourceMachineDef "{A2}" {\n m_sMachineId "smelter-iron"\n m_InputPrefab "{2C571D6F7C05E095}Prefabs/Resources/Mining/IronOre.et"\n}\n`;
const machines = unsourcedMachines(chains, unsourced);
ok(machines.has("refinery-copper") && !machines.has("smelter-iron"), "the copper refinery is an unsourced machine, the iron smelter is not");

const run = (files, placed = []) => check({ files, placed, unsourced, machines });
const buyer = (op) => `LF_BuyerDemandRow "{6C1D4F82B9A3E505}" {\n   m_sOp "${op}"\n   m_sBuyer "hardware"\n   m_sGood "{577A665F1662E7AD}Prefabs/Resources/Mining/CopperBar.et"\n  }\n`;

ok(run([{ rel: "a.conf", text: buyer("add") }]).length > 0, "a buyer-demand row for a copper bar is flagged");
ok(run([{ rel: "a.conf", text: buyer("remove") }]).length === 0, "a pack row that removes the core's copper bar row is the fix, not a leak");
ok(run([{ rel: "a.conf", text: buyer("remove") + buyer("add") }]).length > 0, "a removal beside an add row for the same good still flags the add row");
ok(run([{ rel: "market.conf", text: `LF_MarketGoodDef "{3A}" {\n   m_sOp "patch"\n   m_sGood "{577A665F1662E7AD}Prefabs/Resources/Mining/CopperBar.et"\n   m_iBase 40\n  }\n` }]).length > 0, "a market curve patch for a copper bar (the price board line) is flagged");
ok(run([{ rel: "trader.et", text: `LF_TraderItem "{59}" {\n m_ItemPrefab "{C7FC93FD15F0574C}Prefabs/Resources/Mining/CopperOre.et"\n m_ValuePerItem 8\n}\n` }]).length > 0, "a trader price for copper ore is flagged");
ok(run([{ rel: "trader.et", text: ` m_TradablePrefabs {\n  "{C7FC93FD15F0574C}Prefabs/Resources/Mining/CopperOre.et"\n }\n` }]).length > 0, "a trader's tradable list naming copper ore is flagged");
ok(run([{ rel: "loot.conf", text: `LF_LootRowDef "{9D}" {\n m_Prefab "{1881C4F2BD43E3E1}Prefabs/Resources/Mining/CopperNugget.et"\n m_iWeight 1\n}\n` }]).length > 0, "a loot row for a copper nugget is flagged");
ok(run([{ rel: "unlocks.conf", text: `LF_TradeUnlockRow "{58}" {\n m_eKind MACHINE\n m_sRef "refinery-copper"\n}\n` }]).length > 0, "an unlock row that names the copper refinery is flagged");
ok(run([{ rel: "ok.conf", text: `LF_BuyerDemandRow "{22}" {\n m_sGood "{BCD1E8CD7F975074}Prefabs/Resources/Mining/IronBar.et"\n}\n LF_TradeUnlockRow "{4}" {\n m_sRef "smelter-iron"\n}\n` }]).length === 0, "iron goods and the iron smelter pass");
ok(run([], [{ rel: "Furnace.et", text: ` m_sMachineId "furnace-copper"\n m_InputPrefab "{1881C4F2BD43E3E1}Prefabs/Resources/Mining/CopperNugget.et"\n` }]).length > 0, "a placed core prefab with a copper recipe is flagged");

const placed = placedCorePrefabs(`a : "{5DDBFA4225659FFE}Prefabs/Trader/UniversalTrader.et" {\n}\nb : "{CF27D083C01F1C36}Prefabs/Processor/LF_MineFurnace.et" {\n}\n`, (p) => p.endsWith("UniversalTrader.et"));
ok(placed.length === 1 && placed[0] === "Prefabs/Trader/UniversalTrader.et", "placedCorePrefabs lists the core prefabs a layer names, not the pack's own");

if (fails.length) {
  for (const f of fails) console.error("FAIL " + f);
  process.exit(1);
}
console.log("pack-unsourced-goods-selftest: OK");

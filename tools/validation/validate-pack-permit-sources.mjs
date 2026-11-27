// tools/validation/validate-pack-permit-sources.mjs
// inputs: addons/LF_Everon1989/Configs/Jobs/LF_TradeUnlocks_Everon1989.conf, LF_Jobs_Everon1989.conf,
//         addons/LF_Everon1989/Worlds/MainWorld/**/*.layer
//
// #5065 follow-up: the Mining 3 level card offered a "Copper seam permit" (and Mining 6 a gold one) on an island
// with no copper or gold rock. Every miner permit the flagship hands out, as a LICENSE row on the level card or as a
// licence gate in the jobs table, must have its ore placed in the island world (MainWorld layers). A permit for
// ore that is not placed is a promise nothing keeps; it comes back in the change that places the rock. The
// licence itself stays defined in the core (the bits are positional, LF_Test_LicenceBitOrder) and so does any
// save that holds it; only the pack stops offering it.
// red-proof: restoring `m_eKind LICENSE` + `m_sRef "MINER_COPPER"` on the Mining 3 row (or the MINER_COPPER gate in
// LF_Jobs_Everon1989.conf) fails with "MINER_COPPER is offered by ..., but the island places no
// Prefabs/Resources/Mining/CopperOre.et" (observed 2026-11-27: the unfixed confs fail for MINER_COPPER and MINER_GOLD).

import { existsSync, readFileSync, readdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const PACK = join(ROOT, "addons", "LF_Everon1989");

// Each miner licence and the ore prefab its permit lets a player cut. A new MINER_* licence is added here with the
// rock that earns it, or to NO_ORE when it grants no ore (a speed perk).
const ORE = {
  MINER_COAL: "Prefabs/Resources/Mining/CoalOre.et",
  MINER_IRON: "Prefabs/Resources/Mining/IronOre.et",
  MINER_COPPER: "Prefabs/Resources/Mining/CopperOre.et",
  MINER_GOLD: "Prefabs/Resources/Mining/GoldOre.et",
};
const NO_ORE = new Set(["MINER_SPEED"]);

function layers(dir, out = []) {
  if (!existsSync(dir)) return out;
  for (const e of readdirSync(dir, { withFileTypes: true })) {
    const p = join(dir, e.name);
    if (e.isDirectory()) layers(p, out);
    else if (e.name.endsWith(".layer")) out.push(p);
  }
  return out;
}

// The miner licences the pack offers: LICENSE rows on the level card and gates in the jobs table.
export function offered(tradeUnlocksText, jobsText) {
  const found = new Map();
  for (const block of tradeUnlocksText.split(/LF_TradeUnlockRow "\{/).slice(1)) {
    if (!/m_eKind\s+LICENSE\b/.test(block)) continue;
    const ref = block.match(/m_sRef "([A-Z_]+)"/);
    if (ref && ref[1].startsWith("MINER_")) found.set(ref[1], "the level card (LF_TradeUnlocks_Everon1989.conf)");
  }
  for (const block of jobsText.split(/LF_JobLicenceGateDef "\{/).slice(1)) {
    const lic = block.match(/m_sLicence "([A-Z_]+)"/);
    if (lic && lic[1].startsWith("MINER_") && !found.has(lic[1])) found.set(lic[1], "a licence gate (LF_Jobs_Everon1989.conf)");
  }
  return found;
}

export function check(offeredMap, placedText) {
  const errors = [];
  for (const [licence, where] of offeredMap) {
    if (NO_ORE.has(licence)) continue;
    const prefab = ORE[licence];
    if (!prefab) { errors.push(`${licence} is offered by ${where} but this check does not know its ore: add it to ORE (or NO_ORE) in validate-pack-permit-sources.mjs`); continue; }
    if (!placedText.includes(prefab)) errors.push(`${licence} is offered by ${where}, but the island places no ${prefab}: drop the permit row and gate, or place the rock`);
  }
  return errors;
}

if (import.meta.url === `file://${process.argv[1].replace(/\\/g, "/")}` || process.argv[1]?.endsWith("validate-pack-permit-sources.mjs")) {
  const trade = readFileSync(join(PACK, "Configs", "Jobs", "LF_TradeUnlocks_Everon1989.conf"), "utf8");
  const jobs = readFileSync(join(PACK, "Configs", "Jobs", "LF_Jobs_Everon1989.conf"), "utf8");
  const placed = layers(join(PACK, "Worlds", "MainWorld")).map((f) => readFileSync(f, "utf8")).join("\n");
  const errors = check(offered(trade, jobs), placed);
  if (errors.length) {
    for (const e of errors) console.error("FAIL " + e);
    console.error(`validate-pack-permit-sources: ${errors.length} problem(s)`);
    process.exit(1);
  }
  console.log("validate-pack-permit-sources: OK (every miner permit the pack offers has its ore placed on the island)");
}

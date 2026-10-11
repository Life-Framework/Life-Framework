// DebugWorld chain probes for the agent box.
//
// `layers` only sees an inline *Action on a named entity. These probes are the
// named entities a player can stand beside. GAPS are action hosts with no
// entity name, so `stand` cannot reach them. The static check does not boot
// the game. `chains --run` stands, looks, and uses each probe in play mode.

import { readFileSync } from "node:fs";
import { join } from "node:path";

export const REQUIRED_LAYERS = [
  "ATM.layer",
  "AppleChain.layer",
  "CementChain.layer",
  "CrimeChain.layer",
  "FarmingChain.layer",
  "GravelChain.layer",
  "HousesKeys.layer",
  "Jobs.layer",
  "LoggingChain.layer",
  "MiningChain.layer",
  "PlumChain.layer",
  "Police.layer",
  "SandChain.layer",
  "Shops.layer",
  "Survival.layer",
  "TomatoChain.layer",
  "TraderChain.layer",
];

// action: class look must list. "" records look and does not require a class.
export const PROBES = [
  { layer: "ATM.layer", name: "ATM_Debug", action: "EL_ATMAction" },
  { layer: "AppleChain.layer", name: "AppleGatherTree", action: "EL_GatherAction" },
  { layer: "AppleChain.layer", name: "AppleTradeContainer", action: "EL_OpenTraderAction" },
  { layer: "AppleChain.layer", name: "AppleProcessor", action: "EL_ProcessAction" },
  { layer: "PlumChain.layer", name: "PlumGatherTree", action: "EL_GatherAction" },
  { layer: "PlumChain.layer", name: "PlumTradeContainer", action: "EL_OpenTraderAction" },
  { layer: "TomatoChain.layer", name: "TomatoGather", action: "EL_GatherAction" },
  { layer: "TomatoChain.layer", name: "TomatoTradeContainer", action: "EL_OpenTraderAction" },
  { layer: "FarmingChain.layer", name: "TomatoCrop_Debug", action: "EL_GatherAction" },
  { layer: "CementChain.layer", name: "CementPile_Debug", action: "EL_GatherAction" },
  { layer: "GravelChain.layer", name: "GravelPile_Medium_Debug", action: "EL_GatherAction" },
  { layer: "SandChain.layer", name: "SandPile_Debug", action: "EL_GatherAction" },
  { layer: "CementChain.layer", name: "CementPlant_Debug", action: "EL_ProcessAction" },
  { layer: "GravelChain.layer", name: "GravelPile_Large_Debug", action: "EL_GatherAction" },
  { layer: "MiningChain.layer", name: "IronOre_Debug", action: "EL_GatherAction" }, // layer grants 4, enough for both furnaces
  { layer: "MiningChain.layer", name: "GoldOre_Debug", action: "EL_GatherAction" },
  { layer: "MiningChain.layer", name: "CopperOre_Debug", action: "EL_GatherAction" },
  { layer: "MiningChain.layer", name: "Furnace", action: "EL_ProcessAction" },
  { layer: "LoggingChain.layer", name: "LogPile_Debug", action: "EL_GatherAction" },
  { layer: "LoggingChain.layer", name: "Sawmill_Debug", action: "EL_ProcessAction" },
  { layer: "MiningChain.layer", name: "Pickaxe_Debug", action: "SCR_PickUpItemAction" },
  { layer: "LoggingChain.layer", name: "Axe_5ACCA165954CE408", action: "SCR_PickUpItemAction" },
  { layer: "LoggingChain.layer", name: "CuttableTree_1", action: "" },
  { layer: "CrimeChain.layer", name: "CrimeTrader_Debug", action: "EL_OpenTraderAction" },
  { layer: "CrimeChain.layer", name: "CrimeFurnace_Debug", action: "EL_ProcessAction" },
  { layer: "Jobs.layer", name: "GatherPoint_AppleTree", action: "EL_GatherAction" },
  { layer: "Shops.layer", name: "Shop_GeneralStore_Debug", action: "EL_ShopAction" },
  { layer: "Shops.layer", name: "Shop_BlackMarket_Debug", action: "EL_ShopAction" },
  { layer: "Police.layer", name: "PoliceStation_Debug", action: "EL_DutyAction" },
  { layer: "Survival.layer", name: "SurvivalPoint_Food", action: "EL_GatherAction" },
  { layer: "Survival.layer", name: "SurvivalPoint_Water", action: "EL_GatherAction" },
  { layer: "HousesKeys.layer", name: "House_Door_Locked", action: "SCR_DoorUserAction" },
  { layer: "TraderChain.layer", name: "TraderChainCounter", action: "EL_OpenTraderAction" },
];

// Crimes.layer holds the rob points. CrimeChain.layer does not.
export const EXTRA_PROBES = [
  { layer: "Crimes.layer", name: "RobberyPoint_Money", action: "EL_RobAction" },
  { layer: "Crimes.layer", name: "RobberyPoint_Weapon", action: "EL_RobWeaponAction" },
  { layer: "Crimes.layer", name: "RobberyPoint_Vehicle", action: "EL_RobVehicleAction" },
];

// Cuttable trees stay melee (axe hit), not an F-key action. The probe only stands.
export const GAPS = [];

export function allProbes() {
  return [...PROBES, ...EXTRA_PROBES];
}

// Robberies stay civilian. Duty is after them, once the debug menu has set police.
export function runOrder() {
  const robs = allProbes().filter((probe) => probe.action.startsWith("EL_Rob"));
  const rest = allProbes().filter((probe) => !probe.action.startsWith("EL_Rob"));
  const policeAt = rest.findIndex((probe) => probe.name === "PoliceStation_Debug");
  if (policeAt < 0) return rest.concat(robs);
  return rest.slice(0, policeAt).concat(robs, rest.slice(policeAt));
}

export function checkChains(root) {
  const errors = [];
  const dir = join(root, "addons", "LifeFramework", "Worlds", "DebugWorld", "DebugWorld_Layers");
  const covered = new Set([...PROBES.map((probe) => probe.layer), ...GAPS.map((gap) => gap.layer)]);
  for (const layer of REQUIRED_LAYERS) {
    if (!covered.has(layer)) errors.push(`required layer ${layer} has neither a probe nor a gap`);
  }
  for (const probe of allProbes()) {
    let text = "";
    try {
      text = readFileSync(join(dir, probe.layer), "utf8");
    } catch {
      errors.push(`missing layer file ${probe.layer}`);
      continue;
    }
    if (!text.includes(probe.name)) errors.push(`${probe.name} is not in ${probe.layer}`);
  }
  for (const gap of GAPS) {
    if (!REQUIRED_LAYERS.includes(gap.layer)) errors.push(`gap layer ${gap.layer} is not a required DebugWorld layer`);
  }
  return { ok: errors.length === 0, errors };
}

export function formatPlan() {
  const lines = ["probes:"];
  for (const probe of runOrder()) {
    lines.push(`  ${probe.layer}  ${probe.name}  ${probe.action || "(stand only)"}`);
  }
  if (GAPS.length) {
    lines.push("gaps (no entity name, stand cannot reach them):");
    for (const gap of GAPS) lines.push(`  ${gap.layer}  ${gap.detail}`);
  }
  return lines.join("\n");
}

function sees(detail, action) {
  if (!action) return true;
  if (!detail || detail === "none") return false;
  return detail.split(",").some((part) => part === action || part.startsWith(`${action}@`));
}

export function runChainProbes({ send, pause, onLine }) {
  const lines = [];
  const say = (line) => {
    lines.push(line);
    if (onLine) onLine(line);
  };

  let ping = send({ verb: "look" });
  if (ping.detail === "no_session") {
    say("no_session");
    return { code: 1, lines, summary: { noSession: true } };
  }
  if (ping.detail === "no_character" || ping.status === "error") {
    const ui = send({ verb: "ui" });
    say(`ui ${ui.status} ${ui.detail}`);
    if (ui.status === "ok" && ui.detail.includes("CivilianButton")) {
      send({ verb: "click", widget: "CivilianButton" });
      pause(800);
      send({ verb: "click", widget: "TownButton" });
      pause(4000);
    }
    ping = send({ verb: "look" });
    if (ping.detail === "no_session" || ping.detail === "no_character" || ping.status !== "ok") {
      say(`no_session ${ping.detail || ping.status}`);
      return { code: 1, lines, summary: { noSession: true } };
    }
  }

  let standFail = 0;
  let missing = 0;
  let useFail = 0;
  for (const probe of runOrder()) {
    if (probe.name === "PoliceStation_Debug") {
      const opened = send({ verb: "open", menu: "DebugMenu", mode: "show" });
      say(`prep duty menu ${opened.status} ${opened.detail}`);
      pause(400);
      const clicked = send({ verb: "click", widget: "FactionPoliceButton" });
      say(`prep duty faction ${clicked.status} ${clicked.detail}`);
      pause(600);
      send({ verb: "open", menu: "DebugMenu", mode: "hide" });
    }
    const stood = send({ verb: "stand", entityName: probe.name });
    if (stood.status !== "ok") {
      standFail += 1;
      say(`FAIL ${probe.name} stand ${stood.detail}`);
      continue;
    }
    pause(500);
    const looked = send({ verb: "look" });
    if (looked.status !== "ok") {
      standFail += 1;
      say(`FAIL ${probe.name} look ${looked.detail}`);
      continue;
    }
    if (!sees(looked.detail, probe.action)) {
      missing += 1;
      say(`FAIL ${probe.name} look=${looked.detail} missing ${probe.action}`);
      continue;
    }
    if (!probe.action) {
      say(`PASS ${probe.name} look=${looked.detail}`);
      continue;
    }
    const used = send({ verb: "use", className: probe.action, entityName: probe.name });
    if (used.status !== "ok") {
      useFail += 1;
      say(`FAIL ${probe.name} use ${probe.action} ${used.detail}`);
      continue;
    }
    say(`PASS ${probe.name} ${probe.action} use=ok look=${looked.detail}`);
  }

  for (const gap of GAPS) say(`GAP  ${gap.layer} ${gap.detail}`);
  const summary = { standFail, missing, useFail, probes: allProbes().length, noSession: false };
  say(`SUMMARY stand_fail=${standFail} missing_action=${missing} use_fail=${useFail} probes=${summary.probes}`);
  const code = standFail + missing + useFail > 0 ? 1 : 0;
  return { code, lines, summary };
}

import { readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { arrive, clientBox, newestLog, parseState, sessionDir, sessionPaths } from "../session.mjs";

// The dirty-money loop on the island, end to end (#3405, M7 beats 2 to 4 of #1025), the way a player runs it:
//   node agent-box/session.mjs up --mission island --wait
//   node agent-box/session.mjs dirty-money-island
//   node agent-box/session.mjs down
// One client: puts UNDERWORLD standing on its own account (`repgrant`, the server's dev gate decides), is handed
// moonshine (`itemgive`), finds the night-shift worker (Remi Duval, the informant) at the island's resident home
// at dawn, asks him about his work and then for the tip (the #2922 reply), and reads the private map mark off the
// server log. It then stands at the spot the tip named, waits for the buyer to appear, and sells the bottles.
// Every verdict reads `[LFDebug:Laundering]` lines of the server log, never a client guess; each is printed.
// Steps: 1 standing, 2 goods, 3 informant talk opens, 4 tip bought (fee taken, mark put), 5 buyer appears at the
// marked spot, 6 sale pays marked notes at the 1989 price. Exits non-zero when any step is missing or fails.
const MOONSHINE = "{5280F09CEB005042}Prefabs/Items/Drinks/Moonshine.et";
const BOTTLES = 3;
const PRICE = 80; // LF_BuyerPrices_Everon1989.conf
const FEE = 150; // LF_Launder_Everon1989.conf, m_iInformantFee
const ROOT = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const PACK = join(ROOT, "addons", "LF_Everon1989");
const HOME_ANCHOR = "Fit_saint_philippe_residents_anchor_door"; // fallback: the shared slot's doorstep, used only when the pack names no house for him
// Pure: the night-shift worker's home place id in the pack's residents conf, or "".
export function nightShiftHomeId(confText) {
  return /m_sId "night_shift"[\s\S]*?m_sHomePlaceId "([^"]+)"/.exec(String(confText))?.[1] ?? "";
}
// Pure: the name of the first `ResidentDoor_*` entity that follows the house marker with this place id in a layer text, or "".
export function homeDoorOf(layerText, placeId) {
  if (!placeId) return "";
  const at = String(layerText).indexOf(`m_sId "${placeId}"`);
  if (at < 0) return "";
  return /GenericEntity (ResidentDoor_\w+)/.exec(String(layerText).slice(at))?.[1] ?? "";
}
// The worker lives in his own house (the pack's timetable names its place id), not at the shared residents slot (#5047).
function homeAnchor() {
  try {
    const conf = readFileSync(join(PACK, "Configs", "Residents", "LF_Residents_Everon1989.conf"), "utf8");
    const layer = readFileSync(join(PACK, "Worlds", "MainWorld", "MainWorld_Layers", "Homes_saint-philippe_resident.layer"), "utf8");
    return homeDoorOf(layer, nightShiftHomeId(conf)) || HOME_ANCHOR;
  } catch {
    return HOME_ANCHOR;
  }
}
const TALK = "LF_DialogueTalkAction";
const INFORMANT = "Resident_night_shift"; // LF_ResidentHarness: a spawned resident by id
const SALE = "LF_BuyerSaleAction";
const HIS_GREETING = /open node=greet(_(regular|friendly|wary|refuses))?(?![a-z_])/; // LF_Dialogue_NightShift.conf: start node `greet` or a standing variant; the generic resident says greet_neutral

async function runDirtyMoneyIsland() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail.slice(0, 220)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const serverLog = () => {
    const f = newestLog(join(sessionPaths(dir).server, "logs"));
    return f ? readFileSync(f, "utf8") : "";
  };
  const results = [];
  const step = (name, ok, detail, fixBy) => {
    results.push({ name, ok });
    console.log(`${ok ? "PASS" : "FAIL"}  ${name}${detail ? `: ${detail}` : ""}${!ok && fixBy ? `   -> ${fixBy}` : ""}`);
    return ok;
  };
  const scene = () => on(1, { verb: "dialogue" }, 10000).detail;
  // Skips the NPC's line the way a player does until the replies show; returns the last scene state.
  const untilChoosing = () => {
    let state = "";
    for (let i = 0; i < 40; i++) {
      state = scene();
      if (/phase=choosing/.test(state) || state.startsWith("closed")) return state;
      if (/phase=line/.test(state) && i >= 2) on(1, { verb: "dialogue", arg: "skip" }, 10000);
      wait(700);
    }
    return state;
  };
  // Every [LFDebug:Laundering] line the server wrote since `from`, printed once.
  const laundering = (from) => {
    const lines = serverLog().slice(from).split(/\r?\n/).filter((l) => l.includes("[LFDebug:Laundering]"));
    for (const l of lines) console.log(`  server  ${l.replace(/^.*\[LFDebug:Laundering\]/, "[LFDebug:Laundering]")}`);
    return lines.join("\n");
  };

  arrive(on, 1, wait);
  wait(6000);
  on(1, { verb: "resetplayer", cash: 1000 }, 10000);
  wait(1500);
  const logStart = serverLog().length;

  // 1. Underworld standing: the informant talks only to tier 1 and up.
  on(1, { verb: "repgrant", words: ["UNDERWORLD", "100"] }, 10000);
  wait(1500);
  const grant = /harness standing (\S+) UNDERWORLD points=(\d+) tier=(-?\d+)/.exec(serverLog().slice(logStart));
  const uid = grant ? grant[1] : "";
  step("1 the character holds underworld standing (tier 1)", !!grant && Number(grant[3]) >= 1, grant ? `points=${grant[2]} tier=${grant[3]}` : "no `harness standing` line in the server log", "the harness verb or the dev gate");
  if (!uid) return finish(results);

  // 2. The goods: bottles the buyer's price list names.
  let given = 0;
  for (let i = 0; i < BOTTLES; i++) if (on(1, { verb: "itemgive", prefab: MOONSHINE }, 10000).status === "ok") given++;
  step("2 the character carries moonshine to sell", given === BOTTLES, `${given}/${BOTTLES} bottles`, "the itemgive verb");

  // 3. The informant: the night-shift worker is at his doorstep from 05:30. The clock is set to dawn so the resident
  // spawner and timetable put him there; `Resident_night_shift` (#3650) addresses the spawned resident by id, so the
  // stand and the talk reach him and never a neighbour. He spawns only with a player near his anchor, so the
  // player first stands at the doorstep, then beside him.
  on(1, { verb: "time", hour: 6 }, 15000);
  on(1, { verb: "stand", entityName: homeAnchor(), side: "e" }, 20000);
  let stand = { status: "none", detail: "" };
  let opened = "";
  for (let tries = 0; tries < 12 && !HIS_GREETING.test(opened); tries++) {
    wait(5000);
    stand = on(1, { verb: "stand", entityName: INFORMANT }, 20000);
    if (stand.status !== "ok") continue;
    wait(1500);
    on(1, { verb: "use", className: TALK, entityName: INFORMANT }, 10000);
    wait(900);
    opened = scene();
    if (!HIS_GREETING.test(opened)) {
      on(1, { verb: "input", action: "MenuBack" });
      wait(1500);
    }
  }
  const greetOk = step("3 the night-shift worker answers the talk at the home doorstep", stand.status === "ok" && HIS_GREETING.test(opened), `stand=${stand.status} ${stand.detail.slice(0, 80)} scene=${opened}`, "the Resident_<id> harness target (#3650): he is not live (not due, or no player near), or his greeting nodes changed");
  if (!greetOk) { laundering(logStart); return finish(results); }
  let state = untilChoosing();
  // greet rows: zone, heard anything, work; bye is the 0 Goodbye row and not counted in `replies=` (a wary greeting offers none: the standing was not enough).
  if (!/replies=3/.test(state)) { step("3b the greeting offers the work reply", false, state, "the greeting gate (UNDERWORLD or TOWN standing) or a different resident answered"); laundering(logStart); return finish(results); }
  on(1, { verb: "dialogue", arg: "pick", value: "2" }, 10000);
  wait(1200);
  state = untilChoosing();
  const sawTip = /node=work/.test(state) && /replies=3/.test(state);
  step("3b after the work reply the informant's tip reply is offered", sawTip, state, "#2922 (the Informant reply gated by the hook)");
  if (!sawTip) { laundering(logStart); return finish(results); }

  // 4. Buy the tip: 150 cash, a private mark on today's spot.
  const cashBefore = Number(parseState(on(1, { verb: "state" }, 10000).detail).cash);
  const beforeTip = serverLog().length;
  on(1, { verb: "dialogue", arg: "pick", value: "2" }, 10000);
  wait(3500);
  const tipLog = laundering(beforeTip);
  const tip = new RegExp(`informant tip for ${uid}: spot (\\S+) marked until period (\\d+) ends, fee (\\d+)`).exec(tipLog);
  const cashAfter = Number(parseState(on(1, { verb: "state" }, 10000).detail).cash);
  step("4 the informant sells the tip and puts a private mark on today's spot", !!tip && Number(tip[3]) === FEE && cashBefore - cashAfter === FEE, tip ? `spot ${tip[1]}, fee ${tip[3]}, cash ${cashBefore} -> ${cashAfter}` : `no 'informant tip for ${uid}' paid line (${(/informant tip[^\n]*/.exec(tipLog) ?? ["none logged"])[0]})`, "#2922 / #2921 (no buyer spot placed, or standing below tier)");
  on(1, { verb: "input", action: "MenuBack" });
  if (!tip) return finish(results);
  const spot = tip[1];

  // 5. The buyer appears where the mark is.
  const beforeBuyer = serverLog().length;
  const atSpot = on(1, { verb: "stand", entityName: spot, side: "e" }, 20000);
  let appeared = false;
  for (let i = 0; i < 8 && !appeared; i++) {
    wait(3000);
    appeared = new RegExp(`illicit buyer appeared at spot '${spot}'`).test(serverLog().slice(beforeBuyer));
  }
  laundering(beforeBuyer);
  step("5 the buyer appears at the marked spot", atSpot.status === "ok" && appeared, atSpot.status === "ok" ? (appeared ? `spot ${spot}` : `no 'illicit buyer appeared at spot ${spot}' line in 24 s`) : `stand returned ${atSpot.status} ${atSpot.detail}`, "#2919 (buyer sweep) or the spot entity name");
  if (!appeared) return finish(results);

  // 6. The sale: three bottles at the 1989 price, paid in marked notes, none in cash.
  const cashPre = Number(parseState(on(1, { verb: "state" }, 10000).detail).cash);
  const beforeSale = serverLog().length;
  const sold = on(1, { verb: "use", className: SALE, entityName: "" }, 10000);
  wait(3000);
  const saleLog = laundering(beforeSale);
  const sale = new RegExp(`buyer sale by \\S+: (\\d+) line\\(s\\) for (\\d+) \\((marked notes|cash)\\)`).exec(saleLog);
  const cashPost = Number(parseState(on(1, { verb: "state" }, 10000).detail).cash);
  const want = BOTTLES * PRICE;
  step("6 the sale pays the 1989 price in marked notes, not cash", sold.status === "ok" && !!sale && Number(sale[2]) === want && sale[3] === "marked notes" && cashPost === cashPre, sale ? `${sale[2]} in ${sale[3]} (want ${want}), cash ${cashPre} -> ${cashPost}` : `no 'buyer sale by' line (${(/buyer sale[^\n]*/.exec(saleLog) ?? ["none logged"])[0]}; use returned ${sold.status} ${sold.detail})`, "#2922 / #2920 (pack overlays, dirty-money switch, price row)");
  return finish(results);
}

function selftest() {
  const conf = ["LF_ResidentDef \"{1}\" {", " m_sId \"gossip_odile\"", " m_sHomePlaceId \"saint-philippe.home.other\"", "}", "LF_ResidentDef \"{2}\" {", " m_sId \"night_shift\"", " m_sHomeKind \"House\"", " m_sHomePlaceId \"saint-philippe.home.night_shift\"", "}"].join("\n");
  const homes = ["x m_sId \"saint-philippe.home.other\"", "GenericEntity ResidentDoor_a {", "}", "x m_sId \"saint-philippe.home.night_shift\"", "GenericEntity ResidentPlate_b : \"{1}p\" {", "}", "GenericEntity ResidentDoor_b {", "}"].join("\n");
  const ok = nightShiftHomeId(conf) === "saint-philippe.home.night_shift" && homeDoorOf(homes, "saint-philippe.home.night_shift") === "ResidentDoor_b" && homeDoorOf(homes, "nope") === "" && nightShiftHomeId("") === "";
  console.log(ok ? "dirty-money-island selftest: OK" : "dirty-money-island selftest: FAIL");
  return ok ? 0 : 1;
}

function finish(results) {
  const failed = results.filter((r) => !r.ok);
  console.log(`dirty-money-island: ${failed.length ? `FAIL (${failed.map((r) => r.name.split(" ")[0]).join(", ")})` : "PASS"} - ${results.length - failed.length}/${results.length} steps`);
  return failed.length || results.length < 7 ? 1 : 0;
}

export default {
  name: "dirty-money-island",
  usage: "dirty-money-island   (up --mission island)",
  run: (ctx) => ((ctx?.args ?? process.argv.slice(2)).includes("--selftest") ? selftest() : runDirtyMoneyIsland()),
};

import { readFileSync } from "node:fs";
import { join } from "node:path";
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
const HOME_ANCHOR = "Fit_saint_philippe_residents_anchor_door"; // the night-shift worker's doorstep (05:30)
const TALK = "LF_DialogueTalkAction";
const SALE = "LF_BuyerSaleAction";
const HIS_GREETING = /open node=greet_(regular|friendly|wary|refuses)/; // LF_Dialogue_NightShift.conf; the generic resident says greet_neutral

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

  // 3. The informant: the night-shift worker is at his doorstep from 05:30. The player stands there and the clock is
  // set to dawn so the resident spawner and timetable put him on the step; the talk takes the nearest speaker.
  on(1, { verb: "time", hour: 6 }, 15000);
  // He appears on the bench, then walks to the door within 25 s; the generic residents answer a talk too, so a
  // reply that is not one of his own greeting nodes is closed and the next side of the doorstep is tried.
  wait(35000);
  let stand = { status: "none" };
  let opened = "";
  for (let tries = 0; tries < 8 && !HIS_GREETING.test(opened); tries++) {
    stand = on(1, { verb: "stand", entityName: HOME_ANCHOR, side: ["e", "w", "n", "s"][tries % 4] }, 20000);
    wait(4000);
    on(1, { verb: "use", className: TALK, entityName: "" }, 10000);
    wait(900);
    opened = scene();
    if (!HIS_GREETING.test(opened)) {
      on(1, { verb: "input", action: "MenuBack" });
      wait(1500);
    }
  }
  const greetOk = step("3 the night-shift worker answers the talk at the home doorstep", stand.status === "ok" && HIS_GREETING.test(opened), `stand=${stand.status} scene=${opened}`, "the harness cannot address a spawned resident by id: `use LF_DialogueTalkAction` reaches the nearest resident (a neighbour, greet_neutral), not night_shift (follow-up card: a harness verb to stand beside or talk to a resident by id)");
  if (!greetOk) { laundering(logStart); return finish(results); }
  let state = untilChoosing();
  // greet rows: zone, move, work, bye (a 'wary' greeting offers bye only: the standing was not enough).
  if (!/replies=4/.test(state)) { step("3b the greeting offers the work reply", false, state, "the greeting gate (UNDERWORLD or TOWN standing) or a different resident answered"); laundering(logStart); return finish(results); }
  on(1, { verb: "dialogue", arg: "pick", value: "2" }, 10000);
  wait(1200);
  state = untilChoosing();
  const sawTip = /node=work/.test(state) && /replies=4/.test(state);
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

function finish(results) {
  const failed = results.filter((r) => !r.ok);
  console.log(`dirty-money-island: ${failed.length ? `FAIL (${failed.map((r) => r.name.split(" ")[0]).join(", ")})` : "PASS"} - ${results.length - failed.length}/${results.length} steps`);
  return failed.length || results.length < 7 ? 1 : 0;
}

export default {
  name: "dirty-money-island",
  usage: "dirty-money-island   (up --mission island)",
  run: () => runDirtyMoneyIsland(),
};

import { execFileSync } from "node:child_process";
import { existsSync, readdirSync, readFileSync, statSync } from "node:fs";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { arrive, clientBox, parseState, sessionDir } from "../session.mjs";

// The lumber hour, played the way a player plays it (#4833). On a running island session
// (`tools\cli session up --mission island --profile full --wait`, one client):
//   0 the hour-3 checkpoint state: a LUMBERJACK with cash for the Standard axe, the clock at 09:00
//   1 buy the Standard axe at the miners' till of the Saint-Philippe hardware store ($600)
//   2 take it in hand, walk to the woodlot and fell a softwood by the real hold: `fellhold` reads the "Fell tree" row the
//     player sees and completes the hold (the F key's path: the server runs LF_FellAction.TryFell); the log line is
//     `[LFDebug:Lumber] felled <key> (N logs, SOFTWOOD)`
//   3 pick the logs up, feed them to the woodlot sawmill (LF_ProcessAction), pick the planks up
//   4 sell the planks at the hardware store
//   5 print the minutes and the dollars, and the measured $/h beside `tools\cli economy`'s hand-haul planks row
// There is no checkpoint save yet (the X cards cut one); the hour-3 state is `resetplayer`, `levelset LUMBERJACK` and `time`.
// Each step is PASS or FAIL with the server line it read.
const TILL = "Fit_saint_philippe_hardware_hardware_miners_till";
const SHOP = "Fit_saint_philippe_hardware_hardware_shop_till";
const DESK = "Fit_saint_philippe_woodlot_desk";
const SAW = "Fit_saint_philippe_woodlot_saw";
const AXE_PRICE = 600;
const CHECKPOINT_CASH = 700;
const CHECKPOINT_LEVEL = Number(process.env.CHECKPOINT_LEVEL ?? "3");
const SAW_CHOICE = process.env.SAW_CHOICE ?? "LF_ProcessAction";
const FELL_TRIES = 4;

const root = dirname(fileURLToPath(import.meta.url));

// The newest server console log of this session.
function serverLog() {
  const dir = join(sessionDir(), "server", "logs");
  if (!existsSync(dir)) return "";
  const runs = readdirSync(dir).filter((d) => existsSync(join(dir, d, "console.log")));
  runs.sort((a, b) => statSync(join(dir, a, "console.log")).mtimeMs - statSync(join(dir, b, "console.log")).mtimeMs);
  return runs.length ? readFileSync(join(dir, runs[runs.length - 1], "console.log"), "utf8") : "";
}

// `tools\cli economy`'s hand-haul planks row: "steady $398/h, first hour $478/h".
export function parseEconomy(text) {
  const m = /LOOP hand-haul softwood -> planks:[^\n]*?steady \$(\d+)\/h, first hour \$(\d+)\/h/.exec(String(text));
  return m ? { steady: Number(m[1]), first: Number(m[2]) } : null;
}

// `Lumber] felled <key> (N logs, SOFTWOOD)` -> { key, logs, wood }.
export function parseFelled(text) {
  const all = [...String(text).matchAll(/\[LFDebug:Lumber\] felled (\S+) \((\d+) logs?, (\w+)\)/g)];
  const m = all[all.length - 1];
  return m ? { key: m[1], logs: Number(m[2]), wood: m[3] } : null;
}

export function dollarsPerHour(dollars, minutes) {
  return minutes > 0 ? Math.round((dollars * 60) / minutes) : 0;
}

function selftest() {
  const fails = [];
  const e = parseEconomy("  LOOP hand-haul softwood -> planks: ~17.7 logs/h, steady $398/h, first hour $478/h; duty police");
  if (!e || e.steady !== 398 || e.first !== 478) fails.push("economy row");
  if (parseEconomy("LOOP something else")) fails.push("economy refuses junk");
  const f = parseFelled("x\n[LFDebug:Lumber] felled 1:2:prefabs/t.et (3 logs, SOFTWOOD)\n");
  if (!f || f.logs !== 3 || f.wood !== "SOFTWOOD") fails.push("felled line");
  if (parseFelled("tree fall: 1 of 1 logs dropped")) fails.push("felled refuses junk");
  if (dollarsPerHour(100, 15) !== 400 || dollarsPerHour(5, 0) !== 0) fails.push("dollars per hour");
  for (const x of fails) console.log(`FAIL  ${x}`);
  console.log(fails.length ? "lumber-arc selftest: FAIL" : "lumber-arc selftest: PASS");
  return fails.length ? 1 : 0;
}

async function run(args) {
  if (args.includes("--selftest")) return selftest();
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const on = (spec, timeout) => {
    h.setBoxes([clientBox(dir, 1)]);
    const r = h.sendCommand(spec, timeout ?? 10000);
    console.log(`client 1: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${String(r.detail ?? "").slice(0, 300)}`);
    return r;
  };
  const results = [];
  const step = (name, ok, detail) => {
    results.push({ name, ok });
    console.log(`${ok ? "PASS" : "FAIL"}  ${name}${detail ? `: ${detail}` : ""}`);
    return ok;
  };
  const cash = () => Number(parseState(on({ verb: "state" }).detail).cash || 0);
  const carried = (item) => {
    const r = on({ verb: "carries", words: [item] });
    const m = /count=(\d+)/.exec(r.detail);
    return r.status === "ok" && m ? Number(m[1]) : 0;
  };
  const minutes = (since) => (Date.now() - since) / 60000;

  arrive((n, spec, t) => on(spec, t), 1, wait);
  wait(6000);

  // 0. The hour-3 checkpoint state.
  for (let i = 0; i < 4; i++) {
    const reset = on({ verb: "resetplayer", cash: CHECKPOINT_CASH }, 15000);
    if (reset.status === "ok") break;
    wait(3000);
  }
  // The Standard axe is a rank-3 item at the miners' till (the hour-3 standing): `levelset` skips the daily XP cap.
  on({ verb: "levelset", words: ["LUMBERJACK", String(CHECKPOINT_LEVEL)] }, 10000);
  wait(1500);
  on({ verb: "guide", words: ["close"] });
  on({ verb: "time", hour: 9.0 }, 15000);
  wait(1500);
  if (parseState(on({ verb: "state" }).detail).job !== "LUMBERJACK") {
    on({ verb: "stand", entityName: DESK });
    wait(1500);
    on({ verb: "use", className: "LF_WorkplaceJobAction", entityName: DESK });
    wait(1500);
  }
  const cash0 = cash();
  step("0 the hour-3 checkpoint: a lumberjack with the axe money", cash0 >= AXE_PRICE, `cash=$${cash0}, job=${parseState(on({ verb: "state" }).detail).job}`);

  // 1. The Standard axe at the miners' till.
  const arcStart = Date.now();
  on({ verb: "stand", entityName: TILL });
  wait(1500);
  const open = on({ verb: "use", className: "LF_ShopAction", entityName: TILL });
  wait(1500);
  // The sheet lists the rows in catalogue order; find the Standard axe by its name, not by a counted index.
  const sheet = String(on({ verb: "ui", mode: "text" }).detail ?? "");
  const axeRow = /line:Item(\d+)Row=Standard axe/.exec(sheet)?.[1];
  if (axeRow === undefined) console.log("the till sheet lists no Standard axe");
  on({ verb: "click", widget: `Item${axeRow ?? 4}Row_Action` });
  wait(800);
  on({ verb: "click", widget: "PayButton" });
  wait(2500);
  on({ verb: "click", widget: "CloseButton" });
  wait(800);
  on({ verb: "guide", words: ["close"] });
  const cash1 = cash();
  step("1 the Standard axe is bought at the miners' till", open.status === "ok" && cash0 - cash1 === AXE_PRICE && carried("LF_Tool_Axe_Standard") > 0, `till ${open.status}, cash $${cash0} -> $${cash1}`);

  // 2. The real hold on a softwood at the woodlot.
  const found = on({ verb: "treefall", words: ["find", "120"] }, 20000);
  const m = /pos=(-?[\d.e+-]+) (-?[\d.e+-]+) (-?[\d.e+-]+)/.exec(String(found.detail));
  step("2a a standing tree is found for the felling hold", found.status === "ok" && !!m, String(found.detail).slice(0, 120));
  if (!m) return finish(results);
  const [tx, ty, tz] = [Number(m[1]), Number(m[2]), Number(m[3])];
  // A world capture puts the player 2 m from the trunk, the way a walk-up would; it holsters, so draw the axe after it.
  on({ verb: "capture", mode: "world", name: "lumber_arc_tree", x: tx, y: ty, z: tz, ox: -2, oy: 1.6, oz: 0, hour: 9 }, 30000);
  wait(2000);
  const drew = on({ verb: "hold", item: "weapon" });
  step("2b the Standard axe is in hand", drew.status === "ok" && /Axe_Standard/.test(drew.detail), drew.detail);
  wait(2500);
  const logMark = serverLog().length;
  let aim = on({ verb: "fellhold", words: ["aim"] });
  for (let i = 0; i < 5 && aim.status !== "ok"; i++) {
    // The hands read empty while the draw animation or a camera reset is still running: draw again and look at what is carried.
    wait(1500);
    if (/right=none/.test(String(aim.detail))) {
      on({ verb: "carries", words: ["LF_Tool_Axe"] });
      on({ verb: "hold", item: "weapon" });
      wait(2500);
    }
    aim = on({ verb: "fellhold", words: ["aim"] });
  }
  step("2c the felling prompt is on the tree: the row reads Fell tree, shown and performable", aim.status === "ok" && /label=Fell tree/.test(aim.detail) && /shown=1 perform=1/.test(aim.detail), aim.detail);
  const chop = Number(/\((\d+) s\)/.exec(String(aim.detail))?.[1] ?? 0);
  const fellStart = Date.now();
  // The hold lasts the chop seconds on a player's screen; the harness waits them out, then completes it.
  wait(Math.min(chop, 60) * 1000);
  const hold = on({ verb: "fellhold", words: ["hold"] });
  let felled = null;
  for (let i = 0; i < 10 && !felled; i++) {
    wait(1000);
    felled = parseFelled(serverLog().slice(logMark));
  }
  step("2d the hold fells the tree (the server runs TryFell)", hold.status === "ok" && !!felled, felled ? `${felled.key} (${felled.logs} logs, ${felled.wood})` : `no \`Lumber] felled\` line; hold ${hold.status} ${hold.detail}`);
  if (!felled) return finish(results);
  step("2e a softwood", felled.wood === "SOFTWOOD", felled.wood);

  // 3. The logs to the sawmill.
  let logs = 0;
  for (let i = 0; i < 6 && logs < felled.logs; i++) {
    wait(2500);
    on({ verb: "pickup", item: "WoodLog" }, 15000);
    wait(900);
    logs = carried("WoodLog");
  }
  step("3a the logs are picked up", logs >= 1, `carried WoodLog=${logs} of ${felled.logs}`);
  on({ verb: "stand", entityName: SAW });
  wait(2000);
  const sawMark = serverLog().length;
  const saw = on({ verb: "use", className: SAW_CHOICE, entityName: SAW });
  wait(1500);
  step("3b the sawmill takes a log", saw.status === "ok", `${saw.status} ${saw.detail}`);
  let sawn = "";
  for (let i = 0; i < 40 && !sawn; i++) {
    sawn = /\[LFDebug:Processing\][^\n]*(?:output|produced|dropped)[^\n]*/i.exec(serverLog().slice(sawMark))?.[0] ?? "";
    if (!sawn) wait(3000);
  }
  step("3c the sawmill's run ends with its output line", !!sawn, sawn.slice(0, 160) || "no Processing output line");
  let planks = 0;
  for (let i = 0; i < 4 && planks < 1; i++) {
    on({ verb: "pickup", item: "WoodPlank" }, 15000);
    wait(900);
    planks = carried("WoodPlank");
  }
  step("3d the planks are picked up", planks >= 1, `carried WoodPlank=${planks}`);

  // 4. The sale at the hardware store.
  on({ verb: "stand", entityName: SHOP });
  wait(2000);
  const cashBefore = cash();
  console.log(String(on({ verb: "look" }).detail ?? "").slice(0, 400));
  const sell = on({ verb: "use", className: "LF_TraderSellAction", entityName: SHOP });
  wait(2500);
  const cashAfter = cash();
  const earned = cashAfter - cashBefore;
  step("4 the planks are sold at the hardware store", sell.status === "ok" && earned > 0, `${sell.status} ${sell.detail}; cash $${cashBefore} -> $${cashAfter} (+$${earned})`);

  // 5. Minutes and dollars.
  const arcMin = minutes(arcStart);
  const loopMin = minutes(fellStart);
  let econ = null;
  try {
    econ = parseEconomy(execFileSync("node", [join(root, "../../tools/cli.mjs"), "economy"], { encoding: "utf8", timeout: 120000 }));
  } catch (e) {
    console.log(`economy: ${e.message.split("\n")[0]}`);
  }
  const perHour = dollarsPerHour(earned, loopMin);
  console.log(`ARC  whole arc ${arcMin.toFixed(1)} min wall clock (axe shopping included); fell, haul, saw and sell ${loopMin.toFixed(1)} min`);
  console.log(`ARC  axe -$${AXE_PRICE}; planks sold +$${earned} from ${felled.logs} log(s); measured $${perHour}/h over the loop, $${dollarsPerHour(earned - AXE_PRICE, arcMin)}/h over the arc with the axe`);
  console.log(`ARC  tools\\cli economy hand-haul planks: ${econ ? `steady $${econ.steady}/h, first hour $${econ.first}/h` : "row not read"} (one tree is one cycle: the measured figure carries the purse refill only)`);
  step("5 the arc's minutes and dollars are measured", earned > 0 && loopMin > 0, `${loopMin.toFixed(1)} min, +$${earned}, $${perHour}/h`);
  return finish(results);
}

function finish(results) {
  const failed = results.filter((r) => !r.ok);
  console.log(failed.length ? `lumber-arc: FAIL (${failed.length} of ${results.length}: ${failed.map((r) => r.name).join("; ")})` : `lumber-arc: PASS (${results.length} of ${results.length})`);
  return failed.length ? 1 : 0;
}

export default {
  name: "lumber-arc",
  usage: "lumber-arc | --selftest   (session up --mission island --profile full --wait)",
  run: (ctx) => run(ctx.args ?? []),
};

import { spawnSync } from "node:child_process";
import { existsSync, mkdirSync, readFileSync, readdirSync, statSync } from "node:fs";
import { join } from "node:path";
import { ROOT, arrive, clientBox, newestLog, parseState, readSession, sessionDir, sessionPaths } from "../session.mjs";

// The truck licence check at the north-east compound, end to end (#4697), one client on the island:
//   node agent-box/session.mjs up --mission island --profile full --wait
//   node agent-box/session.mjs truck-check-proof
//   node agent-box/session.mjs down
// The harness cannot drive, so the `truckcheck` verb (LF_TruckCheckHarness, harness builds only) moves the lent
// truck through the gates in short steps; the gates test its path between server ticks as they do for a driven truck.
//  1. Cash: `resetplayer cash` then the island ATM (deposit all, withdraw $500 four times) leaves bank money for
//     the truck and $2,000 in the pocket for the driving licence and the check fees; `truckcheck licence` buys the
//     driving licence through the licence desk's server entry (the lot asks for it as well as the truck licence).
//  2. No truck licence yet: the truck yard's lot refuses the buyer (server log, no cash moves).
//  3. Failure path: take the check ($300 gone, the Ural lent), drive gates 0 and 1, then back into gate 0:
//     a gate out of order fails the run and the truck is removed.
//  4. After the retake wait: take it again, drive gates 0, 1, 2, 3 one at a time (a picture after each):
//     GATE_OK three times, PASSED, "holds the truck licence" in the server log, the truck removed.
//  5. The lot now sells: buy the first truck on the sheet. The price is read from the catalogue, never typed in: the
//     pack's price rows for the truck yard times the owner's vehicles.buyPercent (the lot charges base x buyPercent
//     / 100: the flagship's 80 made the Cargo truck $9,600 and the Ural $7,600 while the proof pinned $9,500).
const ATM = "Fit_saint_philippe_atms_drop_b79";
const LOT = "Fit_ne_compound_truckyard_lot";
const EXAMINER = "Fit_ne_compound_truckyard_examiner";
const START = "Fit_ne_compound_truckyard_check_start";
const TAKE_ACTION = "LF_TruckCheckExaminerAction";
const BUY_ACTION = "LF_VehicleBuyAction";
const ATM_ROW = "LF_ATMQuickAction";
const FEE = 300;
const PRICES_CONF = join("addons", "LF_Everon1989", "Configs", "Vehicles", "LF_VehiclePrices_Everon1989.conf");
const OWNER_CONFIG = join("server", "flagship", "config.json");
const YARD = "truck-yard";

// What the truck yard charges for each of its rows: every price entry in the pack's conf whose m_sLotId is the yard,
// base price x buyPercent / 100 rounded as the economy report does. [{ prefab, price }] in file order.
export function yardPrices(confText, buyPercent) {
  const out = [];
  for (const block of String(confText).split(/LF_VehiclePriceEntry\s+"\{[0-9A-Fa-f]+\}"\s*\{/).slice(1)) {
    if (!new RegExp(`m_sLotId\\s+"${YARD}"`).test(block)) continue;
    const base = /m_iBasePrice\s+(\d+)/.exec(block);
    const prefab = /m_Prefab\s+"\{[0-9A-Fa-f]+\}([^"]+)"/.exec(block);
    if (base && prefab) out.push({ prefab: prefab[1], price: Math.round(Number(base[1]) * buyPercent / 100) });
  }
  return out;
}

// The owner config's vehicles.buyPercent; 100 when the file or the key is missing.
export function buyPercentOf(ownerJsonText) {
  try {
    const n = Number(JSON.parse(ownerJsonText)?.vehicles?.buyPercent);
    return Number.isFinite(n) && n > 0 ? n : 100;
  } catch {
    return 100;
  }
}

// `sessionOwnerConfig` is the owner config the session's server actually loaded (server/profile/LifeFramework/config.json,
// written by a proof such as booking-desk-proof that stages the flagship config). A fresh session has none and charges the
// base price (100 percent): reading the repo's flagship file instead priced the truck at 80 percent and failed the price
// check on a bare session. Without a session (the selftest) the flagship file is the reference.
function readYardPrices(sessionOwnerConfig) {
  const conf = existsSync(join(ROOT, PRICES_CONF)) ? readFileSync(join(ROOT, PRICES_CONF), "utf8") : "";
  const ownerFile = sessionOwnerConfig === undefined ? join(ROOT, OWNER_CONFIG) : sessionOwnerConfig;
  const owner = existsSync(ownerFile) ? readFileSync(ownerFile, "utf8") : "";
  return yardPrices(conf, buyPercentOf(owner));
}

const escape = (s) => String(s).replace(/[.*+?^${}()|[\]\\]/g, "\\$&");

// `--truck <name>`: the yard row to buy, by a part of its prefab name ("Ural"), -1 when none matches. The sheet lists the
// yard's rows in the conf's order (Car0, Car1, ...). Without the option the proof buys the first row the buyer can pay.
export function truckRowIndex(yard, name) {
  const want = String(name ?? "").toLowerCase();
  return want ? yard.findIndex((r) => String(r.prefab).toLowerCase().includes(want)) : -1;
}

// The cash `resetplayer` gives before it is all banked: $12,000 without a pinned truck (the first row the account can pay),
// and with one enough over the dearest row for the $2,000 pocket, the licence and the check fees, so the pinned row's
// Buy button is on the sheet (the lot offers a row only when the bank covers its price).
export function startCashFor(wantTruck, dearest) {
  return wantTruck ? Math.max(12000, Number(dearest) + 3000) : 12000;
}

// The prefab path of the last `[LFDebug:Vehicle] purchased <prefab> owner=` line in the server log, "" when there is none.
export function lastPurchased(log) {
  const all = [...String(log).matchAll(/\[LFDebug:Vehicle\] purchased (\S+) owner=/g)];
  return all.length ? all[all.length - 1][1] : "";
}
const count = (log, re) => (String(log).match(re) || []).length;

// The gate lines the server logs: `truck check gate <n>: <uid> reached it, the check says <GATE_OK|PASSED|FAILED_COURSE>`.
export function gateSaid(log, uid, gate, outcome) {
  return new RegExp(`truck check gate ${gate}: ${escape(uid)} reached it, the check says ${outcome}`).test(String(log));
}

export const lentCount = (log, uid) => count(log, new RegExp(`truck check: ${escape(uid)} was lent`, "g"));
export const deletedCount = (log, uid) => count(log, new RegExp(`truck check: ${escape(uid)}'s lent truck is deleted`, "g"));
export const passedLine = (log, uid) => new RegExp(`truck check: ${escape(uid)} passed the course and holds the truck licence`).test(String(log));
export const failedLine = (log, uid) => new RegExp(`truck check: ${escape(uid)} reached gate 0, expected 2`).test(String(log));
export const lotRefusedLine = (log, uid) => new RegExp(`buy from the lot sheet refused for ${escape(uid)}: #LF-Lot_NeedsTruckLicence`).test(String(log));
export const deskLine = (log, uid) => new RegExp(`licence desk: driving licence issued to ${escape(uid)}`).test(String(log));
// A checkpoint character already holds the driving licence (hours 2 to 6): the desk refuses a second one with this line.
export const deskHeldLine = (log, uid) => new RegExp(`driving licence refused: ${escape(uid)} already holds one`).test(String(log));
export const lotBoughtLine = (log, uid) => new RegExp(`bought [^\\n]* from the lot sheet for ${escape(uid)}`).test(String(log));

function selftest() {
  const uid = "00bbbddd-5e55-0000-0000-000000000001";
  const log = `truck check gate 0: ${uid} reached it, the check says GATE_OK\n`
    + `truck check: ${uid} was lent #1 at the start\n`
    + `truck check: ${uid}'s lent truck is deleted\n`
    + `truck check: ${uid} reached gate 0, expected 2\n`
    + `truck check: ${uid} passed the course and holds the truck licence\n`
    + `buy from the lot sheet refused for ${uid}: #LF-Lot_NeedsTruckLicence\n`
    + `licence desk: driving licence issued to ${uid} for 150\n`
    + `driving licence refused: ${uid} already holds one\n`
    + `bought #LF-Vehicle_Ural4320 from the lot sheet for ${uid}\n`;
  const fails = [];
  const entry = (n, op, prefab, base, lot) => ` LF_VehiclePriceEntry "{3436A0417A00000${n}}" {\n  m_sOp "${op}"\n  m_Prefab "{F1FBD0972FA5FE0${n}}${prefab}"\n  m_iBasePrice ${base}\n  m_sLotId "${lot}"\n }\n`;
  const conf = `LF_VehiclePricesConfig {\n${entry(1, "patch", "Prefabs/M923A1.et", 12000, "truck-yard")}${entry(2, "add", "Prefabs/Ural.et", 9500, "truck-yard")}${entry(3, "add", "Prefabs/UAZ.et", 2500, "car-lot")}}\n`;
  const rows = yardPrices(conf, 80);
  if (rows.length !== 2 || rows[0].price !== 9600 || rows[1].price !== 7600) fails.push(`yardPrices read ${JSON.stringify(rows)}, wanted the two yard rows at 9600 and 7600`);
  if (yardPrices(conf, 100)[1]?.price !== 9500) fails.push("yardPrices at 100 percent is not the base price");
  if (buyPercentOf('{"vehicles":{"buyPercent":80}}') !== 80 || buyPercentOf("not json") !== 100 || buyPercentOf("{}") !== 100) fails.push("buyPercentOf");
  // The real catalogue: the two yard rows exist, and a price change moves with the file, not with the proof.
  const real = readYardPrices();
  if (real.length < 1) fails.push("the pack's price conf lists no truck-yard row");
  if (!gateSaid(log, uid, 0, "GATE_OK")) fails.push("gateSaid missed gate 0");
  if (gateSaid(log, uid, 1, "GATE_OK")) fails.push("gateSaid matched a gate that was not reached");
  if (gateSaid(log, "someone-else", 0, "GATE_OK")) fails.push("gateSaid matched another account");
  if (lentCount(log, uid) !== 1 || deletedCount(log, uid) !== 1) fails.push("lent/deleted counts");
  if (!passedLine(log, uid) || !failedLine(log, uid) || !lotRefusedLine(log, uid) || !lotBoughtLine(log, uid) || !deskLine(log, uid)) fails.push("a one-line judge missed its line");
  if (passedLine(log, "someone-else")) fails.push("passedLine matched another account");
  if (!deskHeldLine(log, uid) || deskHeldLine(log, "someone-else")) fails.push("deskHeldLine (a character that already holds the driving licence)");
  if (truckRowIndex(rows, "ural") !== 1 || truckRowIndex(rows, "M923A1") !== 0 || truckRowIndex(rows, "nothing") !== -1 || truckRowIndex(rows, "") !== -1) fails.push("truckRowIndex");
  const bought = "01:11:13.000 SCRIPT: [LFDebug:Vehicle] purchased {F1FBD0972FA5FE09}Prefabs/Vehicles/Wheeled/M923A1/M923A1_transport.et owner=u1 id=7 plate=CD MU 3665 cash=0 bank=9600\n"
    + "01:12:13.000 SCRIPT: [LFDebug:Vehicle] purchased {F1FBD0972FA5FE0A}Prefabs/Vehicles/Wheeled/Ural4320/Ural4320_transport.et owner=u1 id=8 plate=AB CD 1234 cash=0 bank=100\n";
  if (startCashFor("", 12000) !== 12000 || startCashFor("M923A1", 12000) !== 15000 || startCashFor("Ural", 9500) !== 12500 || startCashFor("Ural", 5000) !== 12000) fails.push("startCashFor");
  if (!lastPurchased(bought).includes("Ural4320") || lastPurchased("no purchase here") !== "") fails.push("lastPurchased");
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "truck-check-proof selftest: FAIL" : "truck-check-proof selftest: PASS");
  return fails.length ? 1 : 0;
}

// The session's pictures are deleted with the session: copy this run's captures to tmp/truck-check-pics/ as JPG
// (1280 px wide) before `session down`. Needs ffmpeg on the PATH; says so and goes on without it.
function keepPictures(dir) {
  const logs = join(dir, "client1", "logs");
  if (!existsSync(logs)) { console.log(`no client logs at ${logs}; no pictures kept`); return; }
  const newest = readdirSync(logs).map((n) => ({ n, t: statSync(join(logs, n)).mtimeMs })).sort((a, b) => b.t - a.t)[0];
  const captures = join(logs, newest?.n ?? "", "el-captures");
  if (!existsSync(captures)) { console.log(`no el-captures in ${captures}; no pictures kept`); return; }
  const out = join(ROOT, "tmp", "truck-check-pics");
  mkdirSync(out, { recursive: true });
  let kept = 0;
  for (const file of readdirSync(captures).filter((f) => /^truck_check_.*\.bmp$/.test(f))) {
    const r = spawnSync("ffmpeg", ["-loglevel", "error", "-y", "-i", join(captures, file), "-vf", "scale=1280:-1", "-q:v", "4", join(out, file.replace(/\.bmp$/, ".jpg"))], { windowsHide: true });
    if (r.status === 0) kept++;
    else console.log(`ffmpeg failed for ${file}: ${String(r.stderr ?? r.error ?? "")}`);
  }
  console.log(`kept ${kept} picture(s) in ${out}`);
}

async function run(args = []) {
  const wantTruck = args.indexOf("--truck") >= 0 ? args[args.indexOf("--truck") + 1] : "";
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${String(r.detail).slice(0, 300)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const expect = (cond, what) => { console.log(`${cond ? "PASS" : "FAIL"}  ${what}`); if (!cond) fails.push(what); };
  const serverLog = () => {
    const s = readSession(dir);
    const log = s ? newestLog(join(s.serverProfile, "logs")) : null;
    return log ? readFileSync(log, "utf8") : "";
  };
  const stateOf = () => {
    let r = on(1, { verb: "state" }, 10000);
    for (let i = 0; i < 8 && r.status !== "ok"; i++) { wait(2500); r = on(1, { verb: "state" }, 10000); }
    const s = parseState(r.detail);
    return { cash: Number(s.cash), bank: Number(s.bank) };
  };
  // The sheet offers a Buy button only on a row the buyer can pay (the dearer truck has none): click the first one.
  const clickBuy = (row = -1) => {
    const widgets = String(on(1, { verb: "ui" }).detail);
    const widget = row >= 0 ? `Car${row}_Action` : /Car\d+_Action/.exec(widgets)?.[0] ?? "Car0_Action";
    if (row >= 0 && !widgets.includes(widget)) console.log(`${widget} is not on the sheet (the buyer cannot pay that row?)`);
    on(1, { verb: "click", widget });
  };
  const shot = (name) => on(1, { verb: "capture", mode: "ui", name }, 25000);
  const uid = readSession(dir)?.clients?.find((c) => c.n === 1)?.identity?.uid ?? "";

  arrive(on, 1, wait);
  stateOf();
  if (/WelcomeGuideDismissButton/.test(String(on(1, { verb: "ui" }).detail))) on(1, { verb: "click", widget: "WelcomeGuideDismissButton" });
  wait(1000);
  expect(!!uid, "client 1 has a recorded session identity");
  const yard = readYardPrices(join(sessionPaths(dir).server, "profile", "LifeFramework", "config.json"));
  expect(yard.length > 0, `the truck yard's rows are read from the catalogue (${yard.map((r) => `${r.prefab.replace(/^.*\//, "")} $${r.price}`).join(", ")})`);
  const dearest = Math.max(0, ...yard.map((r) => r.price));

  // 1. Money: $12,000 in the pocket, all of it into the bank at the island ATM, then $500 back out for the fee.
  // A pinned truck must be payable from the bank: the lot's sheet offers a row only when the bank covers its price, and
  // $12,000 less the fees leaves $11,900, short of the $12,000 M923A1 (`--truck M923A1` found no Buy button, 2026-11-28).
  const startCash = startCashFor(wantTruck, dearest);
  on(1, { verb: "resetplayer", cash: startCash }, 15000);
  // The retake wait counts in-game minutes (about one a minute of real time): the clock is set now and jumped past the wait later.
  on(1, { verb: "time", hour: 11 }, 10000);
  wait(2000);
  for (const side of ["e", "w", "n", "s"]) {
    on(1, { verb: "stand", entityName: ATM, side });
    wait(2500);
    if (String(on(1, { verb: "look" }).detail).includes(ATM_ROW)) break;
  }
  on(1, { verb: "use", className: `${ATM_ROW}__5` }, 10000); // deposit all
  wait(1500);
  const banked = stateOf();
  expect(banked.cash === 0 && banked.bank >= dearest + FEE, `the cash is banked (cash ${banked.cash}, bank ${banked.bank})`);
  const withdraw = () => { on(1, { verb: "use", className: `${ATM_ROW}__3` }, 10000); wait(1500); }; // withdraw $500
  for (let i = 0; i < 4; i++) withdraw();
  const funded = stateOf();
  expect(funded.cash === 2000, `$2,000 in the pocket for the fees (${funded.cash})`);

  // The driving licence: the lot asks for it as well as the truck licence. The desk row needs an ID card (a quest
  // scene), so the harness buys it through the desk's own server entry, fee included.
  const dl = on(1, { verb: "truckcheck", words: ["licence"] }, 10000);
  wait(1500);
  expect(dl.status === "ok" && (deskLine(serverLog(), uid) || deskHeldLine(serverLog(), uid)), "the driving licence is bought through the licence desk's server entry, or the character already held it (server log)");

  // 2. No licence: the yard's counter refuses before any cash moves.
  on(1, { verb: "stand", entityName: LOT });
  wait(2500);
  on(1, { verb: "use", className: BUY_ACTION, entityName: LOT }, 10000);
  wait(1500);
  clickBuy();
  wait(2000);
  shot("truck_check_lot_refused");
  expect(lotRefusedLine(serverLog(), uid), "the lot refuses a buyer with no truck licence (server log)");
  on(1, { verb: "click", widget: "LotClose" });
  wait(500);

  // The examiner's action; true when the server lent a truck for it.
  const take = (lentBefore) => {
    on(1, { verb: "stand", entityName: EXAMINER });
    wait(2500);
    const r = on(1, { verb: "use", className: TAKE_ACTION, entityName: EXAMINER }, 10000);
    wait(3000);
    return r.status === "ok" && lentCount(serverLog(), uid) > lentBefore;
  };
  // Stand 7 m east of an entity on the course, facing it (west), so the truck drives toward the camera and stops at the pole (north of the lane is the silo, south the display row).
  const standBeside = (entity) => { on(1, { verb: "stand", entityName: entity, side: "e7" }); wait(1500); };
  const gateName = (k) => `Fit_ne_compound_truckyard_check_gate_${k}`;
  const drive = (gates) => { const r = on(1, { verb: "truckcheck", words: [gates] }, 10000); expect(r.status === "ok", `truckcheck ${gates} starts (${r.status} ${String(r.detail).slice(0, 80)})`); return r; };

  // 3. Failure path.
  const cashBefore1 = stateOf().cash;
  const lent1 = take(0);
  expect(lent1, "the examiner lends a truck (server log)");
  expect(cashBefore1 - stateOf().cash === FEE, `the $${FEE} fee left the pocket`);
  standBeside(START);
  shot("truck_check_lent");
  standBeside(gateName(1));
  drive("0,1");
  wait(3500);
  expect(gateSaid(serverLog(), uid, 0, "GATE_OK") && gateSaid(serverLog(), uid, 1, "GATE_OK"), "gates 0 and 1 answer GATE_OK in order");
  standBeside(gateName(0));
  drive("0");
  wait(2000);
  shot("truck_check_wrong_gate");
  expect(gateSaid(serverLog(), uid, 0, "FAILED_COURSE"), "driving back into gate 0 is a gate out of order (FAILED_COURSE)");
  expect(failedLine(serverLog(), uid), "the server logs the failed course");
  wait(2500);
  expect(deletedCount(serverLog(), uid) >= 1, "the lent truck is removed after the failure");
  shot("truck_check_failed_truck_gone");

  // 4. The retake: the wait is 10 in-game minutes; the clock jumps two hours and the examiner lends a truck again.
  on(1, { verb: "time", hour: 13 }, 10000);
  wait(1500);
  let lent2 = false;
  for (let i = 0; i < 3 && !lent2; i++) {
    if (i) wait(5000);
    lent2 = take(1);
  }
  expect(lent2, "after the retake wait the examiner lends a truck again");
  for (const gate of [0, 1, 2, 3]) {
    standBeside(gateName(gate));
    drive(String(gate));
    wait(2500);
    shot(`truck_check_gate_${gate}`);
    const want = gate === 3 ? "PASSED" : "GATE_OK";
    expect(gateSaid(serverLog(), uid, gate, want), `gate ${gate} answers ${want}`);
  }
  expect(passedLine(serverLog(), uid), "the server logs the pass and the truck licence");
  wait(2000);
  expect(deletedCount(serverLog(), uid) >= 2, "the lent truck is removed after the pass");

  // 5. The lot sells now.
  on(1, { verb: "stand", entityName: LOT });
  wait(2500);
  on(1, { verb: "use", className: BUY_ACTION, entityName: LOT }, 10000);
  wait(1500);
  shot("truck_check_lot_open");
  const bankBefore = stateOf().bank;
  // Which truck an account gets depends on its bank: after `resetplayer` a fresh account is left with too little for the
  // dearer M923A1 and takes the Ural, a checkpoint-10 account (the RC order) can pay the M923A1. `--truck Ural` pins
  // the row, so the driven haul (`drive-haul-proof`, which the M923A1 cannot yet get through the woodlot sheds) gets the
  // same truck in every order.
  const row = truckRowIndex(yard, wantTruck);
  if (wantTruck) expect(row >= 0, `--truck ${wantTruck} names a row of the yard's catalogue (${yard.map((r) => r.prefab.replace(/^.*\//, "")).join(", ")})`);
  clickBuy(row);
  wait(3000);
  shot("truck_check_bought");
  expect(lotBoughtLine(serverLog(), uid), "the yard sells a truck to the licence holder (server log)");
  if (wantTruck) expect(lastPurchased(serverLog()).toLowerCase().includes(wantTruck.toLowerCase()), `the truck bought is the ${wantTruck} (server log: ${lastPurchased(serverLog()).replace(/^.*\//, "")})`);
  else console.log(`bought: ${lastPurchased(serverLog()).replace(/^.*\//, "")}`);
  const paid = bankBefore - stateOf().bank;
  expect(yard.some((r) => r.price === paid), `the truck's $${paid} left the bank: one of the yard's catalogue prices (${yard.map((r) => r.price).join(", ")})`);

  keepPictures(dir);
  console.log(fails.length ? `truck-check-proof: FAIL (${fails.length})` : "truck-check-proof: PASS");
  return fails.length ? 1 : 0;
}

export default {
  name: "truck-check-proof",
  usage: "truck-check-proof [--truck <Ural|M923A1>] [--selftest]   (--truck pins the yard row bought; without it the first row the account can pay)",
  run: (ctx) => ((ctx?.args ?? []).includes("--selftest") ? selftest() : run(ctx?.args ?? [])),
};

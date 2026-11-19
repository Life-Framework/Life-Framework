import { spawnSync } from "node:child_process";
import { readFileSync } from "node:fs";
import { join } from "node:path";
import { ROOT, arrive, clientBox, newestLog, parseState, readSession, sessionDir } from "../session.mjs";

// The driving licence through a rejoin, a late joiner and a restart (#3186). Saturday's money path is
// "licence, then car": the flagship's car lot refuses a buyer without the licence (#3151, #3168), the
// licence reaches clients as the `RplProp m_iUnlockedBits` bitfield (29 of 30 slots used), and the only
// other licence proof is the gun counter's (one client). Island mission, real Everon:
//   node agent-box/session.mjs up --mission island --clients 1 --profile lowspec --wait
//   node agent-box/session.mjs licence-late-proof
//   node agent-box/session.mjs down
// 1. Client 1 buys the driving licence at the town hall desk (the clerk's desk, `LF_LicenceDeskDrivingAction`).
// 2. Client 1 rejoins (`session rejoin 1`): same account, new player id. Its car lot buy goes through, and
//    the desk's action is hidden from it (its client reads the licence from the replicated bits).
// 3. Client 2 joins late (`session join 2`), has no licence: the lot's refusal toast (`#LF-Lot_NeedsDrivingLicence`,
//    whose %1 is the fee, #3166) reaches it and the server logs the refusal; the desk still offers the action.
// 4. The server restarts (`session restart`, save, stop, load, rejoin): client 1 still holds the licence (the desk
//    stays hidden from it) and client 2, who bought one after the refusal, buys a car without a refusal.
// `--no-restart` stops after step 3. `--selftest` runs the judges on made-up lines, no game.
const CLERK = "Fit_saint_philippe_arrival_town_hall_clerk";
const LOT = "Fit_saint_philippe_carlot_vehicle_lot";
const DESK_ACTION = "LF_LicenceDeskDrivingAction";
const BUY_ACTION = "LF_VehicleBuyAction";
const REFUSAL = "#LF-Lot_NeedsDrivingLicence";

const escape = (s) => String(s).replace(/[.*+?^${}()|[\]\\]/g, "\\$&");

// Did the server log a licence refusal at the lot for this account?
export function refusedFor(log, uid) {
  return new RegExp(`\\[LFDebug:Vehicle\\][^\\n]*purchase refused for ${escape(uid)}: no driving licence`).test(String(log));
}

// Did the server log a car bought from the lot sheet for this account?
export function boughtFor(log, uid) {
  return new RegExp(`\\[LFDebug:Vehicle\\][^\\n]*bought [^\\n]* from the lot sheet for ${escape(uid)}`).test(String(log));
}

// Did the server log the desk issuing the driving licence to this account?
export function licenceSoldTo(log, uid) {
  return new RegExp(`licence desk: driving licence issued to ${escape(uid)}`).test(String(log));
}

function selftest() {
  const fails = [];
  const uid = "00bbbddd-5e55-0000-0000-000000000002";
  const log = `12:00:01 SCRIPT : [LFDebug:Vehicle] purchase refused for ${uid}: no driving licence\n`
    + `12:00:02 SCRIPT : [LFDebug:Vehicle] bought #X from the lot sheet for 00bbbddd-5e55-0000-0000-000000000001\n`;
  if (!refusedFor(log, uid)) fails.push("refusedFor missed the refusal line");
  if (refusedFor(log, "00bbbddd-5e55-0000-0000-000000000001")) fails.push("refusedFor matched another account");
  if (!boughtFor(log, "00bbbddd-5e55-0000-0000-000000000001")) fails.push("boughtFor missed the purchase line");
  if (boughtFor(log, uid)) fails.push("boughtFor matched an account that bought nothing");
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "licence-late-proof selftest: FAIL" : "licence-late-proof selftest: PASS");
  return fails.length ? 1 : 0;
}

async function runLicenceLate(args) {
  if (args.includes("--selftest")) return selftest();
  const noRestart = args.includes("--no-restart");
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail.slice(0, 240)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const expect = (cond, what) => { if (!cond) fails.push(what); };
  const cli = (...a) => {
    const r = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "session", ...a], { encoding: "utf8", windowsHide: true, env: { ...process.env, LF_SESSION_ROUTED: "1" }, timeout: 540000 });
    process.stdout.write(r.stdout ?? "");
    return r.status;
  };
  const serverLog = () => {
    const s = readSession(dir);
    const log = s ? newestLog(join(s.serverProfile, "logs")) : null;
    return log ? readFileSync(log, "utf8") : "";
  };
  const uidOf = (n) => readSession(dir)?.clients?.find((c) => c.n === n)?.identity?.uid ?? "";
  // A client just after a spawn or a join answers no_session for a few seconds: retry.
  const stateOf = (n) => {
    let r = on(n, { verb: "state" }, 10000);
    for (let i = 0; i < 8 && r.status !== "ok"; i++) {
      wait(2500);
      r = on(n, { verb: "state" }, 10000);
    }
    return parseState(r.detail);
  };
  // Stand at the lot, open its sheet and buy the first car; returns the buyer's last toast.
  const tryBuy = (n) => {
    stateOf(n);
    on(n, { verb: "stand", entityName: LOT });
    wait(2000);
    const open = on(n, { verb: "use", className: BUY_ACTION, entityName: LOT }, 10000);
    expect(open.status === "ok", `client ${n} opens the car lot (${open.status} ${open.detail.slice(0, 120)})`);
    wait(1500);
    on(n, { verb: "click", widget: "Car0_Action" });
    wait(2500);
    const toast = stateOf(n).toast;
    on(n, { verb: "click", widget: "LotClose" });
    wait(500);
    return toast;
  };
  // The desk's driving action is shown only to a player who lacks the licence (the client reads the replicated bits).
  const deskOffered = (n) => {
    on(n, { verb: "stand", entityName: CLERK });
    wait(1500);
    return on(n, { verb: "use", className: DESK_ACTION }, 10000);
  };
  const done = () => {
    for (const f of fails) console.log(`FAIL  ${f}`);
    console.log(fails.length
      ? "licence-late-proof: FAIL"
      : "licence-late-proof: PASS (the licence holds through a rejoin and a restart; a late joiner without one gets the refusal)");
    return fails.length ? 1 : 0;
  };

  // 1. Client 1 buys the licence at the desk.
  arrive(on, 1, wait);
  wait(3000);
  on(1, { verb: "input", action: "MenuBack" });
  wait(1500);
  const uid1 = uidOf(1);
  expect(!!uid1, "client 1 has a recorded session identity");
  const before = stateOf(1);
  stateOf(1);
  const bought = deskOffered(1);
  expect(bought.status === "ok", `client 1 buys the driving licence at the desk (${bought.status} ${bought.detail.slice(0, 160)})`);
  wait(2500);
  const after = stateOf(1);
  expect(Number(after.cash) < Number(before.cash), `the licence fee left client 1's cash (${before.cash} -> ${after.cash})`);
  expect(licenceSoldTo(serverLog(), uid1), "the server logged the licence desk issuing client 1 the driving licence");

  // 2. Client 1 rejoins: the same account, a new player id.
  const rejoin = cli("rejoin", "1");
  expect(rejoin === 0, `session rejoin 1 exited ${rejoin}`);
  wait(20000);
  const toast1 = tryBuy(1);
  expect(toast1 !== REFUSAL, `after the rejoin client 1's car lot buy is not refused for the licence (toast ${toast1})`);
  expect(boughtFor(serverLog(), uid1), "the server logged client 1's car lot purchase after the rejoin");
  const hidden = deskOffered(1);
  expect(hidden.status !== "ok", `after the rejoin the desk no longer offers client 1 the driving licence (${hidden.status} ${hidden.detail.slice(0, 120)})`);

  // 3. Client 2 joins late and has no licence.
  const join2 = cli("join", "2");
  expect(join2 === 0, `session join 2 exited ${join2}`);
  if (join2 === 0) {
    arrive(on, 2, wait);
    wait(8000);
    on(2, { verb: "input", action: "MenuBack" });
    wait(1500);
    const uid2 = uidOf(2);
    // The lot first, while client 2 is still unlicensed.
    const toast2 = tryBuy(2);
    expect(toast2 === REFUSAL, `client 2, without a licence, gets the lot's refusal toast (toast ${toast2})`);
    expect(refusedFor(serverLog(), uid2), `the server logged the licence refusal for client 2 (${uid2})`);
    // Then the desk still offers client 2 the licence (its client reads no bit): it buys it, so the restart can show it kept.
    const offered = deskOffered(2);
    expect(offered.status === "ok", `the desk offers client 2 the driving licence (${offered.status} ${offered.detail.slice(0, 120)})`);
    wait(2500);
  }

  // 4. The server restarts: both licences are kept.
  if (!noRestart) {
    const restart = cli("restart");
    expect(restart === 0, `session restart exited ${restart}`);
    wait(20000);
    // Client 1 spent its cash on a car, so the lot's server-side answer is read through client 2, who bought the licence just before the restart.
    const toast3 = tryBuy(2);
    expect(toast3 !== REFUSAL, `after the server restart client 2's car lot buy is not refused for the licence (toast ${toast3})`);
    const hidden2 = deskOffered(1);
    expect(hidden2.status !== "ok", `after the restart the desk no longer offers client 1 the driving licence (${hidden2.status} ${hidden2.detail.slice(0, 120)})`);
  }

  return done();
}

export default {
  name: "licence-late-proof",
  noReset: true,
  usage: "licence-late-proof [--no-restart] | --selftest   (session up --mission island --clients 1)",
  run: (ctx) => runLicenceLate(ctx.args ?? []),
};

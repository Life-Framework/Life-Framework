import { readFileSync } from "node:fs";
import { join } from "node:path";
import { arrive, clientBox, newestLog, parseState, sessionDir, sessionPaths } from "../session.mjs";

// #5152: a player who goes down pages the on-duty medic. Client 1 clocks in at the Saint-Philippe
// clinic desk; client 2 is downed through the harness `shoot`. The server must place a medic call
// for client 2 (LF_DownedCalls), pin it to the medic board and raise its map mark; client 1 must see
// the pager toast (`seen`); a revive by client 1 must close the call (board line and map mark).
//   node agent-box/session.mjs up --mission island --clients 2 --profile full
//   node agent-box/session.mjs downed-call
//   node agent-box/session.mjs down
const DESK = "Fit_saint_philippe_clinic_lf_medic_duty_desk";
const BOARD = "Fit_saint_philippe_clinic_lf_dispatch_board_medic";

// Index of the first line at or after `from` holding every needle, or -1.
function findLine(lines, from, ...needles) {
  for (let i = from; i < lines.length; i++) if (needles.every((n) => lines[i].includes(n))) return i;
  return -1;
}

async function runDownedCall() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${String(r.detail).slice(0, 240)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const logLines = () => {
    const f = newestLog(join(sessionPaths(dir).server, "logs"));
    return f ? readFileSync(f, "utf8").split("\n") : [];
  };
  const base = logLines().length;
  const fresh = () => logLines().slice(base);
  const state = (n) => parseState(on(n, { verb: "state" }, 10000).detail);
  const fails = [];

  arrive(on, 1, wait);
  arrive(on, 2, wait);
  wait(6000);

  // 1. Client 1 clocks in as the medic; client 2 stands beside them (any distance works; near keeps the revive short).
  if (!String(state(1).faction).includes("MEDIC")) {
    on(1, { verb: "stand", entityName: DESK });
    wait(2500);
    on(1, { verb: "look" });
    on(1, { verb: "use", className: "LF_DutyAction", entityName: DESK }, 10000);
    wait(2500);
  }
  if (!String(state(1).faction).includes("MEDIC")) fails.push("clock in: client 1 is not MEDIC after the desk");
  on(2, { verb: "stand", entityName: BOARD });
  wait(2500);
  // The kit's pager reaches the client's inventory a few seconds after the clock-in: poll for it.
  const pagerCount = (budgetMs = 20000) => {
    const t0 = Date.now();
    for (;;) {
      const r = on(1, { verb: "pager" }, 10000);
      const n = Number((/messages=(\d+)/.exec(String(r.detail)) || [])[1] ?? NaN);
      if (Number.isFinite(n) || Date.now() - t0 > budgetMs) {
        if (Number.isFinite(n)) on(1, { verb: "click", widget: "Close" });
        return n;
      }
      wait(2000);
    }
  };
  const pagedBefore = pagerCount();
  if (!Number.isFinite(pagedBefore)) fails.push("pager: the clocked-in medic carries no pager");

  // 2. Client 2 goes down.
  const victim = state(2);
  on(1, { verb: "shoot", playerId: Number(victim.player) }, 10000);
  wait(3000);
  if (state(2).downed !== "1") fails.push("down: client 2 is not downed after the shot");

  const lines = fresh();
  const paged = findLine(lines, 0, "[LFDebug:DownedCall]", "paged");
  if (paged < 0) fails.push("call: no 'downed <uid> ... paged N medic(s)' line");
  if (findLine(lines, 0, "[LFDebug:ServiceCalls]", "called medic") < 0) fails.push("call: no medic service call was placed");
  if (findLine(lines, 0, "[LFDebug:ServiceBoard]", "pinned to the medic board") < 0) fails.push("board: the call is not on the medic board");
  // The mark line right after the downed call's own board pin (a casualty may have rolled first).
  const pin = findLine(lines, 0, "[LFDebug:ServiceBoard]", "pinned to the medic board", "#LF-DownedCall_Summary");
  const mark = pin < 0 ? -1 : findLine(lines, pin, "[LFDebug:ServiceCalls]", "is map mark dispatch:");
  if (mark < 0) fails.push("map: no dispatch map mark line for the call");
  else console.log(`map mark: ${lines[mark].trim()}`);
  const pagerLine = findLine(lines, 0, "[LFDebug:Pager]", "#LF-DownedCall_Summary", "paged to");
  if (pagerLine < 0) fails.push("pager: the server paged no pager with the call");
  else console.log(`server: ${lines[pagerLine].trim()}`);

  // 3. What the medic's client shows: the replicated pager card.
  let pagedAfter = NaN;
  for (let i = 0; i < 10; i++) {
    wait(1500);
    const r = on(1, { verb: "pager" }, 10000);
    pagedAfter = Number((/messages=(\d+)/.exec(String(r.detail)) || [])[1] ?? NaN);
    if (pagedAfter > pagedBefore) break;
    on(1, { verb: "click", widget: "Close" });
  }
  wait(800);
  on(1, { verb: "capture", mode: "ui", name: "downed_call_pager" }, 25000);
  on(1, { verb: "click", widget: "Close" });
  console.log(`medic pager messages on client 1: before ${pagedBefore} after ${pagedAfter}`);
  if (!(pagedAfter > pagedBefore || (pagedBefore >= 5 && pagedAfter >= 5))) fails.push(`pager: client 1's pager did not get the call (${pagedBefore} -> ${pagedAfter})`);
  on(1, { verb: "stand", entityName: BOARD });
  wait(2500);
  on(1, { verb: "look" });
  on(1, { verb: "use", className: "LF_WantedBoardReadAction", entityName: BOARD }, 10000);
  wait(2500);
  on(1, { verb: "capture", mode: "ui", name: "downed_call_board" }, 25000);
  const board = on(1, { verb: "ui" }).detail;
  on(1, { verb: "click", widget: "Close" });
  wait(800);
  console.log(`board on client 1: ${String(board).slice(0, 300)}`);

  // 4. The medic revives client 2: the call closes.
  const patient = `Player${victim.player}`;
  on(1, { verb: "stand", entityName: patient });
  wait(2500);
  on(1, { verb: "look" });
  const revive = on(1, { verb: "use", className: "LF_ReviveAction", entityName: patient }, 15000);
  wait(8000);
  const after = fresh();
  if (state(2).downed === "1") fails.push(`revive: client 2 is still downed (${revive.status} ${revive.detail})`);
  else {
    if (findLine(after, 0, "[LFDebug:DownedCall]", "closed (revived)") < 0) fails.push("close: the revive did not close the call");
    if (findLine(after, 0, "[LFDebug:ServiceBoard]", "on the medic board closed") < 0) fails.push("close: the medic board line did not close");
  }

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "downed-call: FAIL" : "downed-call: PASS (downed player paged the medic: call, board, pager; revive closed it)");
  return fails.length ? 1 : 0;
}

export default { name: "downed-call", usage: "downed-call", run: () => runDownedCall() };

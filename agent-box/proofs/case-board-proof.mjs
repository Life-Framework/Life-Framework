import { existsSync, readdirSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { ROB_TARGET, arrive, clientBox, clientSees, sessionDir } from "../session.mjs";

// Scene evidence and the case board (#148) on a running `up --clients 2 --profile full` session.
// Client 2 (on the police roster) clocks in and stands by the fuel till, so client 1's robbery there
// is caught in the act (identified: the portrait goes on the board). The robbery leaves a footprint
// card behind the robber, on the ground. Client 1 (a civilian) cannot bag it; client 2 bags it, asks
// the fuel clerk what she saw (a statement paper when she remembers something), bags that too, and
// pins both at the case board in the station porch. Client 2's board draws one case with its cards and
// strings; client 1 at the same board draws nothing (police detail never reaches a civilian). Client 2
// clocks out (the face goes) and back in (the list is asked for again: the path a late joiner on duty
// takes). Shots: the card on the ground, the board, a close-up, the civilian's plain cork.
//   node agent-box/session.mjs up --clients 2 --profile full --wait
//   node agent-box/session.mjs case-board-proof
//   node agent-box/session.mjs down
const BOARD = "Roadmap_Police_CaseBoard";
// Roadmap_Police_CaseBoard in Street_Police.layer: the cork's centre, facing west (-x).
const BOARD_AT = { x: 234.72, y: 1.555, z: 239.19 };
const DESK = "Roadmap_Police_Desk";
const CLERK = "Roadmap_Fuel_Clerk";

function logsOf(dir) {
  if (!existsSync(dir)) return "";
  return readdirSync(dir).sort().map((d) => join(dir, d, "console.log")).filter(existsSync).map((p) => readFileSync(p, "utf8")).join("\n");
}

export function casesOf(seen) {
  const m = /^(\d+)\/(\d+)\/(\w+)$/.exec(seen?.cases ?? "");
  return m ? { cases: Number(m[1]), cards: Number(m[2]), face: m[3] } : null;
}

async function runCaseBoardProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${String(r.detail).slice(0, 220)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const must = (r, what) => {
    if (r.status !== "ok") fails.push(`${what} returned ${r.status} ${r.detail}`);
    return r;
  };
  const expect = (n, label, judge, budgetMs = 15000) => {
    const r = clientSees(on, wait, n, (s) => {
      const c = casesOf(s);
      return c ? judge(c) : [`no cases field: ${s.cases}`];
    }, { label, budgetMs });
    fails.push(...r.fails);
    if (!r.fails.length) console.log(`ok    ${label} (${r.seconds.toFixed(1)} s)`);
    return r;
  };
  const serverLog = () => logsOf(join(dir, "server", "logs"));
  const shot = (n, name, from, to, fov = 60) => {
    on(n, { verb: "camera", args: "hud off" });
    on(n, { verb: "camera", args: "stop" });
    on(n, { verb: "camera", args: `static ${from.x} ${from.y} ${from.z} ${to.x} ${to.y} ${to.z} ${fov} 50` });
    wait(2500);
    const r = on(n, { verb: "capture", mode: "ui", name }, 25000);
    on(n, { verb: "camera", args: "stop" });
    on(n, { verb: "camera", args: "hud on" });
    return r;
  };
  const clockToggle = (n, what) => {
    on(n, { verb: "stand", entityName: DESK });
    wait(2500);
    on(n, { verb: "look" });
    must(on(n, { verb: "use", className: "LF_DutyAction", entityName: DESK }, 10000), what);
    wait(2500);
  };

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  clockToggle(2, "clock in");

  // The robbery, with the officer beside the till: caught in the act.
  on(2, { verb: "stand", entityName: ROB_TARGET });
  on(1, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  on(1, { verb: "look" });
  must(on(1, { verb: "use", className: "LF_RobAction", entityName: ROB_TARGET }, 10000), "rob the till");
  wait(4000);

  const left = /\[LFDebug:Evidence\] footprint left as evidence for report #(\d+) at .*? at <(-?[\d.]+),(-?[\d.]+),(-?[\d.]+)>/.exec(serverLog());
  if (!left) fails.push("the robbery left no footprint card (server log)");
  const card = left ? { x: Number(left[2]), y: Number(left[3]), z: Number(left[4]) } : null;
  if (card) {
    console.log(`ok    footprint card for report #${left[1]} at ${card.x}, ${card.y}, ${card.z}`);
    shot(2, "case_board_footprint_card_client2", { x: card.x + 1.1, y: card.y + 0.9, z: card.z + 0.6 }, card, 50);
  }

  // A civilian cannot bag it; the officer can.
  const civilian = on(1, { verb: "use", className: "LF_BagEvidenceAction" }, 10000);
  if (civilian.status === "ok") fails.push(`a civilian bagged evidence: ${civilian.detail}`);
  on(2, { verb: "stand", entityName: "Player1" });
  wait(2500);
  on(2, { verb: "look" });
  must(on(2, { verb: "use", className: "LF_BagEvidenceAction" }, 10000), "officer bags the footprint card");
  wait(2000);
  if (!/\[LFDebug:Evidence\] footprint bagged for report #/.test(serverLog())) fails.push("no footprint bagged on the server");

  // The clerk saw it: her statement lies at her feet when she remembers something (NPC accuracy).
  on(2, { verb: "stand", entityName: CLERK });
  wait(2500);
  on(2, { verb: "look" });
  must(on(2, { verb: "use", className: "LF_WitnessAskAction", entityName: CLERK }, 10000), "ask the clerk");
  wait(2000);
  let statement = /\[LFDebug:Evidence\] statement left as evidence/.test(serverLog());
  if (statement) {
    must(on(2, { verb: "use", className: "LF_BagEvidenceAction" }, 10000), "officer bags the statement");
    wait(2000);
  } else {
    console.log("note  the clerk remembered nothing this time (NPC accuracy roll): one card only");
  }

  // Pin at the board in the station porch.
  on(2, { verb: "stand", entityName: BOARD });
  wait(2500);
  on(2, { verb: "look" });
  must(on(2, { verb: "use", className: "LF_CaseBoardPinAction", entityName: BOARD }, 10000), "pin evidence");
  const cards = statement ? 2 : 1;
  expect(2, `the officer's board draws the case with ${cards} card(s)`, (c) => (c.cases === 1 && c.cards === cards && c.face !== "released" ? [] : [`cases=${c.cases}/${c.cards}/${c.face}`]), 20000);
  // Toward the board from the porch, then the cards up close (after the settle refresh).
  wait(11000);
  shot(2, "case_board_officer_client2", { x: BOARD_AT.x - 2.0, y: BOARD_AT.y + 0.1, z: BOARD_AT.z + 0.2 }, BOARD_AT, 60);
  shot(2, "case_board_close_client2", { x: BOARD_AT.x - 0.95, y: BOARD_AT.y, z: BOARD_AT.z }, BOARD_AT, 60);

  // A civilian at the same board: plain cork, no face, nothing received.
  on(1, { verb: "stand", entityName: BOARD });
  wait(4000);
  expect(1, "a civilian's board draws nothing", (c) => (c.cases === 0 && c.cards === 0 && c.face === "released" ? [] : [`cases=${c.cases}/${c.cards}/${c.face}`]), 8000);
  shot(1, "case_board_civilian_client1", { x: BOARD_AT.x - 2.0, y: BOARD_AT.y + 0.1, z: BOARD_AT.z + 0.2 }, BOARD_AT, 60);

  // Off duty the face goes; back on duty the list is asked for again (a late joiner's path).
  clockToggle(2, "clock out");
  clockToggle(2, "clock in again");
  on(2, { verb: "stand", entityName: BOARD });
  expect(2, "back on duty the board is drawn again from the list it asked for", (c) => (c.cases === 1 && c.cards === cards ? [] : [`cases=${c.cases}/${c.cards}/${c.face}`]), 20000);

  on(1, { verb: "resetcrime" }, 10000);
  const log1 = logsOf(join(dir, "client1", "logs"));
  const log2 = logsOf(join(dir, "client2", "logs"));
  const server = serverLog();
  if (!/\[LFDebug:CaseBoard\] card for locker row #\d+ \(report #\d+, footprint\) pinned/.test(server)) fails.push("the server never pinned the footprint card");
  if (!/\[LFDebug:CaseBoard\] list changed: .* sent to 1 on-duty officer/.test(server)) fails.push("the server never sent the list to the one on-duty officer");
  if (/\[LFDebug:CaseBoard\] client: [1-9]\d* entr/.test(log1)) fails.push("the civilian's client received case entries");
  if (!/\[LFDebug:CaseBoard\] client: face built at .* with 1 case\(s\)/.test(log2)) fails.push("client 2 never built a face with the case");
  if (!/\[LFDebug:CaseBoard\] client: off duty/.test(log2)) fails.push("client 2 never dropped the list off duty");
  if (!/\[LFDebug:CaseBoard\] case #\d+: portrait of the identified suspect/.test(log2)) fails.push("client 2 never drew the identified suspect's portrait");
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "case-board-proof: FAIL" : "case-board-proof: PASS (footprint on the ground, police-only bag, pin, officer face, civilian blank, duty re-ask)");
  return fails.length ? 1 : 0;
}

export default { name: "case-board-proof", usage: "case-board-proof   (up --clients 2 --profile full; client 2 on the police roster)", run: () => runCaseBoardProof() };

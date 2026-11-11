#!/usr/bin/env node
// M2 QA runs the per-feature proofs in session.mjs do not cover, on a running session
// (`session.mjs up --clients 2 --wait`, `roster 2` before `up`): the comms loop from a payphone to
// a taken call (police, then medic with the pager), and the /report flag. Each step prints the
// command, the state, and what the server log says; the verdict is the log lines.
//
//   node agent-box/qa-m2.mjs comms     payphone -> board -> take -> caller hears back; pager card
//   node agent-box/qa-m2.mjs report    /report with a recent incident and with none
//   node agent-box/qa-m2.mjs square     the spawn square payphone (#489): the five call actions are offered where a player stands, and a call connects
//   node agent-box/qa-m2.mjs normalize  clock any officer or medic out so the next proof starts clean
//   node agent-box/qa-m2.mjs selftest

import { readFileSync } from "node:fs";
import { dirname, join, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..");

export function parseState(detail) {
  const out = {};
  for (const part of String(detail ?? "").trim().split(/\s+/)) {
    const eq = part.indexOf("=");
    if (eq > 0) out[part.slice(0, eq)] = part.slice(eq + 1);
  }
  return out;
}

// Lines of a server console log that contain every needle.
export function logHas(text, ...needles) {
  return String(text ?? "")
    .split("\n")
    .filter((line) => needles.every((n) => line.includes(n)));
}

export function selftest() {
  const f = [];
  if (parseState("a=1 b=x").b !== "x") f.push("parseState");
  if (logHas("x [LFDebug:Payphone] call\ny", "Payphone", "call").length !== 1) f.push("logHas");
  return f;
}

async function connect() {
  const h = await import("./harness.mjs");
  const ses = await import("./session.mjs");
  const dir = ses.sessionDir();
  const current = ses.readSession(dir);
  if (!current || current.stoppedAt) throw new Error("no session; node agent-box/session.mjs up --clients 2 --wait");
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const on = (n, spec, timeout) => {
    h.setBoxes([ses.clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line ? r.line.replace(/^\S+ /, "") : spec.verb} -> ${r.status} ${r.detail}`);
    return r;
  };
  const state = (n) => parseState(on(n, { verb: "state" }, 10000).detail);
  const shot = (n, name) => on(n, { verb: "capture", mode: "ui", name }, 25000);
  const serverLog = () => {
    const file = ses.newestLog(join(ses.sessionPaths(dir).server, "logs"));
    return file ? readFileSync(file, "utf8") : "";
  };
  const fails = [];
  const expect = (ok, what) => {
    console.log(`${ok ? "pass" : "FAIL"}  ${what}`);
    if (!ok) fails.push(what);
  };
  const arrive = () => {
    for (const n of [1, 2]) {
      const ui = on(n, { verb: "ui" });
      if (ui.status !== "ok" || !ui.detail.includes("SignButton")) continue;
      on(n, { verb: "type", widget: "RegFirstName", text: n === 1 ? "Ada" : "Bea" });
      on(n, { verb: "type", widget: "RegLastName", text: "Tester" });
      on(n, { verb: "type", widget: "RegAge", text: "30" });
      on(n, { verb: "click", widget: "SignButton" });
      wait(1500);
    }
    wait(8000);
  };
  const duty = (n, desk) => {
    on(n, { verb: "stand", entityName: desk });
    wait(2500);
    on(n, { verb: "look" });
    const r = on(n, { verb: "use", className: "LF_DutyAction", entityName: desk }, 10000);
    wait(2500);
    return r;
  };
  return { h, on, state, shot, wait, serverLog, fails, expect, arrive, duty };
}

// A payphone carries one action per service preset, in preset order (three police, medic, taxi);
// "LF_PayphoneAction__3" is the fourth. The server does the rest, as for any use.
function dial(ctx, n, phone, offerName, index) {
  const { on, wait } = ctx;
  on(n, { verb: "stand", entityName: phone });
  wait(2500);
  on(n, { verb: "look" });
  const r = on(n, { verb: "use", className: index ? `LF_PayphoneAction__${index}` : "LF_PayphoneAction", entityName: phone }, 10000);
  wait(2500);
  console.log(`dialled ${offerName}: ${r.status} ${r.detail}`);
  return r;
}

async function runComms() {
  const ctx = await connect();
  const { on, state, shot, wait, serverLog, expect, arrive, duty } = ctx;
  arrive();

  // 1. Police: a civilian calls from the station payphone; the officer's board shows it; take; the caller hears back.
  duty(2, "Roadmap_Police_Desk");
  expect(state(2).duty === "1", "officer clocked in");
  dial(ctx, 1, "Roadmap_Police_Payphone", "report a crime (first offer)", 0);
  const afterCall = state(1);
  shot(1, "qa_m2_payphone_call_client1");
  expect(logHas(serverLog(), "Payphone", "police").length > 0 || /Payphone_Connected/.test(afterCall.toast), `payphone connected the call (toast ${afterCall.toast})`);

  on(2, { verb: "stand", entityName: "Roadmap_Police_DispatchBoard" });
  wait(2500);
  on(2, { verb: "use", className: "LF_WantedBoardReadAction", entityName: "Roadmap_Police_DispatchBoard" }, 10000);
  wait(2500);
  const boardText = on(2, { verb: "ui", mode: "text" }).detail;
  shot(2, "qa_m2_board_with_call_client2");
  expect(!/No calls/i.test(boardText || "") && /Police - dispatch/.test(boardText || ""), `the dispatch board lists the call: ${String(boardText).slice(0, 160)}`);
  on(2, { verb: "click", widget: "Close" });
  wait(800);

  const take = on(2, { verb: "use", className: "LF_ServiceTakeAction", entityName: "Roadmap_Police_DispatchBoard" }, 10000);
  wait(3000);
  expect(take.status === "ok", `take returned ${take.status} ${take.detail}`);
  const officer = state(2);
  const caller = state(1);
  shot(1, "qa_m2_caller_hears_back_client1");
  expect(officer.toast === "#LF-Dispatch_TakeDone", `the officer's take toast is ${officer.toast}`);
  expect(logHas(serverLog(), "ServiceBoard", "telling caller").length > 0, "the server told the caller a unit is on the way");
  expect(caller.toast === "#LF-Service_UnitOnTheWay", `the caller's toast is ${caller.toast}`);
  duty(2, "Roadmap_Police_Desk");

  // 2. Medic: the doctor preset on the hospital payphone pages the medic's pager.
  duty(2, "Roadmap_Hospital_Duty_Desk");
  const medic = state(2);
  console.log(`client 2 after the hospital desk: ${JSON.stringify(medic)}`);
  console.log("waiting 62 s for the per-player payphone cooldown");
  wait(62000);
  dial(ctx, 1, "Roadmap_Hospital_Payphone", "call a medic (4th offer)", 3);
  wait(1500);
  const page = on(2, { verb: "pager" }, 10000);
  wait(1500);
  shot(2, "qa_m2_pager_card_client2");
  on(2, { verb: "click", widget: "Close" });
  expect(page.status === "ok" && /messages=[1-9]/.test(page.detail), `the pager holds the call: ${page.status} ${page.detail}`);
  expect(logHas(serverLog(), "Pager", "paged to").length > 0, "the server paged a pager");

  console.log(ctx.fails.length ? "comms: FAIL" : "comms: PASS");
  return ctx.fails.length ? 1 : 0;
}

async function runReport() {
  const ctx = await connect();
  const { on, state, shot, wait, serverLog, expect, arrive } = ctx;
  arrive();
  // The first report flags what the log holds under the player's name (any cash move or hit counts)
  // and thanks them; a second one inside the cooldown is told to wait.
  on(1, { verb: "stand", entityName: "Roadmap_Fuel_Till", side: "s" });
  wait(2500);
  on(1, { verb: "use", className: "LF_RobAction", entityName: "Roadmap_Fuel_Till" }, 10000);
  wait(3000);
  on(1, { verb: "report" }, 10000);
  wait(2500);
  const first = state(1);
  shot(1, "qa_m2_report_sent_client1");
  expect(logHas(serverLog(), "IncidentReport", "flagged").length > 0, "the report flagged incidents in the log");
  expect(first.toast === "#LF-IncidentReport_Sent", `the player is thanked (toast ${first.toast})`);
  on(1, { verb: "report" }, 10000);
  wait(2500);
  const second = state(1);
  shot(1, "qa_m2_report_cooldown_client1");
  expect(logHas(serverLog(), "IncidentReport", "refused: cooldown").length > 0 && second.toast === "#LF-IncidentReport_Cooldown", `a second report inside the cooldown is refused (toast ${second.toast})`);
  console.log(ctx.fails.length ? "report: FAIL" : "report: PASS");
  return ctx.fails.length ? 1 : 0;
}

// #489 moved Village_Payphone to the square and turned it to face west: look from where a player
// stands must offer all five presets, and the first one dials through.
async function runSquare() {
  const ctx = await connect();
  const { on, state, shot, wait, expect, arrive } = ctx;
  arrive();
  on(1, { verb: "stand", entityName: "Village_Payphone" });
  wait(2500);
  const look = on(1, { verb: "look" }).detail || "";
  shot(1, "qa_m2_square_payphone_client1");
  const offered = (look.match(/LF_PayphoneAction@Village_Payphone/g) || []).length;
  expect(offered === 5, `five payphone actions are offered at the square phone (looked: ${offered}) in ${look.slice(0, 200)}`);
  const call = on(1, { verb: "use", className: "LF_PayphoneAction", entityName: "Village_Payphone" }, 10000);
  wait(2500);
  const st = state(1);
  expect(call.status === "ok" && (st.toast === "#LF-Payphone_Connected" || st.toast === "#LF-Service_NoAnswer"), `the dial goes through from the square phone (connected, or no unit on duty to answer) (${call.status} ${call.detail}, toast ${st.toast})`);
  console.log(ctx.fails.length ? "square: FAIL" : "square: PASS");
  return ctx.fails.length ? 1 : 0;
}

// The proofs clock in by toggling the duty desk, so one that ends on duty turns the next one off.
async function runNormalize() {
  const ctx = await connect();
  for (const n of [1, 2, 3]) {
    let st;
    try {
      st = ctx.state(n);
    } catch {
      continue;
    }
    if (st.duty !== "1") continue;
    const desk = st.faction === "MEDIC" ? "Roadmap_Hospital_Duty_Desk" : "Roadmap_Police_Desk";
    ctx.duty(n, desk);
    console.log(`client ${n} clocked out of ${st.faction}: ${ctx.state(n).duty === "0" ? "ok" : "still on duty"}`);
  }
  return 0;
}

async function main(argv) {
  const cmd = argv[0];
  if (cmd === "selftest") {
    const f = selftest();
    for (const x of f) console.log(`  ${x}`);
    console.log(f.length ? "qa-m2 selftest: FAIL" : "qa-m2 selftest: OK");
    return f.length ? 1 : 0;
  }
  if (cmd === "normalize") return runNormalize();
  if (cmd === "square") return runSquare();
  if (cmd === "comms") return runComms();
  if (cmd === "report") return runReport();
  console.log("usage: node agent-box/qa-m2.mjs comms|report|selftest");
  return 2;
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  main(process.argv.slice(2)).then(
    (c) => process.exit(c),
    (e) => {
      console.error(e.message);
      process.exit(1);
    },
  );
}

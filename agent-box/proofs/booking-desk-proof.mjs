import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { ROOT, arrive, clientBox, newestLog, parseState, sessionDir, sessionPaths } from "../session.mjs";
import { AMOUNT, MONEY, parseMoney } from "./_money.mjs";
import { ensureCuffs, requireBodies } from "./_preconditions.mjs";

// The police booking desk walked end to end on the real island (#4699, #4629 pay now, #4662 licence seizure),
// two players on a dedicated server under the flagship owner config:
//   node agent-box/session.mjs up --mission island --clients 2 --profile full --wait
//   node agent-box/session.mjs booking-desk-proof [--name <shot prefix>]
//   node agent-box/session.mjs down
// Client 1 (A) signs up at the Saint-Philippe duty desk (alpha approve-on-submit) and clocks in. Client 2 (B) holds a
// firearm permit and is charged with weapon theft (`bookcharge weapontheft`: the island's real crimes are jail only, so
// only the harness gives a fineable charge). A raises B's hands, cuffs them, both stand at the booking desk beside the cell
// gate, A uses "Book suspect", ticks the code on the charge sheet and signs.
//   Path 1, pay now: B's offer shows "Pay $X now" and "Take the time"; B pays: cash drops by exactly the fine, the cuffs
//   come off, the wanted level clears, the firearm permit is seized (#4662) and B reads the toast.
//   Path 2, the cell: B robs the fuel till through the attendant (a real offence, jail only), A cuffs and books B at the
//   desk; B's offer has only "Take the time", B takes it: B is in the cell and the sentence counts down.
// Every step prints PASS or FAIL. Shots of the sheet, the offer and the toasts go to the session's shots folder.
async function runBookingDeskProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const flag = (name, dflt) => { const i = process.argv.indexOf(name); return i > 0 ? process.argv[i + 1] : dflt; };
  const prefix = flag("--name", "booking_desk");
  // --order pay-first|cell-first: which booking B meets first (#4791: a second booking in one session).
  // --cell-crime till|robbery: the cell path's crime, the fuel-till dialogue or a `bookcharge robbery` report.
  const order = flag("--order", "pay-first");
  const cellCrime = flag("--cell-crime", "till");
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${String(r.detail).slice(0, 300)}`);
    return r;
  };
  const waitText = (n, text, ms = 10000) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.waitForText(text, ms);
    console.log(`client ${n}: wait-text ${text} -> ${r.status} ${r.found ? "found" : r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  // A client can answer no_session for a few seconds after a booking settles; read again until it answers.
  const state = (n) => {
    let r;
    for (let i = 0; i < 8; i++) {
      r = on(n, { verb: "state" }, 10000);
      if (r.status === "ok") break;
      wait(2000);
    }
    return parseState(r.detail);
  };
  const serverLog = () => {
    const f = newestLog(join(sessionPaths(dir).server, "logs"));
    return f ? readFileSync(f, "utf8") : "";
  };
  const logCount = (re) => (serverLog().match(re) ?? []).length;
  const steps = [];
  const step = (name, ok, why) => { steps.push({ name, ok, why }); console.log(`${ok ? "PASS" : "FAIL"}  ${name}${ok ? "" : `: ${why}`}`); };
  const shot = (n, name) => on(n, { verb: "capture", mode: "ui", name: `${prefix}_${name}` }, 25000);
  // The suspect answers with the interact key, as a player does: the two rows are one action class on one desk, which
  // the server-side `use` cannot tell apart (it cannot see the client's offer), so scroll to the row and confirm.
  // The two rows are one class, so the row is picked by its label (`aim` name=): the handler remembers the row last
  // used, so "scroll 0" took the time instead of paying on a second booking (#4791).
  // `beforeConfirm` runs after the sweep and the shot, just before the confirm key.
  const answer = (n, want, label, beforeConfirm) => {
    // A player looks down at a desk; the harness's level ray passes over a 0.75 m desk and finds no context.
    // Sweep the pitch until the interact key selects the desk's answer, then look level again. The pitch is absolute
    // (`input CharacterPitch`): the suspect's client stalls for about 20 s after paying a fine, a relative turn sent
    // then was lost, and every later sweep landed 30 degrees low, on the floor. That was the "no interaction for
    // ~90 s after the till robbery" of #4791 and #4803; the game offered the row all along.
    let aim;
    const started = Date.now();
    const selected = () => /selected=LF_BookingDeskAnswerAction/.test(aim.detail);
    do {
      for (const target of [-45, -30, -20, -55, -10]) {
        on(n, { verb: "input", action: "CharacterPitch", value: target }, 8000);
        wait(800);
        aim = on(n, { verb: "aim", words: [] }, 10000);
        if (selected()) break;
      }
      if (!selected()) {
        console.log(`client ${n}: no desk answer after ${Math.round((Date.now() - started) / 1000)} s: ${String(aim.detail).slice(0, 400)}`);
        wait(4000);
      }
    } while (!selected() && Date.now() - started < 30000);
    const waited = Math.round((Date.now() - started) / 1000);
    // What the suspect sees when the row is offered: the desk and the interaction prompt.
    if (selected() && label) shot(n, label);
    for (let i = 0; i < 3 && !want.test((/ name=(.*?) menu=/.exec(aim.detail) ?? [0, ""])[1]); i++) {
      on(n, { verb: "interact", step: "next" }, 8000);
      wait(700);
      aim = on(n, { verb: "aim", words: [] }, 10000);
    }
    if (beforeConfirm) beforeConfirm();
    const r = on(n, { verb: "interact", step: "confirm" }, 8000);
    wait(3500);
    on(n, { verb: "input", action: "CharacterPitch", value: 0 }, 8000);
    return { ...r, aim: String(aim.detail), waited };
  };
  const PAY = new RegExp(MONEY);
  const TIME = /min/;
  const lastOffer = () => (serverLog().match(/booking offer: .*/g) ?? ["none"]).slice(-1)[0];
  const toastOf = (detail) => (/toast=(.*?)( dispatches=|$)/.exec(detail ?? "") ?? [0, ""])[1];
  const money = (s) => (Number(s.cash) || 0) + (Number(s.bank) || 0);

  const flagship = JSON.parse(readFileSync(join(ROOT, "server", "flagship", "config.json"), "utf8"));
  const ownerFile = join(sessionPaths(dir).server, "profile", "LifeFramework", "config.json");
  const applyConfig = (graceMinutes) => {
    const c = structuredClone(flagship);
    if (graceMinutes !== undefined) c.grace = { minutes: graceMinutes };
    mkdirSync(dirname(ownerFile), { recursive: true });
    writeFileSync(ownerFile, JSON.stringify(c, null, 2));
    on(1, { verb: "configreload" }, 10000);
    wait(3500);
  };

  const DESK = "Fit_saint_philippe_police_police_kit_duty_desk";
  const BOOKING = "Fit_saint_philippe_police_police_kit_booking_desk";
  const TILL = "Fit_saint_philippe_fuel_till";
  const CLERK = "Fit_saint_philippe_fuel_clerk";
  const PISTOL_NOTE = "pistol";

  for (const n of [1, 2]) arrive(on, n, wait);
  for (const n of [1, 2]) { wait(3000); on(n, { verb: "click", widget: "WelcomeGuideDismissButton" }, 10000); }
  wait(8000);
  if (!requireBodies(on, wait, [1, 2])) return 1;
  applyConfig(0);
  const b = state(2).player;
  const suspect = `Player${b}`;

  // A joins the police and goes on duty.
  on(1, { verb: "stand", entityName: DESK });
  wait(2500);
  on(1, { verb: "look" });
  const open = on(1, { verb: "use", className: "LF_PoliceApplyAction", entityName: DESK }, 10000);
  wait(1500);
  on(1, { verb: "type", widget: "PolAppName", text: "Ada_Newcomer" });
  on(1, { verb: "type", widget: "PolAppWhy", text: "Keep_Saint_Philippe_safe" });
  on(1, { verb: "type", widget: "PolAppAvailability", text: "Evenings" });
  on(1, { verb: "click", widget: "PolAppSubmit" });
  wait(3000);
  // cannot_show: the account is already on the roster from an earlier run in this profile, so the form is not offered.
  step("0a A signs up at the duty desk (approved on submit)", (open.status === "ok" && /approved, added to the police roster/.test(serverLog())) || (open.status === "error" && /cannot_show/.test(String(open.detail))), `apply=${open.status} ${open.detail}`);
  on(1, { verb: "look" });
  // A second run in one session finds A on duty already; the clock would take them off.
  if (state(1).duty !== "1") on(1, { verb: "use", className: "LF_DutyAction", entityName: DESK }, 10000);
  wait(3000);
  const onDuty = state(1);
  step("0b A clocks in", onDuty.duty === "1", `duty=${onDuty.duty}`);

  // B holds a firearm permit, so the weapon-theft charge has a licence to take (#4662).
  on(2, { verb: "licence", name: "FIREARM_PERMIT" }, 10000);
  wait(2500);
  step("0c B is granted a firearm permit", /granted FIREARM_PERMIT/.test(serverLog()), "no '/licence: ... granted FIREARM_PERMIT' line in the server log");

  // Raise B's hands and cuff them where they stand.
  const cuffB = (tag) => {
    on(1, { verb: "stand", entityName: suspect });
    wait(3000);
    on(1, { verb: "handsup", words: ["offer", String(b)] }, 10000);
    wait(1000);
    on(2, { verb: "handsup", words: ["choose", "1"] }, 10000);
    wait(1500);
    // The duty kit is issued once per account: an officer who clocks in again after a reset has no cuffs (NO_CUFFS), so
    // the proof holds them, grants a pair through the harness when the pack has none, and only then cuffs (rc-proof-order).
    const cuffs = ensureCuffs(on, wait, 1);
    const held = { status: cuffs.ok ? "ok" : "error", detail: `cuffs ${cuffs.moves.join(",")}: ${cuffs.last.detail}` };
    wait(2500);
    on(1, { verb: "cuffing", playerId: Number(b), kind: "cuff", mode: "pin" }, 10000);
    wait(2000);
    on(1, { verb: "cuffing", playerId: Number(b), kind: "cuff", mode: "finish" }, 10000);
    wait(2500);
    const cuffed = state(2).cuffed;
    step(`${tag} A cuffs B`, held.status === "ok" && cuffed === "1", `hold=${held.status} ${held.detail}; cuffed=${cuffed}; refusal: ${(/restrain refused: [^\n]*/.exec(serverLog()) ?? ["none"])[0]}`);
  };

  // Both walk to the booking desk beside the cell gate (the harness cannot drive the keys: `stand` puts each beside it).
  const toDesk = () => {
    on(1, { verb: "stand", entityName: BOOKING, side: "e" });
    wait(2500);
    on(2, { verb: "stand", entityName: BOOKING, side: "w" });
    wait(3000);
  };

  // A opens the book from the desk, ticks the first row and signs.
  const bookAndSign = (tag) => {
    on(1, { verb: "look" });
    const lookA = on(1, { verb: "look" }, 10000);
    step(`${tag} on duty A is offered "Book suspect" at the desk`, lookA.detail.includes("LF_BookingDeskBookAction"), lookA.detail);
    const book = on(1, { verb: "use", className: "LF_BookingDeskBookAction", entityName: BOOKING }, 10000);
    const sheet = waitText(1, "Sign", 12000);
    step(`${tag} the charge sheet opens on A's screen`, book.status === "ok" && sheet.status === "ok", `use=${book.status} ${book.detail}; wait=${sheet.status} ${sheet.detail}`);
    const ui = on(1, { verb: "ui", mode: "text" }, 10000);
    on(1, { verb: "click", widget: "ChargeSheetRow0_Action" }, 10000);
    wait(800);
    shot(1, `${tag}_sheet_ticked`);
    on(1, { verb: "ui", mode: "text" }, 10000);
    on(1, { verb: "click", widget: "ChargeSheetSign" }, 10000);
    wait(2500);
    return ui.detail;
  };

  // ---------- Path 1: pay now ----------
  const payPath = () => {
  if (order === "cell-first") { on(2, { verb: "licence", name: "FIREARM_PERMIT" }, 10000); wait(2500); }
  const charge = on(2, { verb: "bookcharge", words: ["weapontheft"] }, 10000);
  step("1a B is charged with weapon theft and wanted", charge.status === "ok" && /wanted=[1-9]/.test(charge.detail), `${charge.status} ${charge.detail}`);
  cuffB("1b");
  toDesk();
  const seizedBefore = logCount(/seized \d+ licence/g);
  const before = state(2);
  bookAndSign("1c");
  const offerLine = lastOffer();
  const fine = parseMoney((new RegExp(String.raw`fine=\$(${AMOUNT})`).exec(offerLine) ?? [0, 0])[1]);
  step("1d the signed sheet made an offer with a fine to pay", /payable=1/.test(offerLine) && fine > 0, `offer line: ${offerLine}`);
  const lookB = on(2, { verb: "look" }, 10000);
  step("1e B's action list at the desk shows both answers", (lookB.detail.match(/LF_BookingDeskAnswerAction/g) ?? []).length >= 2, lookB.detail);
  shot(2, "1_offer_pay_and_time");
  // Paying froze the suspect's client for about 20 s (#4824): `framegap` reads the longest wall time between two
  // harness polls (400 ms apart) from the confirm key until the pay has settled. The window starts at the
  // confirm, not before the aim sweep: the sweep takes a `capture ui` screenshot, the harness's own work, and a
  // capture stalls a client past 1 s on a loaded box (4 of 12 logged gaps in one 2026-11-27 session sat on a
  // "Screenshot successfully saved" line). Dry run 5's 3616 ms gap fell in the old window, which held that
  // shot. The sweep's gap is still printed, so a stall there shows, but it is not the pay's.
  on(2, { verb: "framegap" }, 10000);
  const pay = answer(2, PAY, "1_answer_in_view", () => {
    const pre = on(2, { verb: "framegap" }, 20000);
    console.log(`client 2: longest frame gap across the aim sweep and shot (not the pay) ${pre.status === "ok" ? pre.detail : pre.status}`);
  });
  const after = state(2);
  wait(3000);
  // A client can answer no_session for a moment; ask again, and an answer that never comes is its own failure.
  let gapReply = "";
  for (let i = 0; i < 6 && !/max=\d+/.test(gapReply); i++) {
    const r = on(2, { verb: "framegap" }, 20000);
    if (r.status === "ok") gapReply = String(r.detail); else wait(2000);
  }
  const payGap = Number((/max=(\d+)/.exec(gapReply) ?? [0, NaN])[1]);
  console.log(`client 2: longest frame gap across the pay ${Number.isNaN(payGap) ? "unknown (no framegap answer)" : `${payGap} ms`}`);
  step("1f0 B's game keeps running while paying: no frame gap of 1 s or more", payGap < 1000, Number.isNaN(payGap) ? "no framegap answer" : `longest gap ${payGap} ms`);
  step("1f B pays: cash and bank drop by exactly the fine", pay.status === "ok" && money(before) - money(after) === fine, `use=${pay.status}; ${money(before)} -> ${money(after)}, fine $${fine}`);
  step("1g paid: the cuffs come off and the wanted level clears", after.cuffed === "0" && Number(after.wanted) === 0, `cuffed=${after.cuffed} wanted=${after.wanted}`);
  const lines = serverLog();
  step("1h the fine goes to the treasury sink, logged", new RegExp(String.raw`treasury \+\$${AMOUNT} \(fine from`).test(lines), "no 'treasury +$' line");
  step("1i the signed sheet seized the firearm permit", logCount(/seized [1-9]\d* licence/g) > seizedBefore || /charge sheet of [^\n]* seized [1-9]/.test(lines), `seized lines: ${(lines.match(/charge sheet of [^\n]*seized \d+ licence[^\n]*/g) ?? ["none"]).slice(-1)[0]}`);
  const seen = on(2, { verb: "seen" }, 10000);
  step("1j B reads the licence-seized toast (the last of the booking toasts)", /LicenceSeized|[Pp]aid/.test(toastOf(seen.detail)), `toast: ${toastOf(seen.detail)}`);
  wait(5000);
  const lookPaid = on(2, { verb: "look" }, 10000);
  step("1k once paid, the desk offers B no answer rows", !lookPaid.detail.includes("LF_BookingDeskAnswerAction"), lookPaid.detail.slice(0, 300));
  shot(2, "1_paid_toast");
  wait(1500);
  };

  // ---------- Path 2: the cell ----------
  const cellPath = () => {
  const scene = (arg, value) => on(2, { verb: "dialogue", arg, value }, 10000).detail;
  if (cellCrime === "till") {
  on(2, { verb: PISTOL_NOTE }, 10000);
  wait(1500);
  on(2, { verb: "hold", item: "weapon" }, 10000);
  wait(2500);
  on(2, { verb: "stand", entityName: TILL, side: "s" });
  wait(2500);
  on(2, { verb: "look" });
  on(2, { verb: "use", className: "LF_DialogueTalkAction", entityName: CLERK }, 10000);
  // Wait for the reply list each time (a slow client misses fixed sleeps), as a player reads before answering.
  const choosing = () => { h.setBoxes([clientBox(dir, 2)]); const r = h.waitForDialogue("choosing", undefined, 30000); console.log(`client 2: dialogue-wait choosing -> ${r.status} ${String(r.detail).slice(0, 120)}`); return r; };
  choosing(); scene("pick", "2"); wait(600);
  choosing(); scene("pick", "0"); wait(1500);
  wait(34000);
  on(2, { verb: "input", action: "MenuBack" });
  wait(3000);
  } else {
    on(2, { verb: "bookcharge", words: ["robbery"] }, 10000);
    wait(1500);
  }
  const wanted = Number(state(2).wanted) || 0;
  step("2a B robs the fuel till and is wanted", wanted > 0, `wanted=${wanted}`);
  on(2, { verb: "hold", item: "none" }, 10000);
  wait(2500);
  cuffB("2b");
  toDesk();
  bookAndSign("2c");
  const lookB2 = on(2, { verb: "look" }, 10000);
  step("2d the jail-only robbery sheet offers only the cell", (lookB2.detail.match(/LF_BookingDeskAnswerAction/g) ?? []).length === 1 && /payable=0/.test(lastOffer()), lookB2.detail.slice(0, 200));
  shot(2, "2_offer_time_only");
  const take = answer(2, TIME, "2_answer_in_view");
  wait(500);
  const j1 = Number(state(2).jail) || 0;
  wait(12000);
  const j2 = Number(state(2).jail) || 0;
  step("2e B takes the time: in the cell and the sentence counts down", take.status === "ok" && j1 > 0 && j2 < j1, `use=${take.status} ${take.detail}; jail ${j1} -> ${j2}; last aim: ${take.aim.slice(0, 200)}`);
  // #4803: after the till robbery the answer is offered on the first look round, not a minute and a half later.
  step("2g after the till robbery B is offered the answer at once (under 15 s)", take.waited <= 15, `waited ${take.waited} s; last aim: ${take.aim.slice(0, 300)}`);
  const seen2 = on(2, { verb: "seen" }, 10000);
  shot(2, "2_in_cell");
  step("2f B has a toast to read after the booking", toastOf(seen2.detail) !== "" && toastOf(seen2.detail) !== "none", `toast: ${toastOf(seen2.detail)}`);
  };

  if (order === "cell-first") {
    cellPath();
    // B leaves the cell a free civilian (sentence, cuffs and wanted cleared) for the pay-now booking.
    on(2, { verb: "resetplayer", cash: 400 }, 10000);
    wait(3000);
    payPath();
  } else {
    payPath();
    cellPath();
  }

  const kept = serverLog().split(/\r?\n/).filter((l) => /LFDebug:(Law|License|Police|HandsUp|Duty|Jail|Wanted|Rob|AgentHarness)/.test(l));
  mkdirSync(join(ROOT, "tmp"), { recursive: true });
  writeFileSync(join(ROOT, "tmp", "booking-desk-server.txt"), kept.join("\n"));
  const failed = steps.filter((s) => !s.ok);
  console.log("");
  for (const s of steps) console.log(`${s.ok ? "PASS" : "FAIL"}  ${s.name}`);
  console.log(failed.length ? `booking desk proof: FAIL (${failed.length} of ${steps.length})` : `booking desk proof: PASS (${steps.length} steps)`);
  return failed.length ? 1 : 0;
}

export default {
  name: "booking-desk-proof",
  usage: "booking-desk-proof [--name <shot prefix>]",
  run: (ctx) => runBookingDeskProof(),
};

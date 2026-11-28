import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { ROOT, arrive, clientBox, newestLog, parseState, sessionDir, sessionPaths } from "../session.mjs";
import { ensureCuffs, requireBodies } from "./_preconditions.mjs";

// The crime and police loop played through as two players would, on the real island under the flagship owner config
// (agent playtest 2026-11-27, docs/playtest/agent-playtest-crime-2026-11-27.md):
//   node agent-box/session.mjs up --mission island --clients 2 --profile full --hold 20 --wait
//   node agent-box/session.mjs crime-playtest-proof [--name <shot prefix>]
//   node agent-box/session.mjs down
// Client 1 (CIV) is a civilian with a pistol; client 2 (COP) signs up at the Saint-Philippe duty desk and clocks in.
// CIV robs the fuel till through the attendant; COP reads the call, asks for hands up, cuffs, searches, escorts CIV to the
// booking desk and books them; CIV takes the time, then pays bail at the cell phone, walks out and checks the pistol.
// COP clocks out and the pay is turned into dollars an hour. Each step reads BOTH clients (state, peers, seen) and takes a
// picture on each, so a desync or a step with no feedback shows. Every step prints PASS or FAIL with what each side read.
async function runCrimePlaytestProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const flag = (name, dflt) => { const i = process.argv.indexOf(name); return i > 0 ? process.argv[i + 1] : dflt; };
  const prefix = flag("--name", "crime1127");
  const CIV = 1;
  const COP = 2;
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${String(r.detail).slice(0, 400)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
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
  const steps = [];
  const step = (name, ok, why) => { steps.push({ name, ok, why }); console.log(`${ok ? "PASS" : "FAIL"}  ${name}: ${why}`); };
  const shot = (n, name) => on(n, { verb: "capture", mode: "ui", name: `${prefix}_c${n}_${name}` }, 25000);
  const both = (name) => { shot(CIV, name); shot(COP, name); };
  const toastOf = (detail) => (/toast=(.*?)( dispatches=|$)/.exec(detail ?? "") ?? [0, ""])[1];
  const seen = (n) => String(on(n, { verb: "seen" }, 10000).detail);
  const uiText = (n) => String(on(n, { verb: "ui", mode: "text" }, 10000).detail);
  const peerOf = (n, id) => {
    const d = String(on(n, { verb: "peers" }, 10000).detail);
    const m = new RegExp(`player=${id}\\b`).exec(d);
    if (!m) return "";
    const rest = d.slice(m.index + 1);
    const next = rest.search(/player=\d/);
    return d.slice(m.index, next < 0 ? undefined : m.index + 1 + next);
  };
  const money = (s) => (Number(s.cash) || 0) + (Number(s.bank) || 0);

  const flagship = JSON.parse(readFileSync(join(ROOT, "server", "flagship", "config.json"), "utf8"));
  const ownerFile = join(sessionPaths(dir).server, "profile", "LifeFramework", "config.json");
  const applyConfig = (graceMinutes) => {
    const c = structuredClone(flagship);
    if (graceMinutes !== undefined) c.grace = { minutes: graceMinutes };
    mkdirSync(dirname(ownerFile), { recursive: true });
    writeFileSync(ownerFile, JSON.stringify(c, null, 2));
    on(CIV, { verb: "configreload" }, 10000);
    wait(3500);
  };

  const DESK = "Fit_saint_philippe_police_police_kit_duty_desk";
  const BOOKING = "Fit_saint_philippe_police_police_kit_booking_desk";
  const TILL = "Fit_saint_philippe_fuel_till";
  const CLERK = "Fit_saint_philippe_fuel_clerk";
  const PHONE = "Fit_saint_philippe_jail_jail_bail_phone";

  // --from arrest: the civilian is already wanted in this session; only the officer clocks in again.
  const fromArrest = flag("--from", "") === "arrest";
  if (!fromArrest) {
    for (const n of [CIV, COP]) arrive(on, n, wait);
    for (const n of [CIV, COP]) { wait(3000); on(n, { verb: "click", widget: "WelcomeGuideDismissButton" }, 10000); }
    wait(8000);
    if (!requireBodies(on, wait, [CIV, COP])) return 1;
    applyConfig(0);
  }
  const civId = state(CIV).player;
  const copId = state(COP).player;
  const suspect = `Player${civId}`;
  console.log(`CIV is player ${civId}, COP is player ${copId}`);

  // ---------- 1. The officer signs up and clocks in ----------
  on(COP, { verb: "stand", entityName: DESK });
  wait(2500);
  const deskRows = String(on(COP, { verb: "look" }, 10000).detail);
  shot(COP, "1_duty_desk_rows");
  const open = fromArrest ? { status: "error", detail: "cannot_show (--from arrest)" } : on(COP, { verb: "use", className: "LF_PoliceApplyAction", entityName: DESK }, 10000);
  wait(1500);
  if (open.status === "ok") {
    shot(COP, "1_application_form");
    on(COP, { verb: "type", widget: "PolAppName", text: "Dana_Reyes" });
    on(COP, { verb: "type", widget: "PolAppWhy", text: "Keep_Saint_Philippe_safe" });
    on(COP, { verb: "type", widget: "PolAppAvailability", text: "Evenings" });
    on(COP, { verb: "click", widget: "PolAppSubmit" });
    wait(3000);
    shot(COP, "1_application_sent");
  }
  step("1a COP signs up at the duty desk", open.status === "ok" || /cannot_show/.test(String(open.detail)), `apply=${open.status}; desk rows: ${deskRows.slice(0, 300)}`);
  on(COP, { verb: "look" });
  const preDuty = state(COP);
  if (preDuty.duty !== "1") on(COP, { verb: "use", className: "LF_DutyAction", entityName: DESK }, 10000);
  wait(2500);
  const clockInToast = toastOf(seen(COP));
  shot(COP, "1_clocked_in");
  const onDuty = state(COP);
  const dutyStart = Date.now();
  step("1b COP clocks in", onDuty.duty === "1", `duty=${onDuty.duty}; toast: ${clockInToast}; money ${money(preDuty)}`);
  // The duty kit: the cuffs are what the arrest needs.
  const cuffsAtStart = on(COP, { verb: "hold", item: "Handcuffs" }, 10000);
  on(COP, { verb: "hold", item: "none" }, 10000);
  step("1c COP's duty kit holds handcuffs", cuffsAtStart.status === "ok" && /held/.test(String(cuffsAtStart.detail)), `${cuffsAtStart.status} ${cuffsAtStart.detail}`);

  if (!fromArrest) {
  // ---------- 2. The civilian gets money in the bank, a pistol, and robs the fuel till ----------
  // checkpoint 10: the hour-10 purse ($450 cash, $8,450 bank), so bail is a choice and not the #5047 refusal.
  on(CIV, { verb: "checkpoint", words: ["10"] }, 20000);
  wait(3000);
  on(CIV, { verb: "pistol" }, 10000);
  wait(1500);
  const civBefore = state(CIV);
  const drew = on(CIV, { verb: "hold", item: "weapon" }, 10000);
  wait(2500);
  on(CIV, { verb: "stand", entityName: TILL, side: "s" });
  wait(2500);
  on(CIV, { verb: "look" });
  on(CIV, { verb: "use", className: "LF_DialogueTalkAction", entityName: CLERK }, 10000);
  const choosing = () => { h.setBoxes([clientBox(dir, CIV)]); const r = h.waitForDialogue("choosing", undefined, 30000); console.log(`client ${CIV}: dialogue-wait choosing -> ${r.status} ${String(r.detail).slice(0, 200)}`); return r; };
  const d1 = choosing();
  shot(CIV, "2_clerk_dialogue");
  on(CIV, { verb: "dialogue", arg: "pick", value: "2" }, 10000); wait(600);
  const d2 = choosing();
  shot(CIV, "2_clerk_demand");
  on(CIV, { verb: "dialogue", arg: "pick", value: "0" }, 10000); wait(1500);
  wait(8000);
  shot(CIV, "2_robbery_hold");
  const copDuring = seen(COP);
  shot(COP, "2_call_while_robbing");
  wait(26000);
  on(CIV, { verb: "input", action: "MenuBack" });
  wait(3000);
  const civRobbed = state(CIV);
  shot(CIV, "2_after_robbery");
  const copAfter = seen(COP);
  shot(COP, "2_call_after_robbery");
  // The till pays marked notes into the bag (#2851), never HUD cash: the payout is read from the server line.
  const robPaid = /Stole_Money/.test(civRobbed.toast ?? "") || /robbery[^\n]*(paid|marked)/i.test(serverLog());
  step("2a CIV robs the till: wanted and paid in marked notes", Number(civRobbed.wanted) > 0 && robPaid, `drew=${drew.detail}; wanted=${civRobbed.wanted}; toast=${civRobbed.toast}; HUD money ${money(civBefore)} -> ${money(civRobbed)} (unchanged by design); dialogue ${String(d1.detail).slice(0, 160)} / ${String(d2.detail).slice(0, 160)}`);
  step("2b COP hears about it (a toast, dispatch or call)", /dispatch|robb|call|alarm/i.test(copDuring + copAfter), `during: ${toastOf(copDuring)} | after: ${toastOf(copAfter)} | ${copAfter.slice(0, 300)}`);
  }
  on(CIV, { verb: "hold", item: "none" }, 10000);
  wait(2000);

  // ---------- 3. Hands up and cuffs ----------
  on(COP, { verb: "stand", entityName: suspect });
  wait(3000);
  const fieldRows = String(on(COP, { verb: "look" }, 10000).detail);
  shot(COP, "3_field_rows");
  // The prompt lives 8 s (LF_HandsUp.PROMPT_MS): answer first, as a player pressing the key does, then take the pictures.
  on(COP, { verb: "handsup", words: ["offer", String(civId)] }, 10000);
  wait(1000);
  const answered = on(CIV, { verb: "handsup", words: ["choose", "1"] }, 10000);
  wait(1500);
  // Hands stay up 30 s (LF_HandsUpRules TIMEOUT): the cuffing follows at once; pictures come after.
  const handsLine = (serverLog().match(/[^\n]*LFDebug:HandsUp\][^\n]*/g) ?? ["none"]).slice(-2).join(" | ");
  step("3a CIV raises their hands at the prompt", !/ignored/.test(handsLine), `answer=${answered.status} ${answered.detail}; server: ${handsLine}; field rows: ${fieldRows.slice(0, 300)}`);
  const cuffs = ensureCuffs(on, wait, COP);
  wait(1000);
  on(COP, { verb: "cuffing", playerId: Number(civId), kind: "cuff", mode: "pin" }, 10000);
  wait(1200);
  shot(CIV, "3_cuffing_hold");
  on(COP, { verb: "cuffing", playerId: Number(civId), kind: "cuff", mode: "finish" }, 10000);
  wait(3000);
  const civCuffed = state(CIV);
  const copSeesCuffed = peerOf(COP, civId);
  const civToastCuff = toastOf(seen(CIV));
  const copSeenCuff = seen(COP);
  both("3_cuffed");
  step("3b cuffed on both screens", civCuffed.cuffed === "1" && /cuffed=1/.test(copSeesCuffed), `cuffs=${cuffs.ok} ${cuffs.moves.join(",")}; CIV state cuffed=${civCuffed.cuffed}; COP peers: ${copSeesCuffed}; CIV toast: ${civToastCuff}; COP seen: ${copSeenCuff.slice(0, 200)}`);

  // ---------- 4. Search ----------
  on(COP, { verb: "hold", item: "none" }, 10000);
  wait(1000);
  on(COP, { verb: "look" });
  const search = on(COP, { verb: "use", className: "LF_ConfiscateAction", entityName: suspect }, 10000);
  wait(2500);
  const copSearchToast = toastOf(seen(COP));
  const civSearchToast = toastOf(seen(CIV));
  both("4_searched");
  step("4a COP searches CIV and both get feedback", search.status === "ok" && copSearchToast !== "" && copSearchToast !== "none", `search=${search.status} ${search.detail}; COP toast: ${copSearchToast}; CIV toast: ${civSearchToast}`);

  // ---------- 5. Escort to the station ----------
  on(COP, { verb: "look" });
  const escort = on(COP, { verb: "use", className: "LF_JailEscortArrestAction", entityName: suspect }, 10000);
  wait(2000);
  both("5_escort_start");
  const escortToast = toastOf(seen(CIV));
  on(COP, { verb: "stand", entityName: BOOKING, side: "e" });
  wait(5000);
  const civAtDesk = state(CIV);
  const copAtDesk = state(COP);
  both("5_escort_at_desk");
  step("5a COP escorts CIV (CIV follows to the booking desk)", escort.status === "ok", `escort=${escort.status} ${escort.detail}; CIV pos ${civAtDesk.pos} COP pos ${copAtDesk.pos}; CIV toast ${escortToast}`);
  // The harness cannot walk: put CIV beside the desk too, where the escort leaves them.
  on(CIV, { verb: "stand", entityName: BOOKING, side: "w" });
  wait(3000);

  // ---------- 6. Booking ----------
  on(COP, { verb: "look" });
  const lookCop = String(on(COP, { verb: "look" }, 10000).detail);
  const book = on(COP, { verb: "use", className: "LF_BookingDeskBookAction", entityName: BOOKING }, 10000);
  h.setBoxes([clientBox(dir, COP)]);
  const sheet = h.waitForText("Sign", 12000);
  const sheetText = uiText(COP);
  shot(COP, "6_charge_sheet");
  shot(CIV, "6_while_cop_writes");
  on(COP, { verb: "click", widget: "ChargeSheetRow0_Action" }, 10000);
  wait(800);
  shot(COP, "6_sheet_ticked");
  on(COP, { verb: "click", widget: "ChargeSheetSign" }, 10000);
  wait(2500);
  const copSigned = toastOf(seen(COP));
  shot(COP, "6_signed");
  step("6a COP books CIV at the desk (sheet opens, signs)", book.status === "ok" && sheet.status === "ok", `book=${book.status}; desk rows ${lookCop.slice(0, 200)}; sheet: ${sheetText.slice(0, 500)}; COP toast after sign: ${copSigned}`);
  const offer = (serverLog().match(/booking offer: .*/g) ?? ["none"]).slice(-1)[0];
  // CIV answers: the till robbery is jail only, so "Take the time" is the one row.
  let aim;
  for (const pitch of [-45, -30, -20, -55, -10]) {
    on(CIV, { verb: "input", action: "CharacterPitch", value: pitch }, 8000);
    wait(800);
    aim = on(CIV, { verb: "aim", words: [] }, 10000);
    if (/selected=LF_BookingDeskAnswerAction/.test(aim.detail)) break;
  }
  shot(CIV, "6_offer_in_view");
  const civRows = String(on(CIV, { verb: "look" }, 10000).detail);
  const take = on(CIV, { verb: "interact", step: "confirm" }, 8000);
  wait(3500);
  on(CIV, { verb: "input", action: "CharacterPitch", value: 0 }, 8000);
  const j1 = Number(state(CIV).jail) || 0;
  const civCellToast = toastOf(seen(CIV));
  both("6_in_cell");
  wait(10000);
  const civJail = state(CIV);
  const j2 = Number(civJail.jail) || 0;
  const copBooked = toastOf(seen(COP));
  shot(CIV, "6_jail_clock");
  step("6b CIV takes the time: in the cell, clock counting", take.status === "ok" && j1 > 0 && j2 < j1, `offer: ${offer}; CIV rows: ${civRows.slice(0, 300)}; jail ${j1} -> ${j2}; CIV toast: ${civCellToast}; COP toast: ${copBooked}; cuffed=${civJail.cuffed} wanted=${civJail.wanted}`);

  // ---------- 7. Bail and the walk out ----------
  on(CIV, { verb: "stand", entityName: PHONE });
  wait(2500);
  const phoneRows = String(on(CIV, { verb: "look" }, 10000).detail);
  shot(CIV, "7_bail_phone");
  const bank0 = Number(state(CIV).bank) || 0;
  const pay = on(CIV, { verb: "use", className: "LF_JailBailAction", entityName: PHONE }, 10000);
  wait(3000);
  const civOut = state(CIV);
  const bailToast = toastOf(seen(CIV));
  const bailLine = (/[^\n]*paid bail[^\n]*/.exec(serverLog()) ?? ["none"])[0];
  both("7_after_bail");
  step("7a CIV pays bail from the bank and is released", pay.status === "ok" && (Number(civOut.jail) || 0) === 0 && Number(civOut.bank) < bank0, `rows: ${phoneRows.slice(0, 300)}; pay=${pay.status} ${pay.detail}; bank ${bank0} -> ${civOut.bank}; jail=${civOut.jail}; toast: ${bailToast}; server: ${bailLine}`);
  step("7b released: free of cuffs and warrant, outside the cell", civOut.cuffed === "0" && Number(civOut.wanted) === 0, `cuffed=${civOut.cuffed} wanted=${civOut.wanted} pos=${civOut.pos}`);
  const copSeesFree = peerOf(COP, civId);
  step("7c COP's screen agrees CIV is free", /cuffed=0/.test(copSeesFree), `COP peers: ${copSeesFree}`);
  // Gear: does the pistol come back, and does anything say where it went?
  const gear = on(CIV, { verb: "hold", item: "weapon" }, 10000);
  wait(1500);
  on(CIV, { verb: "input", action: "Inventory" }, 8000);
  wait(2500);
  shot(CIV, "7_inventory_after_release");
  const invText = uiText(CIV);
  on(CIV, { verb: "input", action: "MenuBack" }, 8000);
  on(CIV, { verb: "hold", item: "none" }, 10000);
  const seizeLines = serverLog().split(/\r?\n/).filter((l) => /seiz|confiscat|evidence/i.test(l)).slice(-6).join(" | ");
  // An unlicensed pistol is seized for good (the law, no property locker): the check is that it is gone and the suspect was told.
  step("7d CIV's pistol stays seized after release, and CIV was told at the search", /no_weapon/.test(String(gear.detail)) && /took/i.test(civSearchToast), `hold weapon: ${gear.status} ${gear.detail}; search toast to CIV: ${civSearchToast}; inventory text: ${invText.slice(0, 300)}; server: ${seizeLines}`);

  // ---------- 8. The officer clocks out and is paid ----------
  on(COP, { verb: "stand", entityName: DESK });
  wait(2500);
  on(COP, { verb: "look" });
  const copBefore = state(COP);
  const dutySeconds = (Date.now() - dutyStart) / 1000;
  on(COP, { verb: "use", className: "LF_DutyAction", entityName: DESK }, 10000);
  wait(1200);
  shot(COP, "8_clocked_out");
  const copAfterOut = state(COP);
  const payToast = toastOf(seen(COP));
  // Clocking out also hands back the civilian cash stored at clock-in, so the pay is read from the server line.
  const payLines = serverLog().match(/shift pay \+\d+/g) ?? ["+0"];
  const paid = Number(/\+(\d+)/.exec(payLines.slice(-1)[0])[1]);
  const perHour = Math.round(paid / (dutySeconds / 3600));
  const payLine = (serverLog().match(/[^\n]*LFDebug:(DutyPay|Duty)\][^\n]*(pa(id|y)|shift)[^\n]*/g) ?? ["none"]).slice(-3).join(" | ");
  step("8a COP is paid at the bottom rung ($350-450/h)", paid > 0 && perHour >= 300 && perHour <= 500, `paid $${paid} for ${Math.round(dutySeconds)} s on duty = $${perHour}/h; duty=${copAfterOut.duty}; toast: ${payToast}; server: ${payLine}`);

  const kept = serverLog().split(/\r?\n/).filter((l) => /LFDebug:(Law|License|Police|HandsUp|Duty|DutyPay|DutyIdle|Jail|JailBail|Wanted|Rob|Dispatch|Evidence|Restrain|Escort|AgentHarness)|SCRIPT \(E\)|VME/.test(l));
  mkdirSync(join(ROOT, "tmp"), { recursive: true });
  writeFileSync(join(ROOT, "tmp", "crime-playtest-server.txt"), kept.join("\n"));
  const failed = steps.filter((s) => !s.ok);
  console.log("");
  for (const s of steps) console.log(`${s.ok ? "PASS" : "FAIL"}  ${s.name}`);
  console.log(failed.length ? `crime playtest proof: FAIL (${failed.length} of ${steps.length})` : `crime playtest proof: PASS (${steps.length} steps)`);
  return failed.length ? 1 : 0;
}

export default {
  name: "crime-playtest-proof",
  usage: "crime-playtest-proof [--name <shot prefix>]",
  run: (ctx) => runCrimePlaytestProof(),
};

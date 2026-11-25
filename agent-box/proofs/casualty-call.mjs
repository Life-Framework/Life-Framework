import { readFileSync } from "node:fs";
import { join } from "node:path";
import { arrive, clientBox, newestLog, parseState, sessionDir, sessionPaths } from "../session.mjs";

// #4830: one medic hour on the island, played. Clock in at the Saint-Philippe clinic, wait for the
// casualty roll (first one about 10 s after clock-in), read the medic board, take the call, draw the
// ambulance from the car pool, stand at the casualty, treat them, clock out. Each step is judged from
// the server log line the feature writes and the report prints the seconds each took.
//   node agent-box/session.mjs up --mission island --profile full
//   node agent-box/session.mjs casualty-call [--skip-treat] [--no-carry]
//   node agent-box/session.mjs down
// --skip-treat is the red run: the medic never treats, so the "treated" and the bonus steps fail.
// The first casualty is the natural roll (or `casualty roll` when none came, #4853). After the treat a second one is rolled
// with `casualty roll Dock`, loaded into the drawn ambulance, which `parkat` moves to the clinic bed (the harness cannot
// drive), and put in the bed for the $15 delivery. --no-carry stops after the treat.
const DESK = "Fit_saint_philippe_clinic_lf_medic_duty_desk";
const BOARD = "Fit_saint_philippe_clinic_lf_dispatch_board_medic";
// The place entity of each row kind the proof can stand beside (the casualty lies within the spot radius of it).
const PLACE_ENTITY = { Dock: "Fit_saint_philippe_dock_dock_Place" };
const POOL = "Fit_saint_philippe_clinic_medic_fleet_board";
const BED = "Fit_saint_philippe_clinic_lf_hospital_bed";

// The first casualty row the log holds after `from`: { uid, summary, kind, at: [x, y, z], open, line }.
export function parseCasualty(lines, from = 0) {
  for (let i = from; i < lines.length; i++) {
    const m = /\[LFDebug:CasualtyCall\] casualty (\S+) '([^']*)' at (\S+) \(<?\s*(-?[\d.]+)[ ,]+(-?[\d.]+)[ ,]+(-?[\d.]+)\s*>?\); (\d+) open/.exec(lines[i]);
    if (m) return { uid: m[1], summary: m[2], kind: m[3], at: [Number(m[4]), Number(m[5]), Number(m[6])], open: Number(m[7]), line: i };
  }
  return null;
}

// The reply of `casualty roll`: "casualty:<n> '<summary>' at <(x, y, z)> expires_in=<s>". null when it is not one.
export function parseRollReply(detail) {
  const m = /(casualty:\d+) '([^']*)' at <?\s*\(?\s*(-?[\d.]+)[ ,]+(-?[\d.]+)[ ,]+(-?[\d.]+)\s*\)?>?\s+expires_in=(-?\d+)/.exec(String(detail));
  if (!m) return null;
  return { uid: m[1], summary: m[2], at: [Number(m[3]), Number(m[4]), Number(m[5])], expiresIn: Number(m[6]) };
}

// Index of the first line at or after `from` holding every needle, or -1.
export function findLine(lines, from, ...needles) {
  for (let i = from; i < lines.length; i++) if (needles.every((n) => lines[i].includes(n))) return i;
  return -1;
}

// How the duty bonus of `reason` went at or after line `from`: "booked" (a `bonus +$ (<reason>)` line), "capped" (the
// hourly cap of LF_DutyBonus was already reached, so a re-run in the same game hour books nothing; the server then logs
// `bonus capped for <id>`) or "none" (the server decided nothing: a real failure).
export function bonusOutcome(lines, from, reason) {
  if (findLine(lines, from, "[LFDebug:DutyBonus]", "bonus +$", reason) >= 0) return "booked";
  if (findLine(lines, from, "[LFDebug:DutyBonus]", "bonus capped for") >= 0) return "capped";
  return "none";
}

// The ordered judgement over the server lines after the baseline. Pure. Returns the failures.
export function judgeCasualty(lines, { treat = true } = {}) {
  const fails = [];
  const c = parseCasualty(lines);
  if (!c) return ["call: no casualty was rolled after the clock-in"];
  const taken = findLine(lines, c.line, "[LFDebug:ServiceBoard]", "on the medic board taken by");
  if (taken < 0) fails.push("take: the medic board logged no take of the call");
  if (treat) {
    const treated = findLine(lines, c.line, `casualty ${c.uid} treated by medic`);
    if (treated < 0) fails.push(`treat: ${c.uid} was never treated`);
    if (findLine(lines, c.line, "[LFDebug:ServiceBoard]", "on the medic board closed", c.uid) < 0) fails.push(`board: the medic board line of ${c.uid} did not close on the treat`);
    if (findLine(lines, c.line, "[LFDebug:ServiceCalls]", "closed: map mark", "removed") < 0) fails.push(`map: the dispatch map mark of ${c.uid} did not close on the treat`);
    const bonus = bonusOutcome(lines, c.line, "REVIVE");
    if (bonus === "none") fails.push("bonus: no revive bonus was booked or capped for the treatment");
    // A capped bonus adds nothing to the payslip: the pay line is only required when a bonus was booked.
    const paid = findLine(lines, Math.max(c.line, 0), "[LFDebug:DutyPay]", "duty bonus +");
    if (bonus === "booked" && paid < 0) fails.push("payslip: clock-out paid no duty bonus");
  }
  const shift = findLine(lines, c.line, "[LFDebug:DutyPay]", "shift pay +");
  if (shift < 0) fails.push("payslip: clock-out paid no shift pay");
  return fails;
}

// #4863: every casualty the server raised was dressed (a work outfit of at least a torso, trousers and shoes) and no roll
// was refused for want of clear ground. `count` is how many casualties the run raised. Pure. Returns the failures.
export function judgeDressed(lines, count) {
  const fails = [];
  const dressed = lines.map((l) => /\[LFDebug:CasualtyCall\] casualty dressed in (\d+) piece\(s\) of (\S+)/.exec(l)).filter(Boolean);
  if (dressed.length < count) fails.push(`dressed: ${dressed.length} dressed-casualty line(s) for ${count} casualties`);
  for (const m of dressed) if (Number(m[1]) < 3) fails.push(`dressed: only ${m[1]} piece(s) of ${m[2]} went on`);
  if (findLine(lines, 0, "[LFDebug:CasualtyCall]", "no clear ground") >= 0) fails.push("ground: a roll found no clear ground near its place");
  return fails;
}

// #4878: the car pool's reply is the server's log line, not the `use` verb's status (the verb says ok when the draw was
// refused). Fails with the refusal line, which names the vehicle that blocks the bay. Pure. Returns the failures.
export function judgeDraw(lines) {
  const refused = lines.find((l) => l.includes("[LFDebug:MedicCarPool]") && l.includes("draw refused"));
  if (refused) return [`draw: ${refused.slice(refused.indexOf("draw refused"))}`];
  if (!lines.some((l) => l.includes("[LFDebug:MedicCarPool]") && l.includes(" drew "))) return ["draw: the car pool logged no draw"];
  return [];
}

// #4849: a casualty put in a clinic bed closes its call: the bed line, the board line and the map mark. Pure. Returns the failures.
export function judgeBed(lines, uid) {
  const delivered = findLine(lines, 0, "[LFDebug:Medic]", "put in a hospital bed by medic");
  if (delivered < 0) return ["bed: no 'put in a hospital bed' line for the second casualty"];
  const fails = [];
  if (findLine(lines, delivered, "[LFDebug:CasualtyTreat]", `casualty ${uid} put in a bed; call closed`) < 0) fails.push(`bed: ${uid} was put in a bed but its call did not close`);
  if (findLine(lines, delivered, "[LFDebug:ServiceBoard]", "on the medic board closed", uid) < 0) fails.push(`bed: the medic board line of ${uid} did not close on the delivery`);
  if (findLine(lines, delivered, "[LFDebug:ServiceCalls]", "closed: map mark", "removed") < 0) fails.push(`bed: the dispatch map mark of ${uid} did not close on the delivery`);
  return fails;
}

async function runCasualtyCall(args) {
  const treat = !args.includes("--skip-treat");
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${String(r.detail).slice(0, 200)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const logLines = () => {
    const f = newestLog(join(sessionPaths(dir).server, "logs"));
    return f ? readFileSync(f, "utf8").split("\n") : [];
  };
  const t0 = Date.now();
  const stamps = [];
  const mark = (step) => { stamps.push([step, Math.round((Date.now() - t0) / 1000)]); };
  const base = args.includes("--reuse") ? 0 : logLines().length; // --reuse: a casualty already open in this session
  const fresh = () => logLines().slice(base);
  const fails = [];
  const use = (cls, entity, extra = {}) => {
    if (entity) {
      on(1, { verb: "stand", entityName: entity });
      wait(2500);
    }
    on(1, { verb: "look" });
    const spec = { verb: "use", className: cls };
    if (entity) spec.entityName = entity;
    return on(1, { ...spec, ...extra }, 10000);
  };

  arrive(on, 1, wait);
  wait(6000);
  mark("arrived");

  // 1. Clock in at the clinic desk.
  if (!String(parseState(on(1, { verb: "state" }, 10000).detail).faction).includes("MEDIC")) use("LF_DutyAction", DESK);
  wait(2500);
  if (!String(parseState(on(1, { verb: "state" }, 10000).detail).faction).includes("MEDIC")) fails.push("clock in: the faction is not MEDIC after the desk");
  mark("clocked in");

  // 2. The first roll comes about 10 s after the clock-in; wait up to 90 s for the log line.
  let casualty = null;
  for (let i = 0; i < 45 && !casualty; i++) {
    wait(2000);
    casualty = parseCasualty(fresh());
  }
  if (!casualty) {
    const forced = on(1, { verb: "casualty", words: ["roll"] }, 15000);
    wait(2000);
    casualty = parseCasualty(fresh());
    console.log(`note: no natural roll in 90 s; casualty roll returned ${forced.status} ${forced.detail}`);
  }
  if (!casualty) {
    for (const f of ["call: no casualty rolled within 90 s of the clock-in, nor by casualty roll"]) console.log(`FAIL  ${f}`);
    console.log("casualty-call: FAIL");
    return 1;
  }
  mark(`call rolled (${casualty.kind}, '${casualty.summary}')`);
  console.log(`casualty ${casualty.uid} at ${casualty.at.join(", ")}`);

  // 3. The board and the pager.
  on(1, { verb: "stand", entityName: BOARD });
  wait(2500);
  on(1, { verb: "look" });
  on(1, { verb: "use", className: "LF_WantedBoardReadAction", entityName: BOARD }, 10000);
  wait(2500);
  on(1, { verb: "capture", mode: "ui", name: "casualty_board" }, 25000);
  on(1, { verb: "click", widget: "Close" });
  wait(800);
  mark("board read");
  on(1, { verb: "capture", mode: "ui", name: "casualty_pager" }, 25000);
  const take = use("LF_ServiceTakeAction", BOARD);
  if (take.status !== "ok") fails.push(`take: ${take.status} ${take.detail}`);
  wait(1500);
  mark("call taken");

  // 4. The ambulance from the car pool, then the harness `park` stands in for the drive.
  const draw = use("LF_MedicCarPoolDrawAction", POOL);
  if (draw.status !== "ok") console.log(`note: the car pool draw returned ${draw.status} ${draw.detail}`);
  wait(2500);
  fails.push(...judgeDraw(fresh()));
  mark("ambulance drawn");

  // 5. Stand at the casualty body itself (the harness name Casualty, #4853), so the hold is in reach.
  const [x, y, z] = casualty.at;
  const st = on(1, { verb: "stand", entityName: "Casualty" }, 30000);
  if (st.status !== "ok") fails.push(`stand: Casualty returned ${st.status} ${st.detail}`);
  wait(2500);
  on(1, { verb: "capture", mode: "world", name: "casualty_site", x, y: y + 0.4, z, ox: 3, oy: 0.5, oz: -3 }, 25000);
  mark("at the casualty");
  if (treat) {
    on(1, { verb: "stand", entityName: "Casualty" }, 30000); // the shot camera moved the view
    wait(2500);
    on(1, { verb: "look" });
    const rev = on(1, { verb: "use", className: "LF_ReviveAction", entityName: "Casualty" }, 15000);
    if (rev.status !== "ok") fails.push(`treat: the revive returned ${rev.status} ${rev.detail}`);
    wait(11000); // "Treat the patient" holds about 8 s
    mark("treated");
    on(1, { verb: "capture", mode: "world", name: "casualty_treated", x, y: y + 0.4, z, ox: 3, oy: 0.5, oz: -3 }, 25000);
    on(1, { verb: "stand", entityName: BOARD });
    wait(2500);
    on(1, { verb: "look" });
    on(1, { verb: "use", className: "LF_WantedBoardReadAction", entityName: BOARD }, 10000);
    wait(2500);
    on(1, { verb: "capture", mode: "ui", name: "casualty_board_after", }, 25000);
    on(1, { verb: "click", widget: "Close" });
    wait(800);
    mark("board read after the treat");
  }

  // 5b. A second casualty rolled now and carried to a clinic bed for the $15 delivery.
  let carried = false;
  let secondUid = "";
  if (treat && !args.includes("--no-carry")) {
    const second = on(1, { verb: "casualty", words: ["roll", "Dock"] }, 15000);
    const roll2 = parseRollReply(second.detail);
    if (second.status !== "ok" || !roll2) {
      fails.push(`carry: the second casualty roll returned ${second.status} ${second.detail}`);
    } else {
      mark("second casualty rolled");
      secondUid = roll2.uid;
      // The dressed body on clear ground (#4863): the proof's second picture.
      on(1, { verb: "stand", entityName: "Casualty" }, 30000);
      wait(2500);
      on(1, { verb: "capture", mode: "world", name: "casualty_site_2", x: roll2.at[0], y: roll2.at[1] + 0.4, z: roll2.at[2], ox: 3, oy: 0.5, oz: -3 }, 25000);
      const park1 = on(1, { verb: "casualty", words: ["ambulance", "Casualty", "4"] }, 15000);
      if (park1.status !== "ok") fails.push(`carry: the ambulance did not park by the casualty: ${park1.status} ${park1.detail}`);
      const st2 = on(1, { verb: "stand", entityName: "Casualty" }, 30000);
      if (st2.status !== "ok") fails.push(`carry: stand Casualty returned ${st2.status} ${st2.detail}`);
      wait(2500);
      on(1, { verb: "look" });
      const load = on(1, { verb: "use", className: "LF_LoadPatientAction", entityName: "Casualty" }, 15000);
      if (load.status !== "ok") fails.push(`carry: load returned ${load.status} ${load.detail}`);
      wait(6000);
      mark("second casualty loaded");
      const park2 = on(1, { verb: "casualty", words: ["ambulance", BED, "4"] }, 15000);
      if (park2.status !== "ok") fails.push(`carry: the ambulance did not park by the bed: ${park2.status} ${park2.detail}`);
      on(1, { verb: "stand", entityName: BED });
      wait(2500);
      on(1, { verb: "look" });
      const bed = on(1, { verb: "use", className: "LF_BedDeliveryAction", entityName: BED }, 20000);
      if (bed.status !== "ok") fails.push(`carry: the bed delivery returned ${bed.status} ${bed.detail}`);
      wait(5000);
      carried = true;
      mark("second casualty put in a bed");
      on(1, { verb: "capture", mode: "ui", name: "casualty_bed" }, 25000);
      // The delivery closed the call (#4849): the board has no line for it any more.
      on(1, { verb: "stand", entityName: BOARD });
      wait(2500);
      on(1, { verb: "look" });
      on(1, { verb: "use", className: "LF_WantedBoardReadAction", entityName: BOARD }, 10000);
      wait(2500);
      on(1, { verb: "capture", mode: "ui", name: "casualty_board_after_bed" }, 25000);
      on(1, { verb: "click", widget: "Close" });
      wait(800);
      mark("board read after the bed");
    }
  }

  // 6. Clock out: the shift and the bonus are paid.
  use("LF_DutyAction", DESK);
  wait(4000);
  on(1, { verb: "capture", mode: "ui", name: "casualty_payslip" }, 25000);
  mark("clocked out");

  fails.push(...judgeCasualty(fresh(), { treat }));
  if (carried) {
    if (findLine(fresh(), 0, "put in a hospital bed by medic") < 0) fails.push("carry: no 'put in a hospital bed' line for the second casualty");
    if (bonusOutcome(fresh(), 0, "PATIENT_DELIVERED") === "none") fails.push("carry: no delivery bonus was booked or capped");
    fails.push(...judgeBed(fresh(), secondUid));
  }
  fails.push(...judgeDressed(fresh(), carried ? 2 : 1));
  const open = findLine(fresh(), 0, "[LFDebug:ServiceBoard]", "closed") >= 0;
  console.log(`call closed on the board log: ${open}`);
  for (const [s, t] of stamps) console.log(`  ${String(t).padStart(4)} s  ${s}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "casualty-call: FAIL" : "casualty-call: PASS (clock in, call, take, treat, bonus and shift paid at clock-out)");
  return fails.length ? 1 : 0;
}

export default { name: "casualty-call", usage: "casualty-call [--skip-treat] [--no-carry]", run: (ctx) => runCasualtyCall((ctx && ctx.args) || process.argv.slice(3)) };

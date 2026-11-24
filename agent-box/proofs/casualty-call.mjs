import { readFileSync } from "node:fs";
import { join } from "node:path";
import { arrive, clientBox, newestLog, parseState, sessionDir, sessionPaths } from "../session.mjs";

// #4830: one medic hour on the island, played. Clock in at the Saint-Philippe clinic, wait for the
// casualty roll (first one about 10 s after clock-in), read the medic board, take the call, draw the
// ambulance from the car pool, stand at the casualty, treat them, clock out. Each step is judged from
// the server log line the feature writes and the report prints the seconds each took.
//   node agent-box/session.mjs up --mission island --profile full
//   node agent-box/session.mjs casualty-call [--skip-treat]
//   node agent-box/session.mjs down
// --skip-treat is the red run: the medic never treats, so the "treated" and the bonus steps fail.
const DESK = "Fit_saint_philippe_clinic_lf_medic_duty_desk";
const BOARD = "Fit_saint_philippe_clinic_lf_dispatch_board_medic";
// The place entity of each row kind the proof can stand beside (the casualty lies within the spot radius of it).
const PLACE_ENTITY = { Dock: "Fit_saint_philippe_dock_dock_Place" };
const POOL = "Fit_saint_philippe_clinic_medic_fleet_board";

// The first casualty row the log holds after `from`: { uid, summary, kind, at: [x, y, z], open, line }.
export function parseCasualty(lines, from = 0) {
  for (let i = from; i < lines.length; i++) {
    const m = /\[LFDebug:CasualtyCall\] casualty (\S+) '([^']*)' at (\S+) \(<?\s*(-?[\d.]+)[ ,]+(-?[\d.]+)[ ,]+(-?[\d.]+)\s*>?\); (\d+) open/.exec(lines[i]);
    if (m) return { uid: m[1], summary: m[2], kind: m[3], at: [Number(m[4]), Number(m[5]), Number(m[6])], open: Number(m[7]), line: i };
  }
  return null;
}

// Index of the first line at or after `from` holding every needle, or -1.
export function findLine(lines, from, ...needles) {
  for (let i = from; i < lines.length; i++) if (needles.every((n) => lines[i].includes(n))) return i;
  return -1;
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
    const bonus = findLine(lines, c.line, "[LFDebug:DutyBonus]", "bonus +$", "REVIVE");
    if (bonus < 0) fails.push("bonus: no revive bonus was booked for the treatment");
    const paid = findLine(lines, Math.max(c.line, 0), "[LFDebug:DutyPay]", "duty bonus +");
    if (paid < 0) fails.push("payslip: clock-out paid no duty bonus");
  }
  const shift = findLine(lines, c.line, "[LFDebug:DutyPay]", "shift pay +");
  if (shift < 0) fails.push("payslip: clock-out paid no shift pay");
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
    for (const f of ["call: no casualty rolled within 90 s of the clock-in"]) console.log(`FAIL  ${f}`);
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
  mark("ambulance drawn");

  // 5. Stand at the casualty: beside the place entity of its row (the casualty lies within the spot radius of it).
  const [x, y, z] = casualty.at;
  const placeEntity = PLACE_ENTITY[casualty.kind];
  if (placeEntity) {
    on(1, { verb: "stand", entityName: placeEntity });
    wait(2500);
  } else {
    console.log(`note: no place entity known for ${casualty.kind}; the world shot only frames it`);
  }
  on(1, { verb: "capture", mode: "world", name: "casualty_site", x, y: y + 0.4, z, ox: 3, oy: 0.5, oz: -3 }, 25000);
  mark("at the casualty");
  if (treat) {
    on(1, { verb: "look" });
    const rev = on(1, { verb: "use", className: "LF_ReviveAction" }, 10000);
    if (rev.status !== "ok") fails.push(`treat: the revive returned ${rev.status} ${rev.detail}`);
    wait(7000);
    mark("treated");
    on(1, { verb: "capture", mode: "world", name: "casualty_treated", x, y: y + 0.4, z, ox: 3, oy: 0.5, oz: -3 }, 25000);
  }

  // 6. Clock out: the shift and the bonus are paid.
  use("LF_DutyAction", DESK);
  wait(4000);
  on(1, { verb: "capture", mode: "ui", name: "casualty_payslip" }, 25000);
  mark("clocked out");

  fails.push(...judgeCasualty(fresh(), { treat }));
  const open = findLine(fresh(), 0, "[LFDebug:ServiceBoard]", "closed") >= 0;
  console.log(`call closed on the board log: ${open}`);
  for (const [s, t] of stamps) console.log(`  ${String(t).padStart(4)} s  ${s}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "casualty-call: FAIL" : "casualty-call: PASS (clock in, call, take, treat, bonus and shift paid at clock-out)");
  return fails.length ? 1 : 0;
}

export default { name: "casualty-call", usage: "casualty-call [--skip-treat]", run: (ctx) => runCasualtyCall((ctx && ctx.args) || process.argv.slice(3)) };

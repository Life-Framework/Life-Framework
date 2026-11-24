import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { ROOT, arrive, clientBox, newestLog, parseState, sessionDir, sessionPaths } from "../session.mjs";

// "Run plate" (#4425) on the real island, two players on a dedicated server, under the flagship owner config:
//   node agent-box/session.mjs up --mission island --clients 2 --profile full --wait
//   node agent-box/session.mjs run-plate-island-proof [--name <shot prefix>]
//   node agent-box/session.mjs down
// Client 1 (A) buys an S105 at the Saint-Philippe lot and takes it out of the garage with its key. The harness marks it stolen
// (`markstolen`: a hotwire's result with no skill roll, so the STOLEN read is tested on every run); A reports it stolen. B then joins the police at the duty desk and clocks in. The harness
// cannot drive, so `parkat` sets A's car 8 m ahead of the police car marker `car_b32_3`. B unlocks that police car,
// sits at the wheel, presses "Run plate" and reads the pager: Plate <plate>: STOLEN. The harness `use` takes the action the
// seat's interact menu offers (it answers cannot_show for one the row hides); `aim` reads nothing while seated, so the
// scroll itself is not read back. Pictures: the pager card, and the view from the seat. Every step prints PASS or FAIL.
const FIT = "Fit_saint_philippe_";
const GARAGE = `${FIT}carlot_garage`;
const LOT = `${FIT}carlot_vehicle_lot`;
const DESK = `${FIT}police_police_kit_duty_desk`;
const POLICE_CAR = `${FIT}police_cars_car_b32_3`;
const ACTION = "LF_RunPlateAction";

async function runRunPlateIslandProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const flag = (name, dflt) => { const i = process.argv.indexOf(name); return i > 0 ? process.argv[i + 1] : dflt; };
  const prefix = flag("--name", "run_plate_island");
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${String(r.detail).slice(0, 300)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const state = (n) => parseState(on(n, { verb: "state" }, 10000).detail);
  const serverLog = () => {
    const f = newestLog(join(sessionPaths(dir).server, "logs"));
    return f ? readFileSync(f, "utf8") : "";
  };
  const steps = [];
  const step = (name, ok, why) => { steps.push({ name, ok, why }); console.log(`${ok ? "PASS" : "FAIL"}  ${name}${ok ? "" : `: ${why}`}`); };
  const shot = (n, name) => on(n, { verb: "capture", mode: "ui", name: `${prefix}_${name}` }, 25000);

  // The flagship owner config (police.alphaInstantApproval approves a desk application on submit).
  const flagship = JSON.parse(readFileSync(join(ROOT, "server", "flagship", "config.json"), "utf8"));
  const ownerFile = join(sessionPaths(dir).server, "profile", "LifeFramework", "config.json");
  mkdirSync(dirname(ownerFile), { recursive: true });

  for (const n of [1, 2]) arrive(on, n, wait);
  for (const n of [1, 2]) { wait(3000); on(n, { verb: "click", widget: "WelcomeGuideDismissButton" }, 10000); }
  wait(8000);
  writeFileSync(ownerFile, JSON.stringify(flagship, null, 2));
  on(1, { verb: "configreload" }, 10000);
  wait(3500);

  // 1. A buys a car and takes it out of the garage with its key.
  on(1, { verb: "licence", name: "DRIVING_LICENCE" }, 10000);
  wait(1500);
  on(1, { verb: "stand", entityName: LOT });
  wait(1500);
  on(1, { verb: "use", className: "LF_VehicleBuyAction", entityName: LOT }, 10000);
  wait(1500);
  on(1, { verb: "click", widget: "Car1_Action" });
  wait(2500);
  on(1, { verb: "click", widget: "LotClose" });
  wait(1000);
  const park = on(1, { verb: "park" }, 10000);
  on(1, { verb: "stand", entityName: GARAGE, side: "e5" });
  wait(2500);
  if (/WelcomeGuideDismissButton/.test(String(on(1, { verb: "ui" }).detail))) on(1, { verb: "click", widget: "WelcomeGuideDismissButton" });
  on(1, { verb: "use", className: "LF_VehicleStoreAction", entityName: GARAGE }, 10000);
  wait(3000);
  on(1, { verb: "use", className: "LF_VehicleRetrieveAction", entityName: GARAGE }, 10000);
  wait(3000);
  step("1 A buys a car and gets it out of the garage", park.status === "ok", `park=${park.status} ${park.detail}`);

  // 2. The car is stolen. (A's retrieved car comes out locked; A unlocks it with the key.)
  on(1, { verb: "stand", entityName: GARAGE, side: "n12" });
  wait(3500);
  on(1, { verb: "look" });
  const unlocks = () => (serverLog().match(/toggled by menu -> locked=0/g) || []).length;
  const unlockedBefore = unlocks();
  on(1, { verb: "use", className: "LF_VehicleLockAction" }, 10000);
  wait(2000);
  step("2a A unlocks their car with the key", unlocks() > unlockedBefore, "no 'toggled by menu -> locked=0' line");
  // The car is marked stolen by the harness (`markstolen`, what a hotwire sets), so the STOLEN read is tested on every run:
  // the hotwire is a skill roll and a failed roll left the car clean (round 6 P3). The hotwire has its own proofs (hotwire-proof).
  const marked = on(1, { verb: "markstolen", words: [] }, 10000);
  wait(1500);
  step("2b the harness marks A's car stolen (a hotwire's result, with no roll)", marked.status === "ok" && /plate=\S+/.test(String(marked.detail)), `${marked.status} ${marked.detail}`);

  // 3. A reports it stolen.
  on(1, { verb: "look" });
  const report = on(1, { verb: "use", className: "LF_ReportStolenAction" }, 10000);
  wait(2000);
  const filed = serverLog().match(/reported stolen: \S+ plate='([^']*)'/g) || [];
  const plate = filed.length ? /plate='([^']*)'/.exec(filed[filed.length - 1])[1] : "";
  step("3 A reports the car stolen", report.status === "ok" && !!plate, `use=${report.status} ${report.detail}; filed lines: ${filed.length}`);

  // 4. B joins the police and clocks in.
  on(2, { verb: "resetcrime" }, 10000);
  on(2, { verb: "stand", entityName: DESK });
  wait(2500);
  on(2, { verb: "look" });
  let duty = state(2);
  if (duty.duty !== "1") {
    // (an earlier run in this session may have joined and clocked in already: the duty action toggles)
    if (duty.faction !== "POLICE") {
      on(2, { verb: "use", className: "LF_PoliceApplyAction", entityName: DESK }, 10000);
      wait(1500);
      on(2, { verb: "type", widget: "PolAppName", text: "Bob_Officer" });
      on(2, { verb: "type", widget: "PolAppWhy", text: "Run_the_plates" });
      on(2, { verb: "type", widget: "PolAppAvailability", text: "Evenings" });
      on(2, { verb: "click", widget: "PolAppSubmit" });
      wait(3000);
      on(2, { verb: "look" });
    }
    on(2, { verb: "use", className: "LF_DutyAction", entityName: DESK }, 10000);
    wait(3000);
    duty = state(2);
  }
  step("4 B joins the police and clocks in", duty.duty === "1", `faction=${duty.faction} duty=${duty.duty}`);

  // 5. A's car is set 8 m ahead of the police car; B unlocks the police car and sits at the wheel.
  const parked = on(1, { verb: "parkat", words: [POLICE_CAR, "8"] }, 10000);
  step("5a the stolen car is set ahead of the police car", parked.status === "ok", `${parked.status} ${parked.detail}`);
  wait(2500);
  on(2, { verb: "stand", entityName: POLICE_CAR, side: "w" });
  wait(3500);
  on(2, { verb: "look" });
  on(2, { verb: "use", className: "LF_VehicleLockAction" }, 10000);
  wait(2000);
  const seatB = on(2, { verb: "engine", entityName: "seat" }, 10000);
  wait(4000);
  const clientSees = String(on(2, { verb: "seen", radius: 20 }).detail);
  step("5b B sits in the police car and the stolen car stands ahead of it on B's client", seatB.status === "ok" && /veh=[^ ]*EL_S105_beige:d=([5-9]|1[0-2])[.:]/.test(clientSees), `seat=${seatB.status} ${seatB.detail}; B sees ${clientSees.slice(0, 200)}`);

  // 6. Press Run plate (the harness `use` takes the action the seat's interact menu lists and refuses one that is not
  // shown: cannot_show), read the server's answer and the pager.
  const before = (serverLog().match(/run plate by /g) || []).length;
  const run = on(2, { verb: "use", className: ACTION }, 10000);
  wait(2500);
  step("6a Run plate is offered at the wheel and performed", run.status === "ok", `${run.status} ${run.detail}`);
  const lines = serverLog().match(/run plate by [^\n]*/g) || [];
  const last = lines.length ? lines[lines.length - 1] : "";
  step("6b the server answers STOLEN for the reported plate", lines.length > before && /-> STOLEN/.test(last) && (!plate || last.includes(`'${plate}'`)), `last run plate line: ${last || "none"}; plate=${plate}`);
  on(2, { verb: "pager" }, 10000);
  wait(2000);
  const card = on(2, { verb: "ui", mode: "text" }, 10000).detail;
  step("6c the pager card reads Plate ... STOLEN", /STOLEN/.test(card) && (!plate || card.includes(plate)), String(card).slice(0, 240));
  shot(2, "pager_stolen");
  on(2, { verb: "input", action: "MenuBack" });
  wait(1500);
  shot(2, "behind_stolen_car");

  // Keep the feature lines: `session down` removes the server log.
  const kept = serverLog().split(/\r?\n/).filter((l) => /LFDebug:(RunPlate|Stolen|Hotwire|AgentHarness|Duty|Police)/.test(l));
  mkdirSync(join(ROOT, "tmp"), { recursive: true });
  writeFileSync(join(ROOT, "tmp", "run-plate-island-server.txt"), kept.join("\n"));
  const failed = steps.filter((s) => !s.ok);
  console.log("");
  for (const s of steps) console.log(`${s.ok ? "PASS" : "FAIL"}  ${s.name}`);
  console.log(failed.length ? `run plate island proof: FAIL (${failed.length} of ${steps.length})` : `run plate island proof: PASS (${steps.length} steps)`);
  return failed.length ? 1 : 0;
}

export default {
  name: "run-plate-island-proof",
  usage: "run-plate-island-proof [--name <shot prefix>]",
  run: (ctx) => runRunPlateIslandProof(),
};

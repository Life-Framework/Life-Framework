import { readFileSync } from "node:fs";
import { join } from "node:path";
import { arrive, clientBox, newestLog, parseState, sessionDir, sessionPaths } from "../session.mjs";
import { graceOff } from "./_grace.mjs";

// #3752: the Saint-Philippe bank job on the island with two robbers and two officers on a dedicated
// server. Clients 1 and 2 are robbers, 3 and 4 are officers (the police roster). Steps:
//   1. one officer clocks in: robber 1 holds a charge and tries the staff door; the heist refuses
//      it (fewer than two police on duty), the door stays shut;
//   2. the second officer clocks in: robber 1 arms the staff door and the run starts;
//   3. robber 2 drills the vault (30 s) and the vault opens;
//   4. both robbers go to the police desk (more than the 120 m getaway radius): the run completes
//      with a crew of two;
//   5. an officer locks the staff door again and robber 1 arms a new charge: the big-job cooldown (3 h)
//      refuses it, no second run starts (#4508: the lock is taken at arming).
// The first refusal, the run start, the vault stage, the completion and the lock refusal are read from the server log.
//   tools\cli session up --mission island --clients 4 --roster 3,4
//   node agent-box/session.mjs island-bank-heist-proof
//   tools\cli session down
async function runIslandBankHeistProof() {
  const h = await import("../harness.mjs");
  const DOOR = "Fit_saint_philippe_bank_bank_staff_door";
  const VAULT = "Fit_saint_philippe_bank_lf_bank_vault";
  const DESK = "Fit_saint_philippe_police_police_kit_duty_desk";
  const dir = sessionDir();
  const paths = sessionPaths(dir);
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail.slice(0, 300)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const serverLog = () => { const f = newestLog(join(paths.server, "logs")); return f ? readFileSync(f, "utf8") : ""; };
  const results = [];
  const check = (name, pass, detail) => { results.push({ name, pass }); console.log(`${pass ? "PASS" : "FAIL"}  ${name}: ${detail}`); };
  const doorOpen = (n) => {
    const entry = (parseState(on(n, { verb: "seen", radius: 25 }, 10000).detail).breach ?? "").split("|").find((e) => e.includes("bank_staff_door"));
    const m = /open=(\d)/.exec(entry ?? "");
    return m ? m[1] : "none";
  };
  const waitLog = (re, from, ms) => {
    const t0 = Date.now();
    while (Date.now() - t0 < ms) {
      if (re.test(serverLog().slice(from))) return true;
      wait(3000);
    }
    return re.test(serverLog().slice(from));
  };
  const clockIn = (n) => {
    on(n, { verb: "stand", entityName: DESK });
    wait(2500);
    const r = on(n, { verb: "use", className: "LF_DutyAction", entityName: DESK }, 10000);
    wait(2500);
    return r;
  };

  for (const n of [1, 2, 3, 4]) arrive(on, n, wait);
  graceOff(on, dir, wait); // a fresh account is in its robbery grace: the vault rob would be refused
  wait(8000);
  for (const n of [1, 2]) on(n, { verb: "resetcrime" }, 10000);
  const logStart = serverLog().length;

  const cop1 = clockIn(3);
  check("first officer on duty", cop1.status === "ok", `${cop1.status} ${cop1.detail.slice(0, 100)}`);

  on(1, { verb: "charge" }, 10000);
  on(1, { verb: "stand", entityName: DOOR });
  wait(2500);
  const refused = on(1, { verb: "use", className: "LF_BreachArmAction", entityName: DOOR }, 10000);
  wait(3000);
  const refusedLog = /arm on bank_staff_door refused .*too few police/.test(serverLog().slice(logStart));
  check("one officer: arming the staff door is refused", refused.status !== "ok" || refusedLog, `status=${refused.status} serverLine=${refusedLog}`);
  check("one officer: the staff door stays shut", doorOpen(1) !== "1", `open=${doorOpen(1)}`);
  check("one officer: no run started", !/run started by/.test(serverLog().slice(logStart)), "no 'run started by' line");

  const cop2 = clockIn(4);
  check("second officer on duty", cop2.status === "ok", `${cop2.status} ${cop2.detail.slice(0, 100)}`);

  on(1, { verb: "stand", entityName: DOOR });
  wait(2500);
  const armed = on(1, { verb: "use", className: "LF_BreachArmAction", entityName: DOOR }, 10000);
  check("two officers: the staff door arms", armed.status === "ok", `${armed.status} ${armed.detail.slice(0, 120)}`);
  check("two officers: the run starts", waitLog(/saint_philippe_bank: run started by/, logStart, 20000), "server 'run started by'");
  check("staff door open", doorOpen(1) === "1", `open=${doorOpen(1)}`);

  on(2, { verb: "drill" }, 10000);
  wait(1500);
  // The rob row wants the drill in the hands: a quick-slot press, repeated until `held=1` (a press toggles, so one that
  // finds it held leaves it be).
  for (let slotTry = 0; slotTry < 3; slotTry++) {
    if (/held=1/.test(String(on(2, { verb: "quickslot", words: ["drill"] }, 10000).detail))) break;
    on(2, { verb: "quickslot", words: ["drill", "press"] }, 10000);
    wait(3000);
  }
  on(2, { verb: "stand", entityName: VAULT });
  wait(2500);
  const rob = on(2, { verb: "use", className: "LF_BankRobAction", entityName: VAULT }, 10000);
  check("vault drill action ok", rob.status === "ok", `${rob.status} ${rob.detail.slice(0, 120)}`);
  const stages = () => (serverLog().slice(logStart).match(/Heist\] stage \d+ reached/g) || []).length;
  { const t0 = Date.now(); while (stages() < 2 && Date.now() - t0 < 90000) wait(3000); }
  check("vault stage reached", stages() >= 2, `stage lines=${stages()}`);

  for (const n of [1, 2]) on(n, { verb: "stand", entityName: DESK });
  const done = waitLog(/saint_philippe_bank: run complete, crew of 2 got away/, logStart, 90000);
  check("getaway completes with both robbers", done, "server 'run complete, crew of 2 got away'");
  const failed = /saint_philippe_bank: run failed/.test(serverLog().slice(logStart));
  check("the run did not fail", !failed, `failed=${failed}`);

  // Step 5 (#4438, lock at arming #4508): the job is over, so the big-job cooldown holds the bank. An officer
  // locks the staff door again (the arm offer says "already open" on an open door and never reaches the server),
  // robber 1 takes a new charge and tries to arm it: the server refuses and says the job is cooling down.
  const lockFrom = serverLog().length;
  on(3, { verb: "stand", entityName: DOOR });
  wait(2500);
  const repair = on(3, { verb: "use", className: "LF_BreachRepairAction", entityName: DOOR }, 10000);
  wait(3000);
  const repaired = /Breach\] repaired bank_staff_door by/.test(serverLog().slice(lockFrom));
  check("officer locks the staff door again", repair.status === "ok" && repaired, `${repair.status} serverLine=${repaired}`);
  on(1, { verb: "charge" }, 10000);
  on(1, { verb: "stand", entityName: DOOR });
  wait(2500);
  const second = on(1, { verb: "use", className: "LF_BreachArmAction", entityName: DOOR }, 10000);
  wait(3000);
  // The offer is refused on the lock ("This place was hit recently"), so the use verb answers cannot_perform; the server names it.
  const lockLine = /BigJob\] arming bank refused: (cooling down, \d+ s left|big job \S+ is running)|arm on bank_staff_door refused for .*: (cooling down, \d+ s left|big job \S+ is running)/.exec(serverLog().slice(lockFrom));
  check("second arm: the big-job lock refuses", second.status !== "ok" && lockLine !== null, `status=${second.status} serverLine=${lockLine ? (lockLine[1] || lockLine[2]) : "none"}`);
  check("second arm: the staff door stays shut", doorOpen(1) !== "1", `open=${doorOpen(1)}`);
  const runStarts = (serverLog().slice(logStart).match(/saint_philippe_bank: run started by/g) || []).length;
  check("second arm: no second run started", runStarts === 1, `run started lines=${runStarts}`);

  console.log(results.every((r) => r.pass) ? "ALL PASS" : "SOME FAIL: " + results.filter((r) => !r.pass).map((r) => r.name).join("; "));
  return results.every((r) => r.pass) ? 0 : 1;
}

export default {
  name: "island-bank-heist-proof",
  usage: "island-bank-heist-proof   (up --mission island --clients 4 --roster 3,4)",
  run: () => runIslandBankHeistProof(),
};

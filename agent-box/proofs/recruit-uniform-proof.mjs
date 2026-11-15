import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { arrive, clientBox, fixtureDir, newestLog, parseState, sessionDir, sessionPaths, sessionUid } from "../session.mjs";

// #1450, beat 4 of the recruit chapter, driven end to end on
//   up --clients 2 --mission playtest --admin-client 1 --no-instant-approval --skip-playtime-gate
// (nobody on the roster; before a re-run delete police_applications.txt and whitelist.txt in
// <session dir>/server/profile/LifeFramework, the server keeps them between sessions):
//   1. apply   client 2 files the application at the duty desk (it stays open: the gated path);
//   2. approve client 1, the administrator, approves it through LF_PoliceApplicationStaff (`staffapprove`);
//   3. on duty client 2 uses the duty desk's Clock in, and its account reads POLICE on duty;
//   4. uniform the clock-in line shows the police kit put on client 2 (the starter kit on a first shift, the
//              stored police inventory on a later one) and the server logs no kit error.
// One PASS or FAIL line per step, read from the harness replies and the server log.
// `--skip-approval` leaves step 2 out: client 2 is not on the roster, so the desk refuses it with
// `#LF-Duty_NotOnRoster` and the proof must fail at "on duty" (the red run).
//   node agent-box/session.mjs recruit-uniform-proof [--skip-approval]

// Pure: the verdict of one step from what was read. Returns "" for a pass, or the reason.
export function judgeStep(step, facts) {
  if (step === "apply") {
    if (facts.openStatus !== "ok") return `apply action returned ${facts.openStatus} ${facts.openDetail}`;
    if (facts.filed || facts.alreadyOpen) return "";
    return "server log has no 'police application filed by' line";
  }
  if (step === "approve") {
    if (facts.instantlyApproved) return "the application was approved on submit: start the session with --no-instant-approval";
    if (facts.approveStatus !== "ok" || !String(facts.approveDetail).includes(`approved ${facts.uid2}`)) return `client 1 staffapprove returned ${facts.approveStatus} ${facts.approveDetail}`;
    if (!facts.staffLine) return `server log has no 'staff ${facts.uid1} approved the application of ${facts.uid2}' line`;
    return "";
  }
  if (step === "on duty") {
    if (facts.dutyStatus !== "ok") return `the duty desk refused client 2: ${facts.dutyStatus} ${facts.dutyDetail}`;
    if (!facts.clockedIn) return `server log has no 'clocked in ${facts.uid2}' line`;
    if (String(facts.state.faction).toLowerCase() !== "police" || String(facts.state.duty) !== "1") return `client 2 state is not police on duty: ${facts.stateText}`;
    return "";
  }
  if (step === "uniform") {
    if (!facts.clockedIn) return "no clock-in to read the kit from";
    if (!facts.kitIssued) return `the clock-in line for ${facts.uid2} says neither 'police starter kit issued' nor 'police inventory restored': ${facts.clockLine}`;
    if (facts.kitError) return "server log has 'no police kit issued': the spawn logic has no kit configured";
    return "";
  }
  return `unknown step ${step}`;
}

async function runRecruitUniformProof(args) {
  const skipApproval = args.includes("--skip-approval");
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${String(r.detail).slice(0, 240)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const rawLog = () => {
    const f = newestLog(join(sessionPaths(dir).server, "logs"));
    return f ? readFileSync(f, "utf8") : "";
  };
  // Only what this run wrote: an earlier run in the same session left its own lines.
  const logMark = rawLog().length;
  const serverLog = () => rawLog().slice(logMark);
  const uid1 = sessionUid(1);
  const uid2 = sessionUid(2);
  const results = [];
  const step = (name, facts) => {
    const reason = judgeStep(name, facts);
    results.push({ name, reason });
    console.log(reason ? `FAIL  ${name}: ${reason}` : `PASS  ${name}`);
    return !reason;
  };

  // A roster file left by an earlier run would let client 2 clock in without the approval.
  const roster = join(fixtureDir(sessionPaths(dir).server), "whitelist.txt");
  if (existsSync(roster) && readFileSync(roster, "utf8").includes(uid2)) {
    console.log(`FAIL  precondition: ${uid2} is already on the police roster (${roster}); delete whitelist.txt and police_applications.txt, then up again`);
    console.log("recruit-uniform proof: FAIL");
    return 1;
  }

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);

  // 1. apply
  on(2, { verb: "stand", entityName: "Roadmap_Police_Desk" });
  wait(2500);
  on(2, { verb: "look" });
  const open = on(2, { verb: "use", className: "LF_PoliceApplyAction", entityName: "Roadmap_Police_Desk" }, 10000);
  wait(1500);
  on(2, { verb: "type", widget: "PolAppName", text: "Ann_Lee" });
  on(2, { verb: "type", widget: "PolAppWhy", text: "I_want_to_keep_the_town_safe" });
  on(2, { verb: "type", widget: "PolAppAvailability", text: "Evenings_and_weekends" });
  wait(500);
  on(2, { verb: "click", widget: "PolAppSubmit" });
  wait(2500);
  let log = serverLog();
  step("apply", {
    openStatus: open.status,
    openDetail: open.detail,
    filed: /police application filed by/.test(log),
    alreadyOpen: new RegExp(`police application from ${uid2} refused: #LF-PoliceApp_Error_AlreadyOpen`).test(log),
  });

  // 2. approve (the step the red run leaves out)
  if (skipApproval) {
    console.log("SKIP  approve: --skip-approval, the application stays open");
  } else {
    const r = on(1, { verb: "staffapprove", uid: uid2 }, 15000);
    wait(1500);
    log = serverLog();
    step("approve", {
      uid1,
      uid2,
      approveStatus: r.status,
      approveDetail: r.detail,
      instantlyApproved: /approved, added to the police roster/.test(log) && !log.includes(`staff ${uid1} approved the application of ${uid2}`),
      staffLine: log.includes(`staff ${uid1} approved the application of ${uid2}`),
    });
  }

  // 3 and 4. on duty, then the uniform
  on(2, { verb: "stand", entityName: "Roadmap_Police_Desk" });
  wait(2500);
  const duty = on(2, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Police_Desk" }, 10000);
  wait(2500);
  const stateText = String(on(2, { verb: "state" }, 10000).detail);
  log = serverLog();
  const clockLine = log.split("\n").find((l) => l.includes(`clocked in ${uid2}`)) ?? "";
  const facts = {
    uid2,
    dutyStatus: duty.status,
    dutyDetail: duty.detail,
    clockedIn: clockLine !== "",
    state: parseState(stateText),
    stateText,
    clockLine,
    kitIssued: /police (starter kit issued|inventory restored)/.test(clockLine),
    kitError: /no police kit issued/.test(log),
  };
  step("on duty", facts);
  step("uniform", facts);

  const failed = results.filter((r) => r.reason);
  console.log(failed.length ? `recruit-uniform proof: FAIL at "${failed[0].name}"` : "recruit-uniform proof: PASS");
  return failed.length ? 1 : 0;
}

export default {
  name: "recruit-uniform-proof",
  usage: "recruit-uniform-proof [--skip-approval] (up --clients 2 --mission playtest --admin-client 1 --no-instant-approval --skip-playtime-gate)",
  run: (ctx) => runRecruitUniformProof(ctx.args),
};

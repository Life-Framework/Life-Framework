import { readFileSync } from "node:fs";
import { join } from "node:path";
import { arrive, clientBox, newestLog, sessionDir, sessionPaths, sessionUid } from "../session.mjs";

// #1450 harness prerequisites, on `up --clients 2 --mission playtest --admin-client 1 --no-instant-approval
// --skip-playtime-gate` (roster nobody: the first player to join is auto-police on that mission, and is the
// administrator here). Client 2 files a police application at the duty desk and it STAYS open (the gated path:
// no approve on submit, the minimum playtime bypassed for a fresh account). Then:
//   - client 2 (not an administrator) sends `staffapprove` for itself: refused `#LF-PoliceApp_NotAdmin`,
//     the application is still open;
//   - client 1 (the administrator) stands at the desk and `look` offers `LF_PoliceReviewAction` (the review row);
//   - client 1's `staffapprove <client 2's uid>` is accepted and the server logs `staff <uid 1> approved the
//     application of <uid 2>` and the roster line.
// It is also the check that card C's recruit-to-uniform proof can build on: after this the account is on the
// police roster and can clock in.
async function runStaffApproveProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const serverLog = () => {
    const f = newestLog(join(sessionPaths(dir).server, "logs"));
    return f ? readFileSync(f, "utf8") : "";
  };
  const fails = [];
  const uid1 = sessionUid(1);
  const uid2 = sessionUid(2);
  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);

  // Client 2 applies.
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
  if (open.status !== "ok") fails.push(`apply action returned ${open.status} ${open.detail}`);
  // The server profile keeps police_applications.txt and whitelist.txt between sessions: an application an earlier
  // run left open is just as good here (delete both files in <session dir>/server/profile/LifeFramework for a clean run).
  if (!/police application filed by/.test(log)) {
    if (new RegExp(`police application from ${uid2} refused: #LF-PoliceApp_Error_AlreadyOpen`).test(log)) console.log("NOTE  client 2 already had an open application from an earlier run; the staff path below still applies");
    else fails.push("server log has no 'police application filed by' line");
  }
  if (/approved, added to the police roster/.test(log)) fails.push("the application was approved on submit: start the session with --no-instant-approval");
  if (/Error_Playtime|under the playtime/i.test(log)) fails.push("the playtime gate refused the application: start the session with --skip-playtime-gate");

  // A non-administrator is refused by the staff API and nothing changes.
  const refused = on(2, { verb: "staffapprove", uid: uid2 }, 15000);
  if (refused.status !== "error" || !/NotAdmin/.test(refused.detail)) fails.push(`client 2 (not an administrator) was not refused: ${refused.status} ${refused.detail}`);
  if (/staff .* approved the application of/.test(serverLog())) fails.push("a non-administrator approved an application");

  // The administrator sees the review row and approves through the staff API.
  on(1, { verb: "stand", entityName: "Roadmap_Police_Desk" });
  wait(2500);
  let seen = on(1, { verb: "look" });
  for (let t = 0; t < 5 && !seen.detail.includes("LF_PoliceReviewAction"); t++) {
    wait(1500);
    seen = on(1, { verb: "look" });
  }
  if (!seen.detail.includes("LF_PoliceReviewAction")) fails.push("client 1 is not offered the review row (LF_PoliceReviewAction): is it an engine administrator? start the session with --admin-client 1");
  const approved = on(1, { verb: "staffapprove", uid: uid2 }, 15000);
  if (approved.status !== "ok" || !approved.detail.includes(`approved ${uid2}`)) fails.push(`client 1 staffapprove returned ${approved.status} ${approved.detail}`);
  wait(1500);
  log = serverLog();
  if (!log.includes(`staff ${uid1} approved the application of ${uid2}`)) fails.push(`server log has no 'staff ${uid1} approved the application of ${uid2}' line`);
  if (!/harness admin: player \d+ \(\S+\) is a harness administrator/.test(log)) fails.push("server log has no 'harness admin' grant line");
  // A second approve finds nothing open.
  const again = on(1, { verb: "staffapprove", uid: uid2 }, 15000);
  if (again.status !== "error" || !/NoneOpen/.test(again.detail)) fails.push(`a second approve was not refused as 'none open': ${again.status} ${again.detail}`);

  // The chat verb (#1451): a line typed as a client goes through the real send path to the Admin channel. `!preject`
  // by an administrator reaches LF_PoliceApplications.Reject (nothing is open now, so the server logs 'rejection
  // refused: <uid> has no open application'); the same line from the non-administrator never runs as an administrator
  // (the channel drops it or its handler refuses it), so it adds no such line.
  const ran = () => (serverLog().match(new RegExp(`rejection refused: ${uid2} has no open application`, "g")) ?? []).length;
  const before = ran();
  const chat2 = on(2, { verb: "chat", text: `!preject ${uid2}` }, 15000);
  wait(2000);
  const afterCivilian = ran();
  const chat1 = on(1, { verb: "chat", text: `!preject ${uid2}` }, 15000);
  wait(2000);
  const afterAdmin = ran();
  if (chat2.status !== "ok" || !chat2.detail.includes("sent Admin !preject")) fails.push(`client 2 chat returned ${chat2.status} ${chat2.detail}`);
  if (chat1.status !== "ok" || !chat1.detail.includes("sent Admin !preject")) fails.push(`client 1 chat returned ${chat1.status} ${chat1.detail}`);
  if (afterCivilian !== before) fails.push("the non-administrator's !preject ran on the server");
  if (afterAdmin !== afterCivilian + 1) fails.push(`the administrator's !preject did not reach the server's Admin channel handler (${afterCivilian} -> ${afterAdmin})`);

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "staff approve proof: FAIL" : "staff approve proof: PASS");
  return fails.length ? 1 : 0;
}

export default {
  name: "staff-approve-proof",
  usage: "staff-approve-proof (up --clients 2 --mission playtest --admin-client 1 --no-instant-approval --skip-playtime-gate)",
  run: () => runStaffApproveProof(),
};

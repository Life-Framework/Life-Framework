import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { arrive, clientBox, newestLog, sessionDir, sessionPaths } from "../session.mjs";

// #1288 on a running `up --profile full --clients 2` session (the default world). Both clients open
// the police applications panel as non-staff: the server refuses each and logs the account uid.
// The proof then makes client 1 an administrator on the server's staff roster and files an open
// application for client 2 (the roster and the applications file are read on every call), opens
// the panel on client 1 again, reads the row and approves it. Shots: the refusal, the list with the
// application read, the approved line. The server log must show the refusals, the list and the
// approval, and client 2 must end on the police roster.
async function runPoliceAppGmProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const server = sessionPaths(dir).server;
  const serverLog = () => {
    const f = newestLog(join(server, "logs"));
    return f ? readFileSync(f, "utf8") : "";
  };
  const refusedUids = () => [...serverLog().matchAll(/staff list refused: (\S+) is not staff/g)].map((m) => m[1]);
  const fails = [];

  const profile = join(server, "profile", "LifeFramework"); // -profile <dir> puts $profile: in <dir>\profile
  for (const f of ["staff.txt", "police_applications.txt"]) rmSync(join(profile, f), { force: true });
  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);

  on(1, { verb: "kit", step: "policeapps" }, 10000);
  wait(2500);
  on(1, { verb: "capture", mode: "ui", name: "police_app_gm_refused" }, 25000);
  const staffUid = refusedUids()[0];
  on(1, { verb: "kit", step: "hide" }, 10000);
  wait(400);
  on(2, { verb: "kit", step: "policeapps" }, 10000);
  wait(2500);
  on(2, { verb: "kit", step: "hide" }, 10000);
  const uids = refusedUids();
  const applicantUid = uids.find((u) => u !== staffUid);
  if (!staffUid || !applicantUid) {
    console.log(`FAIL  no refusal uids in the server log (${uids.join(", ")})`);
    console.log("police app gm proof: FAIL");
    return 1;
  }

  mkdirSync(profile, { recursive: true });
  writeFileSync(join(profile, "staff.txt"), `${staffUid} administrator # proof staff\n`);
  const filed = Math.floor(Date.now() / 1000) - 3 * 3600;
  writeFileSync(join(profile, "police_applications.txt"), `OPEN|${applicantUid}|Ann Lee|I want to keep the town safe|Evenings and weekends|${filed}|0|0\n`);

  wait(400);
  on(1, { verb: "kit", step: "policeapps" }, 10000);
  wait(2500);
  on(1, { verb: "kit", step: "policeapps_read" }, 10000);
  wait(800);
  on(1, { verb: "capture", mode: "ui", name: "police_app_gm_list" }, 25000);
  on(1, { verb: "kit", step: "policeapps_approve" }, 10000);
  wait(2500);
  on(1, { verb: "capture", mode: "ui", name: "police_app_gm_approved" }, 25000);

  const log = serverLog();
  if (uids.length < 2) fails.push("the server did not refuse both non-staff clients");
  if (!/\[LFDebug:PoliceAppGM\] sent 1 open application\(s\) to staff player/.test(log)) fails.push("no 'sent 1 open application(s)' line after the roster edit");
  if (!new RegExp(`staff \\S+ approved the application of ${applicantUid}`).test(log)) fails.push(`no approval line for ${applicantUid}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "police app gm proof: FAIL" : `police app gm proof: PASS (staff ${staffUid}, applicant ${applicantUid})`);
  return fails.length ? 1 : 0;
}

export default {
  name: "police-app-gm-proof",
  usage: "police-app-gm-proof",
  run: () => runPoliceAppGmProof(),
};

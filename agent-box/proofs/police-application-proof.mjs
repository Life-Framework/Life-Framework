import { readFileSync } from "node:fs";
import { join } from "node:path";
import { arrive, clientBox, newestLog, sessionDir, sessionPaths } from "../session.mjs";

// #128 on a running `up --clients 2 --mission playtest` session (the playtest town approves on
// submit). The first player to join is the officer, so client 2 is the civilian: it stands at the
// duty desk, opens "Apply to the police", fills the three blanks and hands it in. The server log
// must show the filed line and the approval, and the application file holds the record.
// Also the demo of the form for docs/playtest.md. Shots: the open form and the receipt toast.
async function runPoliceApplicationProof() {
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
  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  on(2, { verb: "stand", entityName: "Roadmap_Police_Desk" });
  wait(2500);
  on(2, { verb: "look" });
  const open = on(2, { verb: "use", className: "LF_PoliceApplyAction", entityName: "Roadmap_Police_Desk" }, 10000);
  wait(1500);
  on(2, { verb: "type", widget: "PolAppName", text: "Ann_Lee" });
  on(2, { verb: "type", widget: "PolAppWhy", text: "I_want_to_keep_the_town_safe" });
  on(2, { verb: "type", widget: "PolAppAvailability", text: "Evenings_and_weekends" });
  wait(500);
  on(2, { verb: "capture", mode: "ui", name: "police_application_form" }, 25000);
  on(2, { verb: "click", widget: "PolAppSubmit" });
  wait(2500);
  on(2, { verb: "capture", mode: "ui", name: "police_application_receipt" }, 25000);
  const log = serverLog();
  if (open.status !== "ok") fails.push(`apply action returned ${open.status} ${open.detail}`);
  if (!/police application filed by/.test(log)) fails.push("server log has no 'police application filed by' line");
  if (!/approved, added to the police roster/.test(log)) fails.push("server log has no approval line");
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "police application proof: FAIL" : "police application proof: PASS");
  return fails.length ? 1 : 0;
}

export default {
  name: "police-application-proof",
  usage: "police-application-proof",
  run: (ctx) => runPoliceApplicationProof(),
};

import { readFileSync } from "node:fs";
import { join } from "node:path";
import { ROB_TARGET, ROOT, arrive, clientBox, newestLog, parseState, sessionDir, sessionPaths } from "../session.mjs";

// M2 #264 on a running `up --clients 2` session: client 1 downs client 2 at the fuel till, client 2
// gives up and wakes in hospital with the New Life wristband (server log), walks to a far spot, then
// back to the death scene inside the window: one quiet toast and one incident row. A revive earlier
// gives no wristband.
async function runNlrProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const state = (n) => parseState(on(n, { verb: "state" }, 10000).detail);
  const serverLog = () => {
    const f = newestLog(join(sessionPaths(dir).server, "logs"));
    return f ? readFileSync(f, "utf8") : "";
  };
  const count = (re) => (serverLog().match(re) || []).length;
  const fails = [];
  const medicConf = readFileSync(join(ROOT, "addons", "LifeFramework", "Configs", "Medic", "LF_Medic.conf"), "utf8");
  const giveUpDelay = Number(/m_fGiveUpDelaySeconds\s+(\d+)/.exec(medicConf)?.[1] ?? 30);

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  const before = count(/NewLife\].*wristband on/g);

  on(2, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  on(1, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  on(1, { verb: "shoot", playerId: 2 }, 10000);
  wait(2500);
  if (state(2).downed !== "1") fails.push("client 2 was not downed");
  wait((giveUpDelay + 2) * 1000);
  on(2, { verb: "giveup" }, 10000);
  wait(4000);
  on(2, { verb: "click", widget: "RespawnButton" });
  wait(14000);
  on(2, { verb: "capture", mode: "ui", name: "m2_264_wristband_toast_client2" }, 25000);
  if (count(/NewLife\].*wristband on/g) !== before + 1) fails.push("the wake after a real death did not log one wristband");
  const incidentsBefore = count(/Incident\].*nlr:/g);
  const remindersBefore = count(/NewLife].*returned to their death scene/g);

  // Out of the scene's radius, then back inside the window.
  on(2, { verb: "stand", entityName: "Roadmap_Convoy_Drop" });
  wait(9000);
  on(2, { verb: "stand", entityName: ROB_TARGET });
  wait(9000);
  on(2, { verb: "capture", mode: "ui", name: "m2_264_return_toast_client2" }, 25000);
  if (count(/NewLife\].*returned to their death scene/g) !== remindersBefore + 1) fails.push("the return did not log exactly one reminder");
  if (count(/Incident\].*nlr:/g) !== incidentsBefore + 1) fails.push("the return did not write exactly one incident row");
  on(2, { verb: "stand", entityName: "Roadmap_Convoy_Drop" });
  wait(7000);
  on(2, { verb: "stand", entityName: ROB_TARGET });
  wait(9000);
  if (count(/NewLife\].*returned to their death scene/g) !== remindersBefore + 1) fails.push("a second return reminded again");

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "nlr proof: FAIL" : "nlr proof: PASS (wristband on the real-death wake, one reminder and one incident row on return, no repeat)");
  return fails.length ? 1 : 0;
}

export default {
  name: "nlr-proof",
  usage: "nlr-proof",
  run: (ctx) => runNlrProof(),
};

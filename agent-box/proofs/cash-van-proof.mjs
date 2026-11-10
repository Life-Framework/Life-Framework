import { arrive, clientBox, sessionDir } from "../session.mjs";
import { makeClientSees } from "./_clientSees.mjs";

// M2 #274 on a running `up --clients 2` session with client 2 on the roster (`session.mjs roster 2`
// before up): client 2 clocks in; client 1 is handed a stamped drill (dev gate, standing in for the
// fence), stands at the cash van box and is refused without it, then opens the box. Three money bags
// lie behind the van and the officer's dispatch line arrives. Confirm `[LFDebug:Bank] heist
// Roadmap_Cash_Van_Box paid 1200 bags` and one `Dispatch` call in the server log.
async function runCashVanProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const sees = makeClientSees({ on, wait, dir });
  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  on(2, { verb: "stand", entityName: "Roadmap_Police_Desk" });
  wait(2500);
  on(2, { verb: "look" });
  const clockIn = on(2, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Police_Desk" }, 10000);
  wait(3000);
  on(1, { verb: "stand", entityName: "Roadmap_Cash_Van_Box" });
  on(2, { verb: "stand", entityName: "Roadmap_Cash_Van_Box" });
  wait(2500);
  // #522: what client 2 sees at the van before the heist (the box offers its action, no bags yet).
  const before2 = sees.lookAround(2, "Roadmap_Cash_Van_Box");
  on(1, { verb: "capture", mode: "ui", name: "m2_274_van_before_client1" }, 25000);
  const noTool = on(1, { verb: "use", className: "LF_HeistRobAction", entityName: "Roadmap_Cash_Van_Box" }, 10000);
  const give = on(1, { verb: "drill" }, 10000);
  wait(1500);
  const rob = on(1, { verb: "use", className: "LF_HeistRobAction", entityName: "Roadmap_Cash_Van_Box" }, 10000);
  wait(3000);
  // #522: the box opened and the bags exist on client 2: its own view of the van gained actions.
  const after2 = sees.lookAround(2, "Roadmap_Cash_Van_Box");
  console.log(`client 2 sees at the van: before ${JSON.stringify(before2)} after ${JSON.stringify(after2)}`);
  if (after2.length <= before2.length) fails.push(`client 2 sees no new bags at the van: before ${before2.length} actions, after ${after2.length}`);
  on(1, { verb: "capture", mode: "ui", name: "m2_274_van_opened_client1" }, 25000);
  on(2, { verb: "capture", mode: "ui", name: "m2_274_dispatch_client2" }, 25000);
  if (clockIn.status !== "ok") fails.push(`clock in returned ${clockIn.status} ${clockIn.detail}`);
  if (noTool.status === "ok") fails.push(`the box opened without a tool: ${noTool.detail}`);
  if (give.status !== "ok" || !give.detail.includes("added=1")) fails.push(`drill not given: ${give.status} ${give.detail}`);
  if (rob.status !== "ok") fails.push(`rob returned ${rob.status} ${rob.detail}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "cash van proof: FAIL" : "cash van proof: PASS (refused without a tool, opened with the drill; confirm the server log for the bags and the single dispatch call)");
  return fails.length ? 1 : 0;
}

export default {
  name: "cash-van-proof",
  usage: "cash-van-proof",
  run: (ctx) => runCashVanProof(),
};

import { arrive, clientBox, clientSees, parseSeen, sessionDir } from "../session.mjs";

// M2 #274 on a running `up --clients 2` session with client 2 on the roster (`session.mjs roster 2`
// before up): client 2 clocks in; client 1 is handed a stamped drill (dev gate, standing in for the
// fence), stands at the cash van box and is refused without it, then opens the box. Three money bags
// lie behind the van and the officer's dispatch line arrives. Confirm `[LFDebug:Bank] heist
// Roadmap_Cash_Van_Box paid 1200 bags` and one `Dispatch` call in the server log.
// Client side (#522): the officer, client 2, stands by the box and must READ the three bags lie behind
// it (the entities replicated to it) and the radio call arrive; client 1 reads the same bags.
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
  const officerBefore = clientSees(on, wait, 2, (s) => [
    ...(s.faction === "POLICE" && s.duty === "1" ? [] : [`faction/duty ${s.faction}/${s.duty}, expected POLICE/1`]),
    ...(s.bags === 0 ? [] : [`${s.bags} bags before the robbery`]),
  ], { label: "before the robbery" });
  const dispatchesBefore = parseSeen(officerBefore.line)?.dispatches ?? 0;
  on(1, { verb: "capture", mode: "ui", name: "m2_274_van_before_client1" }, 25000);
  const noTool = on(1, { verb: "use", className: "LF_HeistRobAction", entityName: "Roadmap_Cash_Van_Box" }, 10000);
  const give = on(1, { verb: "drill" }, 10000);
  wait(1500);
  const rob = on(1, { verb: "use", className: "LF_HeistRobAction", entityName: "Roadmap_Cash_Van_Box" }, 10000);
  wait(3000);
  on(1, { verb: "capture", mode: "ui", name: "m2_274_van_opened_client1" }, 25000);
  on(2, { verb: "capture", mode: "ui", name: "m2_274_dispatch_client2" }, 25000);
  // What the player sees: three bags in the world on BOTH clients, and the officer's radio call.
  const sawBags = (s) => (s.bags === 3 ? [] : [`${s.bags} money bags near the box, expected 3`]);
  const bags1 = clientSees(on, wait, 1, sawBags, { label: "after the robbery" });
  const bags2 = clientSees(on, wait, 2, sawBags, { label: "after the robbery" });
  const radio2 = clientSees(on, wait, 2, (s) => (s.dispatches > dispatchesBefore ? [] : [`${s.dispatches} radio calls heard, expected more than ${dispatchesBefore}`]), { label: "officer's dispatch" });
  console.log(`client sees: officer before ${officerBefore.line}`);
  console.log(`client sees: client 1 ${bags1.line}`);
  console.log(`client sees: client 2 ${bags2.line}`);
  fails.push(...officerBefore.fails, ...bags1.fails, ...bags2.fails, ...radio2.fails);
  if (clockIn.status !== "ok") fails.push(`clock in returned ${clockIn.status} ${clockIn.detail}`);
  if (noTool.status === "ok") fails.push(`the box opened without a tool: ${noTool.detail}`);
  if (give.status !== "ok" || !give.detail.includes("added=1")) fails.push(`drill not given: ${give.status} ${give.detail}`);
  if (rob.status !== "ok") fails.push(`rob returned ${rob.status} ${rob.detail}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "cash van proof: FAIL" : "cash van proof: PASS (refused without a tool, opened with the drill, three bags and the dispatch read on the client; confirm the server log for the bags and the single dispatch call)");
  return fails.length ? 1 : 0;
}

export default {
  name: "cash-van-proof",
  usage: "cash-van-proof",
  run: (ctx) => runCashVanProof(),
};

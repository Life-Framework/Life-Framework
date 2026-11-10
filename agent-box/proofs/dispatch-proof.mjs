import { ROB_TARGET, TILL_COOLDOWN_MS, arrive, clientBox, sessionDir } from "../session.mjs";

async function runDispatchProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const state = (n) => on(n, { verb: "state" }, 10000).detail;
  const duty = (n) => {
    on(n, { verb: "stand", entityName: "Roadmap_Police_Desk" });
    wait(2500);
    on(n, { verb: "look" });
    const r = on(n, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Police_Desk" }, 10000);
    wait(2500);
    return r;
  };
  const rob = (n) => {
    on(n, { verb: "stand", entityName: ROB_TARGET });
    wait(2500);
    on(n, { verb: "look" });
    const r = on(n, { verb: "use", className: "LF_RobAction", entityName: ROB_TARGET }, 10000);
    wait(3000);
    return r;
  };
  const fails = [];

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  if (duty(2).status !== "ok") fails.push("client 2 clock in failed");
  // Client 2 waits at the station, out of the till's normal hear range.
  const first = rob(1);
  if (first.status !== "ok") fails.push(`rob returned ${first.status} ${first.detail}`);
  const robbed1 = state(1);
  on(1, { verb: "capture", mode: "ui", name: "m1_20_alarm_normal_client1" }, 25000);
  wait(1000);
  on(2, { verb: "capture", mode: "ui", name: "m1_20_dispatch_client2" }, 25000);
  const robbed2 = state(2);

  on(2, { verb: "stand", entityName: "Roadmap_Police_WantedBoard" });
  wait(2500);
  on(2, { verb: "look" });
  on(2, { verb: "use", className: "LF_WantedBoardReadAction", entityName: "Roadmap_Police_WantedBoard" }, 10000);
  wait(2500);
  on(2, { verb: "capture", mode: "ui", name: "m1_20_board_dispatch_client2" }, 25000);
  on(2, { verb: "ui" });
  on(2, { verb: "click", widget: "Close" });
  wait(800);

  if (duty(2).status !== "ok") fails.push("client 2 clock out failed");
  console.log(`waiting ${TILL_COOLDOWN_MS / 1000} s for the till cooldown`);
  wait(TILL_COOLDOWN_MS);
  const second = rob(1);
  if (second.status !== "ok") fails.push(`second rob returned ${second.status} ${second.detail}`);
  const loud1 = state(1);
  on(1, { verb: "capture", mode: "ui", name: "m1_20_alarm_loud_client1" }, 25000);
  const loud2 = state(2);

  console.log("");
  console.log(`robbed (officer on duty) client 1: ${robbed1}`);
  console.log(`robbed (officer on duty) client 2: ${robbed2}`);
  console.log(`robbed (nobody on duty)  client 1: ${loud1}`);
  console.log(`robbed (nobody on duty)  client 2: ${loud2}`);
  fails.push(...judgeDispatch(robbed1, robbed2, loud1, loud2));
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "dispatch proof: FAIL" : "dispatch proof: PASS (normal alarm + radio dispatch on duty; loud alarm and no dispatch off duty)");
  return fails.length ? 1 : 0;
}

export default {
  name: "dispatch-proof",
  usage: "dispatch-proof",
  run: (ctx) => runDispatchProof(),
};

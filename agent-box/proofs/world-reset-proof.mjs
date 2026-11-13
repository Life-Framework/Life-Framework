import { readFileSync } from "node:fs";
import { join } from "node:path";
import { ROB_TARGET, arrive, clientBox, newestLog, parseState, sessionDir, sessionPaths } from "../session.mjs";

// #937 on a running `up` session (one client, lowspec is enough): the lone tester robs the till
// (cash up, wanted up, the second rob refused by the cooldown), dies, presses Respawn on the death
// screen, and the world is back: wanted 0, and the till pays again at once. The server log must
// show `world reset (respawn of player` from [LFDebug:WorldReset]. Also the demo of the feature
// (docs/playtest.md): run it to watch a respawn restock the town.
async function runWorldResetProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const state = () => parseState(on(1, { verb: "state" }, 10000).detail);
  const serverLog = () => {
    const f = newestLog(join(sessionPaths(dir).server, "logs"));
    return f ? readFileSync(f, "utf8") : "";
  };
  const rob = () => {
    on(1, { verb: "stand", entityName: ROB_TARGET });
    wait(2500);
    on(1, { verb: "look" });
    on(1, { verb: "use", className: "LF_RobAction", entityName: ROB_TARGET }, 10000);
    wait(3000);
  };
  const fails = [];

  arrive(on, 1, wait);
  wait(4000);
  on(1, { verb: "resetcrime" }, 10000);
  const start = state();
  rob();
  const robbed = state();
  const paid = Number(robbed.cash) - Number(start.cash);
  rob();
  const again = state();
  const paidAgain = Number(again.cash) - Number(robbed.cash);
  if (paid <= 0) fails.push(`the first rob paid nothing (cash ${start.cash} -> ${robbed.cash})`);
  if (Number(robbed.wanted || 0) <= 0) fails.push(`the rob left wanted=${robbed.wanted}`);
  if (paidAgain > 0) fails.push(`the till paid again inside its cooldown (+${paidAgain}); the proof needs it spent`);

  const before = (serverLog().match(/world reset \(respawn of player/g) || []).length;
  on(1, { verb: "die" }, 10000);
  wait(4000);
  on(1, { verb: "click", widget: "RespawnButton" });
  wait(14000);
  const woke = state();
  if (Number(woke.wanted || 0) !== 0) fails.push(`respawn left wanted=${woke.wanted}`);
  const resets = (serverLog().match(/world reset \(respawn of player/g) || []).length - before;
  if (resets < 1) fails.push("the server log shows no `world reset (respawn of player` line");

  rob();
  const after = state();
  const paidAfter = Number(after.cash) - Number(woke.cash);
  if (paidAfter <= 0) fails.push(`the till did not pay after the respawn reset (cash ${woke.cash} -> ${after.cash})`);
  on(1, { verb: "capture", mode: "ui", name: "world_reset_after_respawn" }, 25000);
  on(1, { verb: "resetcrime" }, 10000);

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "world reset proof: FAIL" : `world reset proof: PASS (till paid ${paid}, refused inside its cooldown, wanted cleared by the respawn, till paid ${paidAfter} again)`);
  return fails.length ? 1 : 0;
}

export default {
  name: "world-reset-proof",
  usage: "world-reset-proof",
  run: () => runWorldResetProof(),
};

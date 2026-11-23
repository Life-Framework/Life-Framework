// Session proof for the level-up sting (#4405): one client earns a level through the real path (AddXp, level event, RPC,
// card) and its own log must show the level_up slot resolved to the pack's sting and the sting played once.
//
//   node tools/cli session up --sound ; node tools/assets/level-up/sting-proof.mjs ; node tools/cli session down
import { readFileSync } from "node:fs";
import { arrive, clientBox, newestLog, sessionDir, sessionPaths } from "../../../agent-box/session.mjs";

const h = await import("../../../agent-box/harness.mjs");
const dir = sessionDir();
const on = (n, spec, timeout) => {
  h.setBoxes([clientBox(dir, n)]);
  const r = h.sendCommand(spec, timeout);
  console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
  return r;
};
const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
const log = () => {
  const f = newestLog(sessionPaths(dir).clients[0].logs);
  return f ? readFileSync(f, "utf8").split("\n").map((l) => l.trim()) : [];
};
const pick = (re) => log().filter((l) => re.test(l));

arrive(on, 1, wait);
wait(4000);
on(1, { verb: "levelupcard", words: ["grant", "MINER", "5000"] }, 10000);
wait(6000);
const state = on(1, { verb: "levelupcard", words: ["state"] }, 10000);
const slot = pick(/\[LFDebug:SoundSlots\].*slot 'level_up' ->.*Level_Sting\.acp/);
const played = pick(/\[LFDebug:Sound\] slot level_up played:.*Level_Sting\.acp/);
const silent = pick(/level_up slot yet/);
for (const l of [...slot, ...played]) console.log("    " + l.replace(/^.*\[LFDebug:/, "[LFDebug:"));
const fails = [];
if (!slot.length) fails.push("the level_up slot did not resolve to Level_Sting.acp");
if (played.length !== 1) fails.push(`the sting played ${played.length} times, expected 1`);
if (silent.length) fails.push("the card reported the slot silent");
if (!/card=shown|card=none/.test(state.detail || "")) fails.push("no card state answer");
console.log(fails.length ? "sting proof: FAIL\n  " + fails.join("\n  ") : "sting proof: PASS (slot resolved to the pack sting, played once, card shown)");
process.exit(fails.length ? 1 : 0);

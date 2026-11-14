import { arrive, clientBox, sessionDir } from "../session.mjs";

// #924 on a running `up --profile full` session (one client): the rebuilt market square works the way a
// player meets it. Client 1 walks to each stall and finds its action in reach: the veg stall's universal
// trader (TraderChainCounter), the fruit stall's apple and plum crates, the juice stall's juicer
// (AppleProcessor) and juice crate (AppleTradeContainer3). It is handed apples, presses one juice at the
// juicer, and opens the apple trade at the fruit stall. Pictures: a wide of the square from the street
// and a close view of each stall. Confirm in the server log: `[LFDebug:Processing]` (or the processor's
// feature tag) for the juice and no `SCRIPT (E)`.
const SQUARE = { x: 205, y: 0.3, z: 236.5 };
const STALLS = [
  { stall: "veg", x: 197.4, z: 233.5, checks: [["TraderChainCounter", "LF_OpenTraderAction"]] },
  { stall: "fruit", x: 206.4, z: 233.5, checks: [["AppleTradeContainer", "LF_OpenTraderAction"], ["PlumTradeContainer", "LF_OpenTraderAction"]] },
  { stall: "juice", x: 215.4, z: 233.5, checks: [["AppleProcessor", "LF_ProcessAction"], ["AppleTradeContainer3", "LF_OpenTraderAction"]] },
];

async function runMarketProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (spec, timeout) => {
    h.setBoxes([clientBox(dir, 1)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client 1: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  arrive(on, 1, wait);
  wait(6000);
  on({ verb: "capture", mode: "world", name: "market_wide", ...SQUARE, ox: 4, oy: 1.6, oz: 13, hour: 15 }, 30000);
  for (const s of STALLS) {
    on({ verb: "capture", mode: "world", name: `market_${s.stall}`, x: s.x, y: 1, z: s.z, ox: 0.8, oy: 0.7, oz: 3.6, hour: 15 }, 30000);
    for (const [name, action] of s.checks) {
      on({ verb: "stand", entityName: name });
      wait(1500);
      const look = on({ verb: "look" });
      if (look.status !== "ok" || !look.detail.includes(`${action}@${name}`)) fails.push(`${action} on ${name} is not in reach at the ${s.stall} stall: ${look.detail}`);
    }
  }
  const give = on({ verb: "apples" }, 10000);
  if (give.status !== "ok" || !give.detail.includes("added=")) fails.push(`apples not given: ${give.status} ${give.detail}`);
  on({ verb: "stand", entityName: "AppleProcessor" });
  wait(1500);
  on({ verb: "look" });
  const press = on({ verb: "use", className: "LF_ProcessAction", entityName: "AppleProcessor" }, 10000);
  if (press.status !== "ok") fails.push(`the juicer returned ${press.status} ${press.detail}`);
  wait(2500);
  on({ verb: "stand", entityName: "AppleTradeContainer" });
  wait(1500);
  on({ verb: "look" });
  const trade = on({ verb: "use", className: "LF_OpenTraderAction", entityName: "AppleTradeContainer" }, 10000);
  if (trade.status !== "ok") fails.push(`the apple trade returned ${trade.status} ${trade.detail}`);
  wait(1500);
  on({ verb: "capture", mode: "ui", name: "market_apple_trade" }, 25000);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "market proof: FAIL" : "market proof: PASS (every stall's action in reach, one juice pressed, the apple trade opened; confirm the juice line in the server log)");
  return fails.length ? 1 : 0;
}

export default {
  name: "market-proof",
  usage: "market-proof",
  run: (ctx) => runMarketProof(),
};

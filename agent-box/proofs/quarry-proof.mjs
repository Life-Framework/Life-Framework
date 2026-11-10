import { arrive, clientBox, parseState, sessionDir } from "../session.mjs";

// #243: the quarry is the one lawless area. One client stands at the warning sign (just outside), the quarry sign, then at the
// square sign; `state` must read zone=normal, lawless, safe. Runnable as a demo (docs/playtest.md).
async function runQuarryProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const zone = () => parseState(on(1, { verb: "state" }, 10000).detail).zone;
  const fails = [];

  arrive(on, 1, wait);
  wait(8000);
  on(1, { verb: "stand", entityName: "Sign_Lawless_Entry" });
  wait(2500);
  const gate = zone();
  wait(6000); // the sign face paints on the client once the sign streams in
  on(1, { verb: "capture", mode: "world", name: "Sign_Lawless_Entry", x: 171.5, y: 1.5, z: 207, ox: 0, oy: 1, oz: 9 }, 25000);
  on(1, { verb: "capture", mode: "world", name: "Sign_Lawless_Close", x: 171.5, y: 1.5, z: 207, ox: 0, oy: 0.5, oz: 3 }, 25000);
  on(1, { verb: "stand", entityName: "Sign_Lawless_Pit" });
  wait(2500);
  const pit = zone();
  on(1, { verb: "stand", entityName: "Sign_SafeZone_Spawn" });
  wait(2500);
  const plaza = zone();
  if (gate !== "normal") fails.push(`warning sign: zone=${gate}, expected normal (it stands just outside)`);
  if (pit !== "lawless") fails.push(`quarry sign: zone=${pit}, expected lawless`);
  if (plaza !== "safe") fails.push(`plaza: zone=${plaza}, expected safe`);
  for (const f of fails) console.log(`  ${f}`);
  console.log(fails.length ? "quarry proof: FAIL" : "quarry proof: PASS (normal at the warning sign, lawless at the quarry sign, safe on the plaza)");
  return fails.length ? 1 : 0;
}

export default {
  name: "quarry-proof",
  usage: "quarry-proof",
  run: (ctx) => runQuarryProof(),
};

import { arrive, clientBox, sessionDir } from "../session.mjs";

// #1941 the roadblock ghost on a running `session up --profile full` (one client): the ghost of the
// whole set (barrier and four cones, LF_PlacementCompositionGhost) at the open middle of the grid cell
// `roadblock` (c0r1), which the plan judges VALID, and one world picture of it. The red (blocked)
// ghost still needs a server-side blocker under it (follow-up). Deploy, pick-up and refusals are
// LF_Test_Roadblock's.
async function runRoadblockProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client 1: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];

  arrive(on, 1, wait);
  wait(6000);

  const shots = [
    { name: "roadblock_ghost_green", at: "28 41 0", want: /ghost valid/, z: 41 },
  ];
  for (const s of shots) {
    const r = on(1, { verb: "roadblockghost", words: s.at.split(" ") }, 10000);
    if (r.status !== "ok" || !s.want.test(r.detail)) fails.push(`${s.name}: expected ${s.want}, got ${r.status} ${r.detail}`);
    wait(1500);
    on(1, { verb: "capture", mode: "world", name: s.name, x: s.x ?? 28, y: 0.3, z: s.z, ox: 0, oy: 5.5, oz: -9, hour: 12 }, 25000);
  }
  on(1, { verb: "roadblockghost", words: ["off"] }, 10000);

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "roadblock proof: FAIL" : "roadblock proof: PASS (valid ghost on open ground)");
  return fails.length ? 1 : 0;
}

export default { name: "roadblock-proof", usage: "roadblock-proof", run: () => runRoadblockProof() };

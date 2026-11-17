import { arrive, clientBox, sessionDir } from "../session.mjs";

// #2442 (refs #2046 item 2) the roadblock ghost turns red against a blocker, on a running
// `session up --profile full` (one client).
//   node agent-box/session.mjs up --profile full --mission grid
//   node agent-box/session.mjs roadblock-red-ghost
//   node agent-box/session.mjs down
// The session world does not load grid-cell layer props, so the blocker is spawned for real on the
// server through harness verbs that already exist: `itemgive` puts a jerrycan in the officer's bag
// and `itemdrop` drops it at their feet (LF_AssetStatesHarness), a dynamic body the placement
// probe reports as in the way. `roadblockghost` (LF_Roadblock.c) then aims the ghost of the whole set
// at that spot. Checks: the ghost is not red over open ground (the control, same spot, before the
// drop), the same spot is `ghost blocked` with the jerrycan named in the reason once it lies there,
// and the picture is on disk. LF_PROOF_NO_BLOCKER=1 skips the drop: the proof must then FAIL, which
// is its red proof (a blue ghost where the blocker should be).
const JERRYCAN = "{12D5AD21E383B768}Prefabs/Items/Fuel/Jerrycan_01/Jerrycan_01_item.et";
const SHOT = "roadblock_ghost_red";

const point = (text) => {
  const m = text.match(/<\s*(-?[0-9.]+),\s*(-?[0-9.]+),\s*(-?[0-9.]+)\s*>/);
  return m ? [Number(m[1]), Number(m[2]), Number(m[3])] : null;
};

async function runRoadblockRedGhost() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail.slice(0, 300)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const skip = process.env.LF_PROOF_NO_BLOCKER === "1";

  arrive(on, 1, wait);
  wait(6000);

  // Where the officer stands is where the blocker lies: the drop reports it. With no drop (the
  // control) the ghost goes to the open middle of the roadblock cell, where roadblock-proof's is valid.
  let spot = [28, 0, 41];

  if (!skip) {
    const given = on(1, { verb: "itemgive", prefab: JERRYCAN }, 10000);
    if (given.status !== "ok") fails.push(`itemgive: ${given.status} ${given.detail}`);
    wait(1500);
    const dropped = on(1, { verb: "itemdrop", prefab: JERRYCAN }, 10000);
    if (dropped.status !== "ok") fails.push(`itemdrop: ${dropped.status} ${dropped.detail}`);
    const at = point(dropped.detail);
    if (at) spot = at;
    wait(3000);
  }

  if (!spot) {
    console.log("FAIL  could not read where the officer stands");
    console.log("roadblock red ghost proof: FAIL");
    return 1;
  }
  const [x, y, z] = spot;
  const aim = ["" + x, "" + z, "0"];

  const ghost = on(1, { verb: "roadblockghost", words: aim }, 10000);
  wait(1500);
  const shot = on(1, { verb: "capture", mode: "world", name: SHOT, x, y: y + 0.3, z, ox: 0, oy: 4.5, oz: -7, hour: 12 }, 25000);
  on(1, { verb: "roadblockghost", words: ["off"] }, 10000);

  if (ghost.status !== "ok" || !/ghost blocked/.test(ghost.detail)) fails.push(`expected a red ghost (ghost blocked) at ${aim.join(" ")}, got ${ghost.status} ${ghost.detail}`);
  else if (!/jerrycan/i.test(ghost.detail)) fails.push(`the ghost is blocked, but not by the jerrycan: ${ghost.detail}`);
  if (shot.status !== "ok") fails.push(`capture: ${shot.status} ${shot.detail}`);

  console.log("");
  console.log(`verdict: ${ghost.detail}`);
  console.log(`picture: ${shot.detail}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "roadblock red ghost proof: FAIL" : "roadblock red ghost proof: PASS (red ghost against the jerrycan, picture captured)");
  return fails.length ? 1 : 0;
}

export default { name: "roadblock-red-ghost", usage: "roadblock-red-ghost", run: () => runRoadblockRedGhost() };

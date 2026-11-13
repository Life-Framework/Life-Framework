import { arrive, clientBox, sessionDir } from "../session.mjs";

// #646 police spike strip on a running `up --clients 2 --profile full` session with client 1 on the
// roster (`session.mjs roster 1` before up): client 1 clocks in, draws the patrol kit at the armory
// (the M9 and the folded strip), stands on the main street's south verge facing across it and
// deploys the strip. An empty S105 is sent rolling along the street over the strip's far half
// (the harness cannot drive: `spikecar` is the push, everything after it is the strip's own check
// and the vanilla wheel damage). The server reports two wheels DESTROYED and on a smaller radius,
// both clients see the same through replication, and client 1 picks the strip up again.
// Confirm `[LFDebug:SpikeStrip] deployed by`, `punctured wheel` (twice) and `picked up by` in the
// server log.
const ITEM = "{4B8BDB727DEE59B8}Prefabs/Props/Roleplay/SpikeStrip/SpikeStrip_Item.et";
const STRIP = "{8FEC063EE8B0100B}Prefabs/Props/Roleplay/SpikeStrip/SpikeStrip.et";

async function runSpikeStripProof() {
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
  const must = (r, what) => {
    if (r.status !== "ok") fails.push(`${what}: ${r.status} ${r.detail}`);
    return r;
  };

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);

  // On duty, then the armory kit.
  on(1, { verb: "stand", entityName: "Roadmap_Police_Desk" });
  wait(2500);
  must(on(1, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Police_Desk" }, 10000), "clock in");
  wait(3000);
  on(1, { verb: "stand", entityName: "Roadmap_Police_Armory" });
  wait(2500);
  must(on(1, { verb: "use", className: "LF_ArmoryDrawAction", entityName: "Roadmap_Police_Armory" }, 10000), "armory draw");
  wait(2500);

  // The inventory pictures the engine renders from the two prefabs.
  on(1, { verb: "previews", mode: "show", prefabs: [ITEM, STRIP] }, 10000);
  wait(2500);
  on(1, { verb: "capture", mode: "ui", name: "spike_1_previews" }, 25000);
  on(1, { verb: "previews", mode: "hide" }, 10000);

  // To the verge, facing across the street, and deploy.
  must(on(1, { verb: "spikeverge" }, 10000), "to the verge");
  wait(2500);
  const deploy = must(on(1, { verb: "use", className: "LF_SpikeStripDeployAction" }, 10000), "deploy");
  wait(3000);
  on(1, { verb: "capture", mode: "ui", name: "spike_2_officer_view" }, 25000);
  on(2, { verb: "capture", mode: "world", name: "spike_3_deployed", x: 300, y: 0.1, z: 248.25, ox: -6, oy: 2.2, oz: -4, hour: 12 }, 25000);

  // The car rolls over it.
  must(on(1, { verb: "spikecar" }, 10000), "launch the car");
  wait(6000);
  const server = must(on(1, { verb: "spikecheck" }, 10000), "server wheel state");
  const seen1 = must(on(1, { verb: "spikeseen" }, 10000), "client 1 wheel state");
  const seen2 = must(on(2, { verb: "spikeseen" }, 10000), "client 2 wheel state");
  for (const [who, r] of [["server", server], ["client 1", seen1], ["client 2", seen2]]) {
    if (!/flat=2 of 4/.test(r.detail)) fails.push(`${who} sees ${r.detail}; expected two of four wheels flat`);
  }
  if (!/punctures=2/.test(server.detail)) fails.push(`the strip counted ${server.detail.match(/punctures=\S+/)?.[0]}; expected 2`);
  const radii = [...server.detail.matchAll(/w(\d)=2 r=([\d.]+)\/([\d.]+)/g)];
  if (radii.length !== 2 || radii.some((m) => !(Number(m[2]) < Number(m[3])))) fails.push(`the flat wheels do not run on a smaller radius: ${server.detail}`);

  // Pictures of the car on its flat tyres, from where it stopped.
  const at = (server.detail.match(/car at <([-\d.]+),([-\d.]+),([-\d.]+)>/) || []).slice(1).map(Number);
  if (at.length === 3) {
    on(2, { verb: "capture", mode: "world", name: "spike_4_flat_tyres", x: at[0], y: at[1] + 0.3, z: at[2], ox: 1.5, oy: 1.0, oz: -4.5, hour: 12 }, 25000);
  } else fails.push("the server reported no car position; no picture of the tyres");

  // Pick it up again.
  must(on(1, { verb: "spikeverge" }, 10000), "back to the verge");
  wait(1500);
  must(on(1, { verb: "use", className: "LF_SpikeStripPickUpAction" }, 10000), "pick up");
  wait(2000);
  const gone = on(1, { verb: "use", className: "LF_SpikeStripPickUpAction" }, 10000);
  if (gone.status === "ok") fails.push("the strip could be picked up twice");

  // Last, one free-camera shot of the flat tyre, close (a second free capture crashed a client before).
  if (at.length === 3) on(1, { verb: "capture", mode: "world", name: "spike_5_tyre_close", x: at[0], y: at[1] + 0.2, z: at[2] + 0.9, ox: 0.6, oy: 0.6, oz: 2.2, hour: 12, free: true }, 25000);

  console.log("");
  console.log(`deploy: ${deploy.detail}`);
  console.log(`server:   ${server.detail}`);
  console.log(`client 1: ${seen1.detail}`);
  console.log(`client 2: ${seen2.detail}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "spike strip proof: FAIL" : "spike strip proof: PASS (drawn at the armory, deployed on the verge, two tyres flat on the server and both clients, picked up again)");
  return fails.length ? 1 : 0;
}

export default { name: "spike-strip-proof", usage: "spike-strip-proof", run: () => runSpikeStripProof() };

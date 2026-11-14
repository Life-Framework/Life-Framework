import { arrive, clientBox, sessionDir } from "../session.mjs";

// #928 placement preview prototype (the placement foundation, docs/roadmap/building-foundation.md)
// on a running `up --clients 2 --profile full` session with client 1 on the roster (`session.mjs
// roster 1` before up). Client 1 clocks in, draws the patrol kit, stands on the main street's south
// verge facing across it and turns the deploy preview on (`spikepreview`, what selecting Deploy
// does): the ghost lies where the strip will go, green. Then it lays the real strip and turns the
// preview on again on the same spot: the ghost is red, the strip already lies there. Shots: the
// valid ghost (officer view and from the side), the placed strip from client 2 (who never sees
// client 1's ghost), the invalid ghost. Confirm `[LFDebug:SpikeStrip] preview valid`, `deployed by`
// and `preview blocked` in client 1's log and the server log.
async function runPlacementPreviewProof() {
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
  const side = { x: 300, y: 0.1, z: 248.25, ox: -6, oy: 2.2, oz: -4, hour: 12 };

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);

  // On duty, then the armory kit (the folded strip).
  on(1, { verb: "stand", entityName: "Roadmap_Police_Desk" });
  wait(2500);
  must(on(1, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Police_Desk" }, 10000), "clock in");
  wait(3000);
  on(1, { verb: "stand", entityName: "Roadmap_Police_Armory" });
  wait(2500);
  must(on(1, { verb: "use", className: "LF_ArmoryDrawAction", entityName: "Roadmap_Police_Armory" }, 10000), "armory draw");
  wait(2500);

  // The valid ghost: on the verge, facing across the street.
  must(on(1, { verb: "spikeverge" }, 10000), "to the verge");
  wait(2500);
  const valid = must(on(1, { verb: "spikepreview" }, 10000), "preview on");
  if (!/^preview valid/.test(valid.detail)) fails.push(`the first ghost is not valid: ${valid.detail}`);
  wait(1500);
  on(1, { verb: "capture", mode: "ui", name: "placement_1_valid_view" }, 25000);
  on(1, { verb: "capture", mode: "world", name: "placement_2_valid", ...side }, 25000);
  must(on(1, { verb: "spikepreviewoff" }, 10000), "preview off");

  // The real strip, laid by the server's own plan.
  must(on(1, { verb: "spikeverge" }, 10000), "back to the verge");
  wait(2000);
  must(on(1, { verb: "spikedrop" }, 10000), "drop the folded strip");
  wait(2000);
  must(on(1, { verb: "use", className: "LF_SpikeStripDeployAction" }, 10000), "deploy");
  wait(3000);
  on(2, { verb: "capture", mode: "world", name: "placement_3_placed", ...side }, 25000);

  // The invalid ghost: the same spot, the strip already lies there.
  must(on(1, { verb: "spikeverge" }, 10000), "to the verge again");
  wait(2500);
  const blocked = must(on(1, { verb: "spikepreview" }, 10000), "preview on again");
  if (!/^preview blocked/.test(blocked.detail)) fails.push(`the ghost over the laid strip is not blocked: ${blocked.detail}`);
  wait(1500);
  on(1, { verb: "capture", mode: "ui", name: "placement_4_invalid_view" }, 25000);
  on(1, { verb: "capture", mode: "world", name: "placement_5_invalid", ...side }, 25000);
  must(on(1, { verb: "spikepreviewoff" }, 10000), "preview off again");

  // Fold the strip up again (a world shot moved the officer to the camera spot).
  must(on(1, { verb: "spikeverge" }, 10000), "back to the verge to pick up");
  wait(1500);
  must(on(1, { verb: "use", className: "LF_SpikeStripPickUpAction" }, 10000), "pick up");

  console.log("");
  console.log(`valid:   ${valid.detail}`);
  console.log(`blocked: ${blocked.detail}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "placement preview proof: FAIL" : "placement preview proof: PASS (green ghost on the verge, the strip laid there, red ghost over it)");
  return fails.length ? 1 : 0;
}

export default { name: "placement-preview-proof", usage: "placement-preview-proof", run: () => runPlacementPreviewProof() };

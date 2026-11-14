import { arrive, clientBox, sessionDir } from "../session.mjs";

// #1026 placement service confirm on a running `up --clients 2 --profile full` session with client 1
// on the roster (`session.mjs roster 1` before up). Client 1 clocks in, draws the patrol kit, stands
// on the main street's south verge facing across it and shows the deploy ghost (side shot). Then it
// drops the folded strip and deploys it the way the F key does (`spikeplace`: the ghost starts, then
// ChimeraCharacter.DoStartObjectAction / DoPerformObjectAction), so the action's own data carries the
// ghost's spot to the server, which confirms it (LF_PlacementService.Confirm). Both clients then read
// where the strip lies on their machine (`spikewhere`); it must be where the ghost was. Client 2 takes
// the same side shot of the laid strip. Confirm `[LFDebug:Placement] proposal sent from the ghost` in
// client 1's log and `proposal received` and `placed SpikeStrip ... 0 m` in the server log.
const MAX_OFF = 0.1; // metres between the ghost and the strip on a client

function parseVec(text, label) {
  const m = new RegExp(`${label} <([-\\d.e]+),([-\\d.e]+),([-\\d.e]+)>`).exec(text || "");
  return m ? m.slice(1).map(Number) : null;
}

async function runPlacementConfirmProof() {
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

  // The ghost on the verge, from the side (only client 1 has it).
  must(on(1, { verb: "spikeverge" }, 10000), "to the verge");
  wait(2500);
  const preview = must(on(1, { verb: "spikepreview" }, 10000), "preview on");
  wait(1500);
  on(1, { verb: "capture", mode: "world", name: "confirm_1_ghost", ...side }, 25000);
  must(on(1, { verb: "spikepreviewoff" }, 10000), "preview off");

  // Back on the verge, drop the strip and deploy it through the engine's action path.
  must(on(1, { verb: "spikeverge" }, 10000), "back to the verge");
  wait(2500);
  must(on(1, { verb: "spikedrop" }, 10000), "drop the folded strip");
  wait(2000);
  const place = must(on(1, { verb: "spikeplace" }, 10000), "deploy with the ghost's spot");
  wait(3000);
  const ghost = parseVec(place.detail, "at");
  if (!/^ghost valid/.test(place.detail)) fails.push(`the ghost was not valid when Deploy ran: ${place.detail}`);

  const where1 = must(on(1, { verb: "spikewhere" }, 10000), "client 1 sees the strip");
  const where2 = must(on(2, { verb: "spikewhere" }, 10000), "client 2 sees the strip");
  for (const [who, r] of [["client 1", where1], ["client 2", where2]]) {
    const at = parseVec(r.detail, "strip at");
    if (!ghost || !at) {
      fails.push(`${who}: no position to compare (ghost ${place.detail}; strip ${r.detail})`);
      continue;
    }
    const off = Math.hypot(at[0] - ghost[0], at[2] - ghost[2]);
    console.log(`${who}: strip ${off.toFixed(3)} m from the ghost`);
    if (off > MAX_OFF) fails.push(`${who}: the strip lies ${off.toFixed(2)} m from where the ghost was`);
  }

  on(2, { verb: "capture", mode: "world", name: "confirm_2_placed", ...side }, 25000);
  on(1, { verb: "capture", mode: "world", name: "confirm_3_placed_officer", ...side }, 25000);

  // Fold it up again.
  must(on(1, { verb: "spikeverge" }, 10000), "back to the verge to pick up");
  wait(1500);
  must(on(1, { verb: "use", className: "LF_SpikeStripPickUpAction" }, 10000), "pick up");

  console.log("");
  console.log(`preview:  ${preview.detail}`);
  console.log(`place:    ${place.detail}`);
  console.log(`client 1: ${where1.detail}`);
  console.log(`client 2: ${where2.detail}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "placement confirm proof: FAIL" : "placement confirm proof: PASS (the officer's ghost spot went to the server with the action, the strip lies there on both clients)");
  return fails.length ? 1 : 0;
}

export default { name: "placement-confirm-proof", usage: "placement-confirm-proof", run: () => runPlacementConfirmProof() };

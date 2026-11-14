import { arrive, clientBox, sessionDir } from "../session.mjs";

// A generated building works for a player (tools\cli building, docs/building-pipeline/README.md).
// Run it on a full-quality session: a building's check is how its materials, glass and doors look,
// which the lowspec profile's lowest textures would hide.
//   node agent-box/session.mjs up --profile full
//   node agent-box/session.mjs building-proof [--entity Village_Supermarket] [--side s9] [--shot]
//   node agent-box/session.mjs down
// 1. Standing on the building's street side, outside (s9: 9 m south of its centre; its front faces
//    -z; a bare "s" is 1.2 m from the centre, which is inside a building), the F key offers the door.
// 2. The server opens the door (use SCR_DoorUserAction returns ok, not cannot_perform), and closes it again.
// 3. With --shot, the front with the door shut and open, from the client.
const DOOR_ACTION = "SCR_DoorUserAction";

function flag(args, name, fallback) {
  const i = args.indexOf(name);
  return i >= 0 && args[i + 1] ? args[i + 1] : fallback;
}

async function runBuildingProof(args = []) {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const entity = flag(args, "--entity", "Village_Supermarket");
  const side = flag(args, "--side", "s9");
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail.slice(0, 300)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const must = (r, what) => {
    if (r.status !== "ok") fails.push(`${what} returned ${r.status} ${r.detail}`);
    return r;
  };
  const shot = (name) => {
    if (!args.includes("--shot")) return;
    h.setBoxes([clientBox(dir, 1)]);
    const s = h.sendCommand({ verb: "capture", mode: "ui", name }, 25000);
    wait(3000);
    console.log(`client 1: shot ${name} -> ${h.resolveCapture(s.detail) || "not written"}`);
  };

  arrive(on, 1, wait);
  must(on(1, { verb: "stand", entityName: entity, side }), `stand on the ${side} side of ${entity}`);
  wait(3000);
  const look = must(on(1, { verb: "look" }), `look at ${entity}`);
  if (!look.detail.includes(DOOR_ACTION)) fails.push(`the F key does not offer ${DOOR_ACTION} at ${entity}: ${look.detail.slice(0, 200)}`);
  shot(`building_${entity}_shut`);

  must(on(1, { verb: "use", className: DOOR_ACTION }, 10000), "open the door");
  wait(2500);
  shot(`building_${entity}_open`);
  must(on(1, { verb: "use", className: DOOR_ACTION }, 10000), "close the door");
  wait(1500);

  if (fails.length) {
    console.log("building-proof: FAIL");
    for (const f of fails) console.log(`  ${f}`);
    return 1;
  }
  console.log("building-proof: OK");
  return 0;
}

export default { name: "building-proof", usage: "building-proof [--entity Village_Supermarket] [--side s9] [--shot]", run: (ctx) => runBuildingProof(ctx.args) };

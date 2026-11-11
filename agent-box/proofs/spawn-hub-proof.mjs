import { arrive, clientBox, parseState, sessionDir } from "../session.mjs";

// #607: the travel globe on the spawn table. Stand at the globe, use the travel entry, `ui` must list the
// area rows, click one row and `state` must put the player at that area. Runnable as a demo
// (docs/playtest.md). `--shot` takes a wide shot, a detail shot and the open list on client 1.
const AREAS = [
  { id: "bank", x: 182, z: 255 },
  { id: "farm", x: 150, z: 296 },
  { id: "jail", x: 232.35, z: 229 },
];

function posOf(detail) {
  const m = /^<?\s*(-?[\d.]+)\s*,\s*(-?[\d.]+)\s*,\s*(-?[\d.]+)\s*>?$/.exec(parseState(detail).pos ?? "");
  return m ? { x: Number(m[1]), y: Number(m[2]), z: Number(m[3]) } : null;
}

async function runSpawnHubProof(args) {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const shot = (spec) => {
    const r = on(1, spec, 25000);
    wait(3000);
    console.log(`client 1: shot ${spec.name} ${h.resolveCapture(r.detail) || "not written"}`);
  };
  const fails = [];

  arrive(on, 1, wait);
  wait(8000);
  for (const area of AREAS) {
    on(1, { verb: "stand", entityName: "SpawnHub_Globe" });
    wait(2500);
    if (area.id === "bank" && args.includes("--shot")) {
      shot({ verb: "capture", mode: "world", name: "spawn_hub_wide", x: 119, y: 1.2, z: 243.5, ox: -5, oy: 2.6, oz: -6 });
      shot({ verb: "capture", mode: "world", name: "spawn_hub_detail", x: 119.2, y: 1.0, z: 243.9, ox: 0, oy: 0.7, oz: -2.2 });
    }
    const opened = on(1, { verb: "use", className: "LF_SpawnHubAction", entityName: "SpawnHub_Globe" }, 10000);
    wait(1500);
    const ui = on(1, { verb: "ui" });
    if (opened.status !== "ok") fails.push(`${area.id}: the globe action did not run: ${opened.status} ${opened.detail}`);
    if (!ui.detail.includes(`HubGo_${area.id}`)) fails.push(`${area.id}: the list has no row HubGo_${area.id}`);
    if (area.id === "bank" && args.includes("--shot")) shot({ verb: "capture", mode: "ui", name: "spawn_hub_menu" });
    on(1, { verb: "click", widget: `HubGo_${area.id}_Action` });
    wait(4000);
    const p = posOf(on(1, { verb: "state" }, 10000).detail);
    if (!p) fails.push(`${area.id}: no position in state`);
    else if (Math.hypot(p.x - area.x, p.z - area.z) > 3) fails.push(`${area.id}: player at ${p.x},${p.z}, expected near ${area.x},${area.z}`);
  }
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "spawn-hub-proof: FAIL" : "spawn-hub-proof: PASS (the globe list opened and each pick put the player at the area)");
  return fails.length ? 1 : 0;
}

export default {
  name: "spawn-hub-proof",
  usage: "spawn-hub-proof [--shot]",
  run: (ctx) => runSpawnHubProof(ctx.args),
};

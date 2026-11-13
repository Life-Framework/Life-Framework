import { arrive, clientBox, newestLog, parseState, sessionDir } from "../session.mjs";

// #664 handcuffs held in the officer's left hand, on a running `up --clients 2 --profile full`
// session with `session.mjs roster 1` before `up`. Client 1 clocks in (the kit carries the cuffs)
// and takes them in hand; client 1 shoots its own view, then client 2's free camera shoots the
// hand from in front (its one free capture). Args: optional "ox oy oz" camera offset, default
// 0.9 -0.6 0.0 from 1.1 m above the officer's feet, looking at the officer.
async function runCuffsHeldProof(ctx) {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const state = (n) => parseState(on(n, { verb: "state" }, 10000).detail);
  const fails = [];
  const [ox, oy, oz] = (ctx?.args?.length >= 3 ? ctx.args.slice(0, 3) : ["0.9", "-0.6", "0.0"]).map(Number);

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  on(1, { verb: "stand", entityName: "Roadmap_Police_Desk", side: "s" });
  wait(2500);
  on(1, { verb: "look" });
  const clockIn = on(1, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Police_Desk" }, 10000);
  if (clockIn.status !== "ok") fails.push(`clock-in returned ${clockIn.status} ${clockIn.detail}`);
  wait(4000);
  const held = on(1, { verb: "hold", item: "Handcuffs" }, 10000);
  if (held.status !== "ok") fails.push(`holding the cuffs returned ${held.status} ${held.detail}`);
  wait(4000);
  on(1, { verb: "capture", mode: "ui", name: "cuffs_held_own_view" }, 25000);
  const at = String(state(1).pos ?? "").split(",").map(Number);
  if (at.length === 3 && at.every(Number.isFinite)) {
    on(2, { verb: "capture", mode: "world", name: "cuffs_held_front", x: at[0], y: at[1] + 1.1, z: at[2], ox, oy, oz, hour: 12, free: true }, 25000);
  } else fails.push("no position for client 1; the held cuffs were not shot");
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "cuffs-held proof: FAIL" : "cuffs-held proof: done (read the two shots)");
  return fails.length ? 1 : 0;
}

export default { name: "cuffs-held-proof", usage: "cuffs-held-proof [ox oy oz]", run: (ctx) => runCuffsHeldProof(ctx) };

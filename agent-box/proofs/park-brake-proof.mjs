import { arrive, clientBox, sessionDir } from "../session.mjs";

// Park brake (#979): a car set down on a slope stays put, and still drives off.
//   node agent-box/session.mjs up --profile full     (one client)
//   node agent-box/session.mjs park-brake-proof
//   node agent-box/session.mjs down
// DebugWorld's terrain is flat, so `slopecar` sets a base-game concrete vehicle ramp on open ground
// and an empty S105 on it (LF_ParkBrakeHarness), kept awake so only the brake holds it. The proof reads
// the server's position three times across HOLD_S seconds, then lets go of the brake and gives it
// throttle the way a driver does: the same car must leave, which shows the slope would move it.
const HOLD_S = 60;
const MAX_DRIFT_M = 0.5;
const MIN_TILT_DEG = 8;
const MIN_LEFT_M = 2;

const num = (text, key) => {
  const m = text.match(new RegExp(`${key}=(-?[0-9.]+)`));
  return m ? Number(m[1]) : NaN;
};
const at = (text) => {
  const m = text.match(/at=<\s*(-?[0-9.]+),\s*(-?[0-9.]+),\s*(-?[0-9.]+)\s*>/);
  return m ? [Number(m[1]), Number(m[2]), Number(m[3])] : null;
};

async function runParkBrakeProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail.slice(0, 240)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const must = (r, what) => {
    if (r.status !== "ok") fails.push(`${what} returned ${r.status} ${r.detail}`);
    return r;
  };

  arrive(on, 1, wait);
  const spawn = must(on(1, { verb: "slopecar" }, 15000), "set the car on the slope");
  const tilt = num(spawn.detail, "tilt");
  if (!(tilt >= MIN_TILT_DEG)) fails.push(`the ramp is only ${tilt} deg, a proof needs at least ${MIN_TILT_DEG}`);
  console.log(`slope tilt ${tilt} deg, brake on spawn: ${/brake=(\w+)/.exec(spawn.detail)?.[1]}`);

  // Let the car settle onto its wheels before the first reading, then read it three times
  // across HOLD_S: a settle shows as a first step that shrinks, a creep as equal steps.
  wait(8000);
  const readings = [];
  for (let i = 0; i < 3; i++) {
    if (i > 0) wait((HOLD_S / 2) * 1000);
    const r = must(on(1, { verb: "slopecheck" }, 10000), `reading ${i + 1}`);
    readings.push(at(r.detail));
  }
  if (readings.some((p) => !p)) {
    fails.push(`could not read positions: ${JSON.stringify(readings)}`);
  } else {
    const step = (p, q) => Math.hypot(q[0] - p[0], q[2] - p[2]);
    const half1 = step(readings[0], readings[1]);
    const half2 = step(readings[1], readings[2]);
    const drift = step(readings[0], readings[2]);
    console.log(`position delta over ${HOLD_S} s: ${drift.toFixed(3)} m (first half ${half1.toFixed(3)}, second half ${half2.toFixed(3)}; limit ${MAX_DRIFT_M} m)`);
    if (drift >= MAX_DRIFT_M) fails.push(`the car moved ${drift.toFixed(2)} m in ${HOLD_S} s on a ${tilt} deg slope`);
  }
  const a = readings[0];

  // Driving off: the brake lets go and the car leaves.
  must(on(1, { verb: "slopedrive" }, 10000), "release the brake and give throttle");
  wait(8000);
  const gone = must(on(1, { verb: "slopecheck" }, 10000), "reading after driving off");
  const c = at(gone.detail);
  if (a && c) {
    const left = Math.hypot(c[0] - a[0], c[2] - a[2]);
    console.log(`after release and throttle: moved ${left.toFixed(2)} m, speed ${num(gone.detail, "speed")} km/h, brake=${/brake=(\w+)/.exec(gone.detail)?.[1]}`);
    if (left < MIN_LEFT_M) fails.push(`the car did not leave after the brake was released (${left.toFixed(2)} m)`);
  }

  if (fails.length) {
    console.log("park-brake-proof: FAIL");
    for (const f of fails) console.log(`  ${f}`);
    return 1;
  }
  console.log("park-brake-proof: OK");
  return 0;
}

export default { name: "park-brake-proof", usage: "park-brake-proof", run: () => runParkBrakeProof() };

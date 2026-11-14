import { arrive, clientBox, sessionDir } from "../session.mjs";

// #921 vehicle HUD on a running session: arrive, stand at the lot's blue UAZ, take the wheel, start the
// engine, switch the lights and high beams on and shoot the screen. The vehicle's dashboard indicators
// must sit on the dash or the HUD edge, never in the middle of the screen.
async function runVehicleHudProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  arrive(on, 1, wait);
  wait(6000);
  on(1, { verb: "stand", entityName: "Lot_Display_uaz469" }, 15000);
  wait(1500);
  const seat = on(1, { verb: "engine", entityName: "Lot_Display_uaz469" }, 15000);
  wait(4000);
  on(1, { verb: "engine" }, 10000);
  wait(2000);
  on(1, { verb: "input", action: "VehicleLightsToggle" }, 10000);
  wait(1200);
  on(1, { verb: "input", action: "CarLightsHiBeamToggle" }, 10000);
  wait(1500);
  on(1, { verb: "capture", mode: "ui", name: process.env.LF_SHOT_NAME || "vehicle_hud" }, 25000);
  const ok = seat.status === "ok";
  console.log(ok ? "vehicle hud proof: shot taken" : "vehicle hud proof: FAIL (could not take the wheel)");
  return ok ? 0 : 1;
}

export default { name: "vehicle-hud-proof", usage: "vehicle-hud-proof", run: () => runVehicleHudProof() };

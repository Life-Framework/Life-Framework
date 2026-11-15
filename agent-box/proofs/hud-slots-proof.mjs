import { arrive, clientBox, sessionDir } from "../session.mjs";

// wb-fast-play: the player HUD after arrival with every vanilla HUD slot in EL_HUDManagerRoot.layout.
// Arrive, let the HUD settle (notifications and hints land in their slots, top right), and shoot the
// screen. Before the fix the screen effects VMEd on a NULL m_wRoot and chat, VON, notifications,
// hints and the game version had no slot.
async function runHudSlotsProof() {
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
  wait(5000);
  const shot = on(1, { verb: "capture", mode: "ui", name: process.env.LF_SHOT_NAME || "hud_slots" }, 25000);
  const ok = shot.status === "ok";
  console.log(ok ? "hud slots proof: shot taken" : "hud slots proof: FAIL (no shot)");
  return ok ? 0 : 1;
}

export default { name: "hud-slots-proof", usage: "hud-slots-proof", run: () => runHudSlotsProof() };

import { arrive, clientBox, parseState, sessionDir } from "../session.mjs";

// The downed flow of #5154 and #5155 on the real island, two players, one session.
//   node agent-box/session.mjs up --mission island --clients 2 --profile full --hold 20
//   node agent-box/session.mjs downed-flow-proof
//   node agent-box/session.mjs pull <dir>      (pictures; then `down`)
// Client 1 clocks in at the Saint-Philippe clinic desk and downs client 2 (a harness `shoot`; the
// harness has no fall damage), so client 1 is the downer. Reads:
//   #5155 action order: client 1 faces the body; `look` lists Revive before Introduce.
//   #5155 HUD line: client 2's `seen downedhud=` shows the wait, then the ready line.
//   #5154 VME: client 2 gives up while the downer is online; the three console logs carry no
//   `GetCharacterControlType` exception (the caller greps the logs after `down`; this proof
//   prints their directory).
const DESK = "Fit_saint_philippe_clinic_lf_medic_duty_desk";

async function runDownedFlowProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail.slice(0, 600)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const shot = (n, name) => on(n, { verb: "capture", mode: "ui", name }, 25000);
  const hud = (n) => /downedhud=(\S+)/.exec(on(n, { verb: "seen" }, 10000).detail)?.[1] ?? "";

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  on(1, { verb: "stand", entityName: DESK });
  wait(2500);
  on(1, { verb: "look" });
  on(1, { verb: "use", className: "LF_DutyAction", entityName: DESK }, 10000);
  wait(2500);
  if (!parseState(on(1, { verb: "state" }, 10000).detail).faction?.includes("MEDIC")) fails.push("client 1 did not clock in as a medic");
  on(2, { verb: "stand", entityName: DESK });
  wait(2500);

  on(1, { verb: "shoot", playerId: 2 }, 10000);
  wait(3000);
  const patient = `Player${parseState(on(2, { verb: "state" }, 10000).detail).player}`;
  const wait1 = hud(2);
  shot(2, "downed_hud_wait");
  if (!/Bleeding_out/.test(wait1) || !/give_up_in/.test(wait1)) fails.push(`client 2 HUD line while waiting: "${wait1}"`);

  // The medic's list, the body in front of them.
  on(1, { verb: "stand", entityName: patient });
  wait(2500);
  const list = on(1, { verb: "look" }, 10000).detail;
  shot(1, "medic_action_order");
  const revive = list.search(/LF_ReviveAction|Revive/);
  const intro = list.search(/LF_IntroduceAction|Introduce/);
  if (revive < 0) fails.push("Revive is not in the medic's list");
  else if (intro >= 0 && revive > intro) fails.push("Revive is listed after Introduce");

  wait(22000);
  const mid = hud(2);
  shot(2, "downed_hud_mid");
  wait(12000);
  const ready = hud(2);
  shot(2, "downed_hud_ready");
  if (!/Hold_X_to_give_up/.test(ready)) fails.push(`client 2 HUD line after the delay: "${ready}"`);

  // Give up with the downer (client 1) still online: the #5154 path.
  on(2, { verb: "giveup" }, 10000);
  wait(4000);
  shot(2, "death_screen_after_giveup");
  const after = hud(2);
  if (after !== "none") fails.push(`the HUD line stayed after death: "${after}"`);
  on(2, { verb: "click", widget: "RespawnButton" });
  wait(10000);

  console.log("");
  console.log(`hud waiting: ${wait1}`);
  console.log(`hud mid:     ${mid}`);
  console.log(`hud ready:   ${ready}`);
  console.log(`hud dead:    ${after}`);
  console.log(`medic list:  ${list.slice(0, 600)}`);
  console.log("now: pull the pictures, `down`, then grep the three console.log files for GetCharacterControlType (must be none)");
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "downed flow proof: FAIL" : "downed flow proof: PASS (Revive first, HUD line counts down and opens, give-up ran; read the logs for the VME)");
  return fails.length ? 1 : 0;
}

export default {
  name: "downed-flow-proof",
  usage: "downed-flow-proof   (up --mission island --clients 2 --profile full --hold 20)",
  run: () => runDownedFlowProof(),
};

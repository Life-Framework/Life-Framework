import { arrive, clientBox, parseState, sessionDir } from "../session.mjs";

// The downed flow of #5154 and #5155 on the real island, two players, one session.
//   node agent-box/session.mjs up --mission island --clients 2 --profile full --hold 20
//   node agent-box/session.mjs downed-flow-proof
//   node agent-box/session.mjs pull <dir>      (pictures; then `down`)
// Client 1 clocks in at the Saint-Philippe clinic desk and downs client 2 (a harness `shoot`; the
// harness has no fall damage), so client 1 is the downer. Reads:
//   #5155 action order: client 1 faces the body; the in-game menu (`aim`) puts Revive first and selected.
//   #5155 HUD line: client 2's `seen downedhud=` shows the wait, then the ready line.
//   #5154 VME: client 2 gives up while the downer is online; the three console logs carry no
//   `GetCharacterControlType` exception (the caller greps the logs after `down`; this proof
//   prints their directory).
const DESK = "Fit_saint_philippe_clinic_lf_medic_duty_desk";
// Open ground outside every safe zone, away from the clinic's doctor and till (the arrival lamps are a safe zone: a
// `shoot` there is refused; inside the clinic the view ray meets the doctor or the till first).
// A medic looks straight at the body. At -50 and steeper the aim sits on the body itself (the vanilla LowerTorso context,
// which used to offer Loot and Inspect and no Revive); only a glance above it (-30) used to reach the LF_ID context.
const AIM_PITCH = -60;
const DOWN_SPOT ="Fit_saint_philippe_dock_bus_stops_drop_b03";

// The medic's action menu on the body. `aim` is the real thing: it prints `GetFilteredActions` of the player's
// interaction handler, the list the in-game menu shows and the F key acts on, in menu order, with `selected=` the row the
// F key would run (#5155, #5182). `look` is NOT the menu: it walks each prop's actions in declaration order, whatever their
// Sort Priority, and stops at 12 hits across everything within 4 m, so near the clinic it never reached Revive.
//   { state: "pass" }               the menu's first row, and the selected one, is Revive.
//   { state: "unsettled", why }     aim lands on something else (the clinic doctor, the till: `context=` says what) or Revive
//                                   is not first: read again.
export function judgeActionOrder(aim) {
  const text = String(aim ?? "");
  const offered = /offered=(\S+)/.exec(text)?.[1] ?? "";
  const rows = offered === "" || offered === "none" ? [] : offered.split(",").map((r) => r.replace(/:\d$/, ""));
  const revive = rows.findIndex((r) => /^LF_ReviveAction/.test(r));
  const context = /context=(\S+)/.exec(text)?.[1] ?? "none";
  if (revive < 0) return { state: "unsettled", why: `Revive is not in the medic's menu (aim context=${context}, rows: ${rows.join(" ") || "none"})` };
  if (revive > 0) return { state: "unsettled", why: `Revive is row ${revive + 1} of the menu, after ${rows.slice(0, revive).join(" ")}` };
  if (!/selected=LF_ReviveAction/.test(text)) return { state: "unsettled", why: "Revive is first but not the selected row" };
  return { state: "pass", why: "" };
}

export const LIST_READS = 5;

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
  on(1, { verb: "use", className: "LF_DutyAction", entityName: DESK }, 10000);
  wait(2500);
  if (!parseState(on(1, { verb: "state" }, 10000).detail).faction?.includes("MEDIC")) fails.push("client 1 did not clock in as a medic");
  // The body goes down in the open. At the duty desk the clinic's doctor stands in the view ray, so the medic's aim lands on
  // him ("Talk") and the body's own menu never shows; that is where the player is looking, not a missing Revive.
  on(2, { verb: "stand", entityName: DOWN_SPOT });
  wait(2500);
  on(1, { verb: "stand", entityName: DOWN_SPOT, side: "s" });
  wait(2500);

  on(1, { verb: "shoot", playerId: 2 }, 10000);
  wait(3000);
  const patient = `Player${parseState(on(2, { verb: "state" }, 10000).detail).player}`;
  const wait1 = hud(2);
  shot(2, "downed_hud_wait");
  if (!/Bleeding_out/.test(wait1) || !/give_up_in/.test(wait1)) fails.push(`client 2 HUD line while waiting: "${wait1}"`);

  // The medic's menu, the body in front of them: stand beside the body, look down at it, and `aim` until the menu's first
  // and selected row is Revive. A read before the view settles can still name the last thing aimed at. Only the last read
  // is judged.
  let list = "";
  let verdict = { state: "unsettled", why: "no menu read" };
  for (let read = 1; read <= LIST_READS; read++) {
    on(1, { verb: "stand", entityName: patient });
    wait(2500);
    on(1, { verb: "input", action: "CharacterPitch", value: AIM_PITCH }, 8000);
    wait(800);
    list = on(1, { verb: "aim", words: [] }, 10000).detail;
    verdict = judgeActionOrder(list);
    if (verdict.state === "pass") break;
    console.log(`medic menu read ${read}/${LIST_READS} not settled: ${verdict.why}`);
  }
  shot(1, "medic_action_order");
  on(1, { verb: "input", action: "CharacterPitch", value: 0 }, 8000);
  if (verdict.state !== "pass") fails.push(`${verdict.why} (after ${LIST_READS} reads)`);

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

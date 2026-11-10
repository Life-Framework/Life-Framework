import { readFileSync } from "node:fs";
import { join } from "node:path";
import { ROB_TARGET, ROOT, arrive, clientBox, judgeBleed, judgeDowned, parsePeer, parseState, sessionDir } from "../session.mjs";
import { ensureClient, makeClientSees } from "./_clientSees.mjs";

async function runDownedProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const medicConf = readFileSync(join(ROOT, "addons", "LifeFramework", "Configs", "Medic", "LF_Medic.conf"), "utf8");
  if (/m_bDownedEnabled\s+0/.test(medicConf)) {
    console.log("downed proof: m_bDownedEnabled is 0 in LF_Medic.conf; set it to 1 for the run");
    return 1;
  }
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const state = (n) => on(n, { verb: "state" }, 10000).detail;
  const fails = [];
  const sees = makeClientSees({ on, wait, dir });

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  const start1 = state(1);
  const start2 = state(2);

  on(2, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  on(1, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  const shot = on(1, { verb: "shoot", playerId: 2 }, 10000);
  wait(2500);
  on(1, { verb: "capture", mode: "ui", name: "m2_255_downed_client1" }, 25000);
  on(2, { verb: "capture", mode: "ui", name: "m2_255_downed_client2" }, 25000);
  const victim = state(2);
  const shooter = state(1);
  const peers1 = on(1, { verb: "peers" }, 10000).detail;
  const peers2 = on(2, { verb: "peers" }, 10000).detail;
  // The body on the ground, seen from 2.5 m away (a world shot from the downed player's position).
  const [bx, by, bz] = String(parseState(victim).pos ?? "0,0,0").split(",").map(Number);
  if (Number.isFinite(bx)) on(1, { verb: "capture", mode: "world", name: "m2_255_downed_body", x: bx, y: by + 0.3, z: bz, ox: 2.5, oy: 1.2, oz: 0 }, 25000);
  for (const f of judgeDowned(shot.detail, victim, shooter, peers1, peers2)) fails.push(f);

  // A second lethal hit changes nothing: a downed player takes no further damage.
  const again = on(1, { verb: "shoot", playerId: 2 }, 10000);
  wait(1500);
  const victimAgain = parseState(state(2));
  if (victimAgain.downed !== "1" || victimAgain.life !== "INCAPACITATED") fails.push(`after a second hit: downed=${victimAgain.downed} life=${victimAgain.life}`);

  // Late joiner: client 3 connects after the downing and must read client 2 as downed.
  let peers3 = "";
  if (process.argv.includes("--no-late-join")) {
    console.log("late join skipped (--no-late-join)");
  } else {
    // #504: a second run in one session finds client 3 still connected: reuse it instead of joining again.
    const lateJoin = ensureClient(on, wait, 3);
    if (lateJoin === "failed") {
      fails.push("client 3 did not join late");
    } else {
      on(3, { verb: "stand", entityName: ROB_TARGET });
      wait(4000);
      peers3 = on(3, { verb: "peers" }, 10000).detail;
      on(3, { verb: "capture", mode: "ui", name: "m2_255_downed_latejoin_client3" }, 25000);
      if (!/player=2 downed=1 life=INCAPACITATED/.test(peers3)) fails.push(`the late joiner does not see player 2 downed: ${peers3}`);
    }
  }

  // #259: a medic revives. Client 1 clocks in at the hospital desk, walks to the downed client 2
  // and revives them: client 2 is standing with health above zero, keeps what they carried, and the
  // bleed-out clock is gone. A civilian is not offered the entry (client 1 before clocking in is
  // refused by the server). Then client 1 clocks out and downs client 2 again for the rest of the proof.
  const patientName = `Player${parseState(state(2)).player}`;
  const cashBefore = parseState(state(2)).cash;
  const civilianTry = on(1, { verb: "use", className: "LF_ReviveAction", entityName: patientName }, 10000);
  wait(1500);
  const afterCivilian = parseState(state(2));
  if (afterCivilian.downed !== "1") fails.push(`a civilian's revive changed the patient: downed=${afterCivilian.downed} (${civilianTry.status} ${civilianTry.detail})`);
  on(1, { verb: "stand", entityName: "Roadmap_Hospital_Duty_Desk" });
  wait(2500);
  on(1, { verb: "look" });
  const medicIn = on(1, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Hospital_Duty_Desk" }, 10000);
  wait(2500);
  if (!parseState(state(1)).faction?.includes("MEDIC")) fails.push(`client 1 did not clock in as a medic: ${medicIn.status} ${medicIn.detail}`);
  on(1, { verb: "stand", entityName: patientName });
  wait(2500);
  on(1, { verb: "look" });
  const [px, py, pz] = String(parseState(state(2)).pos ?? "0,0,0").split(",").map(Number);
  const seeBody = (name) => { if (Number.isFinite(px)) on(1, { verb: "capture", mode: "world", name, x: px, y: py + 0.3, z: pz, ox: 2.5, oy: 1.2, oz: 0 }, 25000); };
  seeBody("m2_259_before_revive_world");
  // The world capture moves the view: stand beside the patient again before the action.
  on(1, { verb: "stand", entityName: patientName });
  wait(2500);
  on(1, { verb: "look" });
  // #522: what client 1 sees of the body: the downed pose (replicated flag plus vanilla unconscious state).
  const pose1 = parsePeer(sees.peers(1), 2);
  if (!pose1 || pose1.downed !== "1" || pose1.life !== "INCAPACITATED") fails.push(`client 1 does not see the downed pose before the revive: ${JSON.stringify(pose1)}`);
  const revive = on(1, { verb: "use", className: "LF_ReviveAction", entityName: patientName }, 10000);
  // The vanilla wake takes a few seconds after the downed state ends.
  let revived = parseState(state(2));
  for (let i = 0; i < 8 && revived.life !== "ALIVE"; i++) {
    wait(2000);
    revived = parseState(state(2));
  }
  // #522: and after the revive client 1 sees the body standing again.
  const up1 = parsePeer(sees.peers(1), 2);
  if (!up1 || up1.downed !== "0" || up1.life !== "ALIVE") fails.push(`client 1 does not see client 2 up after the revive: ${JSON.stringify(up1)}`);
  on(2, { verb: "capture", mode: "ui", name: "m2_259_revived_client2" }, 25000);
  seeBody("m2_259_after_revive_world");
  on(1, { verb: "stand", entityName: patientName });
  wait(1500);
  if (revive.status !== "ok") fails.push(`revive returned ${revive.status} ${revive.detail}`);
  if (revived.downed !== "0" || revived.life !== "ALIVE" || revived.bleed !== "-1") fails.push(`after the revive: downed=${revived.downed} life=${revived.life} bleed=${revived.bleed}`);
  if (revived.cash !== cashBefore) fails.push(`the revived player's cash changed: ${cashBefore} -> ${revived.cash}`);
  on(1, { verb: "stand", entityName: "Roadmap_Hospital_Duty_Desk" });
  wait(2500);
  on(1, { verb: "look" });
  on(1, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Hospital_Duty_Desk" }, 10000);
  wait(2500);
  on(2, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  on(1, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  on(1, { verb: "shoot", playerId: 2 }, 10000);
  wait(2500);
  if (parseState(state(2)).downed !== "1") fails.push("client 2 was not downed again after the revive");
  console.log(`revive   client 2: ${JSON.stringify(revived)} (civilian try: ${civilianTry.detail})`);

  // The real death of a downed player (what the bleed-out will call): the death screen, the
  // respawn, and a fresh body that is not downed.
  // #256: the client reads the time left (a replicated, throttled copy); giving up is refused
  // before the delay and accepted after it, and then it is the real death.
  const bleedConf = medicConf;
  const bleedSeconds = Number(/m_fBleedOutSecondss+(d+)/.exec(bleedConf)?.[1] ?? 180);
  const giveUpDelay = Number(/m_fGiveUpDelaySecondss+(d+)/.exec(bleedConf)?.[1] ?? 30);
  const bleedRead = parseState(state(2)).bleed;
  for (const f of judgeBleed(bleedRead, bleedSeconds)) fails.push(f);
  const early = on(2, { verb: "giveup" }, 10000);
  wait(2000);
  const afterEarly = parseState(state(2));
  if (afterEarly.downed !== "1" || afterEarly.life !== "INCAPACITATED") fails.push(`an early give-up changed the state: downed=${afterEarly.downed} life=${afterEarly.life}`);
  wait((giveUpDelay + 2) * 1000);
  const late = on(2, { verb: "giveup" }, 10000);
  if (late.status !== "ok") fails.push(`giveup returned ${late.status} ${late.detail}`);
  wait(4000);
  on(2, { verb: "capture", mode: "ui", name: "m2_255_death_screen_client2" }, 25000);
  // Vanilla keeps the instigator of the knockout as the killer when the downed player dies, so the
  // murder is the downer's, on the real death only.
  const wantedAfterDeath = parseState(state(1)).wanted;
  if (wantedAfterDeath !== "3") fails.push(`shooter wanted=${wantedAfterDeath} after the downed player died, expected 3 (murder attributed to the downer)`);
  on(2, { verb: "click", widget: "RespawnButton" });
  wait(12000);
  const respawned = parseState(state(2));
  if (respawned.downed !== "0" || respawned.life !== "ALIVE") fails.push(`after the respawn: downed=${respawned.downed} life=${respawned.life}`);

  // #256: the bleed-out timer. Down client 2 again and leave them alone: the time runs out, the
  // death is the same real death, and the respawn is the normal one.
  on(2, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  const shot2 = on(1, { verb: "shoot", playerId: 2 }, 10000);
  wait(2500);
  const down2 = parseState(state(2));
  if (down2.downed !== "1") fails.push(`the second downing did not down: ${shot2.detail}`);
  wait((bleedSeconds + 8) * 1000);
  const bled = parseState(state(2));
  // A dead player has no body to answer `state` (no_session), which is the expected end.
  if (bled.downed === "1" || bled.life === "ALIVE" || bled.life === "INCAPACITATED") fails.push(`after the timer: downed=${bled.downed} life=${bled.life}, expected dead`);
  on(2, { verb: "capture", mode: "ui", name: "m2_256_bled_out_client2" }, 25000);
  on(2, { verb: "click", widget: "RespawnButton" });
  wait(12000);
  const woke = parseState(state(2));
  if (woke.downed !== "0" || woke.life !== "ALIVE") fails.push(`after the bleed-out respawn: downed=${woke.downed} life=${woke.life}`);

  console.log("");
  console.log(`start    client 1: ${start1}`);
  console.log(`start    client 2: ${start2}`);
  console.log(`shot     client 1: ${shot.detail}`);
  console.log(`bleed    client 2: ${bleedRead} (early give-up: ${early.detail}, then ${late.detail})`);
  console.log(`bled out client 2: ${JSON.stringify(bled)}; respawn ${JSON.stringify(woke)}`);
  console.log(`victim   client 2: ${victim}`);
  console.log(`shooter  client 1: ${shooter}`);
  console.log(`peers    client 1: ${peers1}`);
  console.log(`peers    client 2: ${peers2}`);
  console.log(`again    client 1: ${again.detail}`);
  console.log(`peers    client 3 (late joiner): ${peers3 || "skipped"}`);
  console.log(`respawn  client 2: ${JSON.stringify(respawned)}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "downed proof: FAIL" : "downed proof: PASS (downed, not dead, no murder; seen downed by both clients and a late joiner; a medic revives; real death is the murder and respawns)");
  return fails.length ? 1 : 0;
}

export default {
  name: "downed-proof",
  usage: "downed-proof",
  run: (ctx) => runDownedProof(),
};

import { join } from "node:path";
import { arrive, clientBox, clientSees, sessionDir } from "../session.mjs";

// The taser's sounds (#341, asset pipeline sound stage) on a running
// `up --clients 2 --profile full --sound` session. An agent cannot listen, so this asks the engine
// (harness `sound`, LF_HarnessSound):
//  - client 1 draws the taser; its sound component knows SOUND_SHOT, SOUND_DRY and the two reload
//    events and starts a voice for each when asked to play it, and does not know
//    SOUND_RELOAD_BOLT_PULL, which only the vanilla M9 handling project has: the taser plays its
//    own projects, not the M9's;
//  - client 1 stuns client 2: both clients start the stun buzz on client 2's body (LF_StunBuzz,
//    driven by the replicated stun flag) and both stop it when the stun runs out.
// On a session with sound, what client 1's process plays is recorded to
// <session>/taser-sound-client1.wav (tools/film/audio.mjs): the four weapon sounds 1.5 s apart, then
// the buzz. Whether the sounds are good is for ears: that file and the PR's manual steps.
const TASER = "Roadmap_Police_Taser";
const DESK = "Roadmap_Police_Desk";
const STUN_SECONDS = 8;
const OWN = ["SOUND_SHOT", "SOUND_DRY", "SOUND_RELOAD_MAG_REMOVE", "SOUND_RELOAD_MAG_INSERT"];
const yes = "(1|true)";
const no = "(0|false)";

// `played` is the event the probe was asked to play.
export function soundFails(detail, played = "") {
  const fails = [];
  if (!/weapon=Handgun_Taser\.et/.test(detail)) fails.push(`the weapon in hand is not the taser: ${detail}`);
  for (const e of OWN) if (!new RegExp(`${e}=known:${yes},played:${e === played ? yes : no}`).test(detail)) fails.push(`${e} is not known${e === played ? " and played" : ""} on the taser`);
  if (!new RegExp(`SOUND_RELOAD_BOLT_PULL=known:${no}`).test(detail)) fails.push("SOUND_RELOAD_BOLT_PULL is known: the M9 handling project is still on the taser");
  if (!new RegExp(`stun_loop_audible=${yes}`).test(detail)) fails.push("the stun loop's project and event are not audible at the player");
  return fails;
}

async function runTaserSoundProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const buzz = (n) => {
    const d = on(n, { verb: "sound" }, 10000).detail;
    return { playing: Number(/stun_buzz_playing=(\d+)/.exec(d)?.[1]), started: Number(/started=(\d+)/.exec(d)?.[1]), stopped: Number(/stopped=(\d+)/.exec(d)?.[1]) };
  };

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  on(1, { verb: "stand", entityName: TASER });
  wait(2000);
  const equip = on(1, { verb: "use", className: "SCR_EquipWeaponAction", entityName: TASER }, 10000);
  if (equip.status !== "ok") fails.push(`equipping the taser returned ${equip.status} ${equip.detail}`);
  wait(4000);
  on(1, { verb: "input", action: "SwitchWeaponCategory2" });
  wait(4000);

  // Record what client 1's process plays, when the session has sound.
  let capture = null;
  try {
    const audio = await import("../../tools/film/audio.mjs");
    capture = await audio.startCapture({ pid: audio.clientPid(join(dir, "session.json"), 1), wav: join(dir, "taser-sound-client1.wav") });
  } catch (e) {
    console.log(`no audio capture: ${e.message}`);
  }

  let probe;
  for (const e of OWN) {
    probe = on(1, { verb: "sound", event: e }, 10000);
    fails.push(...soundFails(probe.detail, e));
    wait(1500);
  }

  on(2, { verb: "stand", entityName: DESK });
  wait(3000);
  const before = [buzz(1), buzz(2)];
  const shot = on(1, { verb: "shoot", playerId: 2 }, 10000);
  if (!/downed=1/.test(shot.detail)) fails.push(`the hit with the taser in hand returned "${shot.detail}", expected downed=1`);
  const stunnedAt = Date.now();
  const down = clientSees(on, wait, 2, (s) => (s.downed === "1" ? [] : [`downed=${s.downed}`]), { label: "after the taser hit", budgetMs: 10000 });
  fails.push(...down.fails);
  const during = [buzz(1), buzz(2)];
  for (const n of [1, 2]) {
    const d = during[n - 1];
    if (!(d.playing === 1 && d.started === before[n - 1].started + 1)) fails.push(`client ${n} did not start the stun buzz: ${JSON.stringify(d)}`);
  }
  const left = STUN_SECONDS * 1000 - (Date.now() - stunnedAt);
  if (left > 0) wait(left + 2500);
  const after = [buzz(1), buzz(2)];
  for (const n of [1, 2]) {
    const d = after[n - 1];
    if (!(d.playing === 0 && d.stopped === before[n - 1].stopped + 1)) fails.push(`client ${n} did not stop the stun buzz when the stun ran out: ${JSON.stringify(d)}`);
  }

  if (capture) {
    const done = await capture.stop();
    console.log(`recorded ${capture.wav}: peak ${done.peakDb} dB`);
    if (!(done.peakDb > -60)) fails.push(`client 1 played nothing audible (peak ${done.peakDb} dB); is the session up with --sound?`);
  }

  console.log("");
  console.log(`weapon sounds: ${probe.detail}`);
  console.log(`stun buzz while stunned: ${JSON.stringify(during)}; after: ${JSON.stringify(after)}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "taser sound proof: FAIL" : "taser sound proof: PASS (the taser plays its own shot, dry-fire and reload events and none of the M9's handling; both clients start the stun buzz with the stun and stop it at its end)");
  return fails.length ? 1 : 0;
}

export default { name: "taser-sound-proof", usage: "taser-sound-proof", run: () => runTaserSoundProof() };

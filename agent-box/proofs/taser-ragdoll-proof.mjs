import { arrive, clientBox, clientSees, parseState, sessionDir } from "../session.mjs";
import { DESK, TASER } from "./taser-proof.mjs";

// #1857 the taser's down state, on a running `up --clients 2 --profile full` session: client 1 draws
// the taser and hits client 2; client 2's body ragdolls (read on the owner, client 2, and on the
// peer, client 1), stays down and INCAPACITATED for the stun, then the ragdoll is released, the
// body lies PRONE while it settles, and the vanilla wake-up stands it up (ALIVE, no ragdoll).
// Client 1 shoots a frame sequence of client 2 from the side: hit, limp, still down, getting up, up.
// The hit is the harness `shoot` verb (see taser-proof.mjs): it enters where a dart's damage does.
const STUN_SECONDS = 8; // LF_StunWeaponComponent.m_fStunSeconds on Handgun_Taser.et

export function parsePeerBody(line, id) {
  const m = new RegExp(`player=${id} downed=(\\d) life=(\\w+)[^|]*?ragdoll=(\\d) stance=(\\w+)`).exec(String(line ?? ""));
  if (!m) return null;
  return { downed: m[1], life: m[2], ragdoll: m[3], stance: m[4] };
}

async function runTaserRagdollProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const want = (label, judge, budgetMs) => {
    const own = clientSees(on, wait, 2, (s) => judge(s, "client 2 (owner)"), { verb: "state", parse: (l) => parseState(l), label, budgetMs });
    const peer = clientSees(on, wait, 1, (s) => judge(s, "client 1 (peer)"), { verb: "peers", parse: (l) => parsePeerBody(l, 2), label, budgetMs });
    console.log(`${label}: owner ${own.seconds.toFixed(1)} s, peer ${peer.seconds.toFixed(1)} s`);
    fails.push(...own.fails, ...peer.fails);
  };
  const frames = [];
  // Frames come from client 1's world camera at fixed offsets from client 2's body.
  const shoot = (name, at) => {
    if (!at) return;
    // From the open side, higher than eye level so a body on the floor is in frame (the police desk
    // has a wall on its +x side; the first offset tried shot the wall).
    on(1, { verb: "capture", mode: "world", name, x: at[0], y: at[1] + 0.3, z: at[2], ox: -3, oy: 1.8, oz: -2.5, hour: 12 }, 25000);
    frames.push(name);
  };

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);

  on(1, { verb: "stand", entityName: TASER });
  wait(2000);
  const equip = on(1, { verb: "use", className: "SCR_EquipWeaponAction", entityName: TASER }, 10000);
  if (equip.status !== "ok") fails.push(`equipping the taser returned ${equip.status} ${equip.detail}`);
  wait(4000);
  on(1, { verb: "input", action: "SwitchWeaponCategory2" });
  wait(3000);
  on(2, { verb: "stand", entityName: DESK });
  wait(3000);
  const at = String(parseState(on(2, { verb: "state" }, 10000).detail).pos ?? "").split(",").map(Number);
  const pos = at.length === 3 && at.every(Number.isFinite) ? at : null;
  if (!pos) fails.push("client 2 reports no position; no frames");
  shoot("taser_down_0_before", pos);

  const shot = on(1, { verb: "shoot", playerId: 2 }, 10000);
  const hitAt = Date.now();
  if (!/downed=1/.test(shot.detail)) fails.push(`the taser hit returned "${shot.detail}", expected downed=1`);
  want("ragdolled after the hit", (s, who) => (s.ragdoll === "1" && s.life === "INCAPACITATED" ? [] : [`${who} ragdoll=${s.ragdoll} life=${s.life}, expected ragdoll=1 INCAPACITATED`]), 4000);
  on(2, { verb: "capture", mode: "ui", name: "taser_down_1_victim_view" }, 25000);
  // Held down: half-way through the stun the body is still downed and unconscious. Read before
  // the world capture: a capture takes seconds.
  const half = (STUN_SECONDS * 1000) / 2 - (Date.now() - hitAt);
  if (half > 0) wait(half);
  const mid = parseState(on(2, { verb: "state" }, 10000).detail);
  const midAt = (Date.now() - hitAt) / 1000;
  if (mid.downed !== "1" || mid.life !== "INCAPACITATED") fails.push(`${midAt.toFixed(1)} s into an ${STUN_SECONDS} s stun the target is downed=${mid.downed} life=${mid.life}, expected still down`);
  shoot("taser_down_1_down", pos);

  // The stun ends at its time: poll the owner for the release.
  let released = null;
  while (Date.now() - hitAt < (STUN_SECONDS + 6) * 1000) {
    const s = parseState(on(2, { verb: "state" }, 10000).detail);
    if (s.downed === "0") {
      released = (Date.now() - hitAt) / 1000;
      break;
    }
    wait(400);
  }
  console.log(`stun released after ${released ?? "never"} s (configured ${STUN_SECONDS} s)`);
  if (released === null || released < STUN_SECONDS - 1 || released > STUN_SECONDS + 3) fails.push(`the stun released after ${released ?? "never"} s, expected about ${STUN_SECONDS} s`);

  // The body settles prone, then gets up through the vanilla wake-up.
  want("released and lying prone", (s, who) => (s.downed === "0" && s.ragdoll === "0" && s.stance === "PRONE" ? [] : [`${who} downed=${s.downed} ragdoll=${s.ragdoll} stance=${s.stance}, expected downed=0 ragdoll=0 PRONE`]), 3000);
  shoot("taser_down_2_released_prone", pos);
  want("up again", (s, who) => (s.life === "ALIVE" && s.ragdoll === "0" ? [] : [`${who} life=${s.life} ragdoll=${s.ragdoll}, expected ALIVE without ragdoll`]), 15000);
  wait(2000);
  shoot("taser_down_3_awake", pos);

  console.log("");
  console.log(`frames: ${frames.join(", ")}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "taser ragdoll proof: FAIL" : "taser ragdoll proof: PASS (hit ragdolls the target on owner and peer, held down for the stun, released prone, up through the vanilla wake-up)");
  return fails.length ? 1 : 0;
}

export default { name: "taser-ragdoll-proof", usage: "taser-ragdoll-proof", run: () => runTaserRagdollProof() };

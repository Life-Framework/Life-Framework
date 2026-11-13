import { arrive, clientBox, clientSees, clientSeesPeer, parseState, sessionDir } from "../session.mjs";

// #341 taser on a running `up --clients 2 --profile full` session: client 1 takes the taser off the
// police desk and draws it, the held weapon and the inventory are shot, client 1 hits client 2,
// client 2 is stunned (downed flag, INCAPACITATED, seen by both clients), cannot give up, and gets
// up by itself when the stun runs out. Getting up is what tells a stun from a lethal downing: a
// downed player bleeds out and dies.
//
// The hit is the harness `shoot` verb: a KINETIC HandleDamage of 1000 on the head with client 1 as
// instigator. It enters where a dart's damage does (SCR_CharacterDamageManagerComponent
// .HijackDamageHandling -> LF_Stun.TryHit), which finds the taser in client 1's hands. The harness
// cannot aim or pull a trigger (`input` takes menu actions only), so a fired dart reaching the
// target stays a manual step.
const TASER = "Roadmap_Police_Taser";
const DESK = "Roadmap_Police_Desk";
const PREVIEWS = ["{6D583A40C5C5C150}Prefabs/Weapons/Handguns/Taser/Handgun_Taser.et", "{22DC576156907CBF}Prefabs/Weapons/Handguns/Taser/Magazine_Taser.et", "{1353C6EAD1DCFE43}Prefabs/Weapons/Handguns/M9/Handgun_M9.et"];
const STUN_SECONDS = 8; // LF_StunWeaponComponent.m_fStunSeconds on Handgun_Taser.et

async function runTaserProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const state = (n) => on(n, { verb: "state" }, 10000).detail;
  const fails = [];
  const sees = (want, label, budgetMs = 15000) => {
    const own = clientSees(on, wait, 2, (s) => (s.downed === want.downed && want.life.includes(s.life) ? [] : [`downed=${s.downed} life=${s.life}, expected downed=${want.downed} life=${want.life}`]), { label, budgetMs });
    console.log(`client sees: ${label} ${own.line}`);
    const peer = clientSeesPeer(on, wait, 1, 2, (p) => (p.downed === want.downed && want.life.includes(p.life) ? [] : [`player 2 downed=${p.downed} life=${p.life}, expected downed=${want.downed} life=${want.life}`]), { label, budgetMs });
    return [...own.fails, ...peer.fails];
  };

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);

  // Take the taser off the desk and draw it (it goes to the handgun slot).
  on(1, { verb: "stand", entityName: TASER });
  wait(2000);
  on(1, { verb: "capture", mode: "ui", name: "taser_1_on_desk" }, 25000);
  const equip = on(1, { verb: "use", className: "SCR_EquipWeaponAction", entityName: TASER }, 10000);
  if (equip.status !== "ok") fails.push(`equipping the taser returned ${equip.status} ${equip.detail}`);
  wait(4000);
  on(1, { verb: "input", action: "SwitchWeaponCategory2" });
  wait(4000);

  // The inventory pictures the engine renders from the prefabs' PreviewRenderAttributes.
  on(1, { verb: "previews", mode: "show", prefabs: PREVIEWS }, 10000);
  wait(2500);
  on(1, { verb: "capture", mode: "ui", name: "taser_3_previews" }, 25000);
  on(1, { verb: "previews", mode: "hide" }, 10000);
  wait(1500);

  // Client 2 comes to the desk: inside the taser's reach.
  on(2, { verb: "stand", entityName: DESK });
  wait(3000);
  const posOf = (n) => String(parseState(state(n)).pos ?? "").split(",").map(Number);
  const [hx, hy, hz] = posOf(1);
  const shot = on(1, { verb: "shoot", playerId: 2 }, 10000);
  if (!/downed=1/.test(shot.detail) || !/alive=1/.test(shot.detail)) fails.push(`the hit with the taser in hand returned "${shot.detail}", expected alive=1 downed=1`);
  const stunnedAt = Date.now();
  fails.push(...sees({ downed: "1", life: ["INCAPACITATED"] }, "after the taser hit"));
  // A stunned player cannot give up. Checked first: the stun is short and a capture takes seconds.
  // (That a stun costs no health is LF_Test_Stun's; the state line carries no health.)
  const giveUp = on(2, { verb: "giveup" }, 10000);
  wait(1000);
  const during = parseState(state(2));
  if (during.downed !== "1" || during.life === "DEAD") fails.push(`after a give-up try the stunned player is downed=${during.downed} life=${during.life}; give-up must be refused (${giveUp.detail})`);
  on(2, { verb: "capture", mode: "ui", name: "taser_6_stunned_view" }, 25000);

  // They get up by themselves.
  const left = STUN_SECONDS * 1000 - (Date.now() - stunnedAt);
  if (left > 0) wait(left + 1500);
  fails.push(...sees({ downed: "0", life: ["ALIVE"] }, "after the stun ran out", 20000));
  on(2, { verb: "capture", mode: "ui", name: "taser_7_back_up" }, 25000);

  // Last, the taser in client 1's hand, shot by client 2 with the free camera 0.8 m off and
  // looking down at the hand. A world capture without `free` looks level from eye height and
  // misses a lowered hand. It is the last step and there is one of them on purpose: the free
  // camera moves the capturing player's body away, and a second free capture crashed the client
  // (2026-11-13, ENGINE (F) Crashed with no script error).
  if (Number.isFinite(hx)) on(2, { verb: "capture", mode: "world", name: "taser_2_held", x: hx, y: hy + 0.95, z: hz, ox: 0.8, oy: -0.95, oz: 0, hour: 12, free: true }, 25000);
  else fails.push("client 1 reports no position; the held weapon was not shot");

  console.log("");
  console.log(`stunned: ${shot.detail}; state while stunned: ${JSON.stringify(during)}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "taser proof: FAIL" : "taser proof: PASS (taser taken from the desk, target stunned and seen stunned by both clients, give-up refused, up again by itself)");
  return fails.length ? 1 : 0;
}

export default { name: "taser-proof", usage: "taser-proof", run: () => runTaserProof() };

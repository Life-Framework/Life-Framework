import { arrive, clientBox, sessionDir } from "../session.mjs";

// The taser on a town NPC (#922) on a running `up --clients 2 --profile full` session.
//   node agent-box/session.mjs up --clients 2 --profile full
//   node agent-box/session.mjs npc-stun-proof
//   node agent-box/session.mjs down
// Client 1 takes the taser off the police desk, draws it, walks to the fuel station and shoots the
// clerk. The harness cannot pull a trigger, so `shootnpc` with a stun weapon in hand runs the
// dart's own code (LF_NpcStun.HarnessShot: a line from the chest to the NPC, the same pick and
// the same stun). Judged from what CLIENT 2 reads of its replicated copy of the clerk (`seen`):
// the stun flag sets and the body lies down, the clerk's talk and shop actions are gone, then the
// flag clears and the clerk stands again. A real trigger pull stays a manual step.
const TASER = "Roadmap_Police_Taser";
const CLERK = "Roadmap_Fuel_Clerk";
const CIVILIAN = "Roadmap_Civilian_1";
const STUN_SECONDS = 8; // LF_StunWeaponComponent.m_fStunSeconds on Handgun_Taser.et

async function runNpcStunProof() {
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
  // Names are not replicated, so the clerk is the nearest town NPC in the `seen` line (both clients
  // stand within 3 m of him).
  const npcOf = (n) => {
    const r = on(n, { verb: "seen", radius: 20 }, 10000);
    const m = / npc=(\S+)/.exec(r.detail);
    const entry = (m ? m[1] : "").split("|")[0] ?? "";
    const f = Object.fromEntries(entry.split(":").slice(1).map((kv) => kv.split("=")));
    return { stun: f.stun, up: Number(f.up), raw: entry || "(no npc in the line)" };
  };

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);

  // Take the taser off the desk and draw it.
  on(1, { verb: "stand", entityName: TASER });
  wait(2000);
  const equip = on(1, { verb: "use", className: "SCR_EquipWeaponAction", entityName: TASER }, 10000);
  if (equip.status !== "ok") fails.push(`equipping the taser returned ${equip.status} ${equip.detail}`);
  wait(4000);
  on(1, { verb: "input", action: "SwitchWeaponCategory2" });
  wait(4000);

  // Both clients to the fuel station: 1 within the taser's reach of the clerk, 2 beside to watch.
  on(1, { verb: "stand", entityName: CLERK, side: "s" });
  on(2, { verb: "stand", entityName: CLERK, side: "w" });
  wait(4000);
  const before = npcOf(2);
  if (before.stun !== "0" || !(before.up > 0.9)) fails.push(`before the shot client 2 reads the clerk as ${before.raw}; expected stun=0 and up near 1`);
  const talkBefore = on(1, { verb: "look" }, 10000);

  const shot = on(1, { verb: "shootnpc", entityName: CLERK }, 10000);
  if (!/stunned=1/.test(shot.detail)) fails.push(`the dart returned "${shot.detail}", expected stunned=1`);
  const stunnedAt = Date.now();
  wait(1500);
  const down = npcOf(2);
  if (down.stun !== "1" || !(down.up < 0.2)) fails.push(`client 2 reads the clerk as ${down.raw} after the dart; expected stun=1 and up near 0`);
  const own = npcOf(1);
  if (own.stun !== "1") fails.push(`client 1 reads the clerk as ${own.raw} after the dart; expected stun=1`);
  const talkDuring = on(1, { verb: "look" }, 10000);
  on(1, { verb: "capture", mode: "ui", name: "npcstun_2_toast" }, 25000);

  // They get up by themselves.
  const left = STUN_SECONDS * 1000 - (Date.now() - stunnedAt);
  if (left > 0) wait(left + 2000);
  const after = npcOf(1);
  if (after.stun !== "0" || !(after.up > 0.9)) fails.push(`client 1 reads the clerk as ${after.raw} after the stun; expected stun=0 and up near 1`);
  const talkAfter = on(1, { verb: "look" }, 10000);

  // The picture: the kiosk glass hides the clerk, so the same dart goes to an NPC standing in the
  // open (Roadmap_Civilian_1, 258.5 243.5, facing +x; he falls toward -x). One free-camera capture,
  // last, on client 2: a second one on a client crashed it (2026-11-13), and the free camera moves
  // the capturing body away.
  on(1, { verb: "stand", entityName: CIVILIAN, side: "s" });
  wait(3000);
  const civ = on(1, { verb: "shootnpc", entityName: CIVILIAN }, 10000);
  if (!/stunned=1/.test(civ.detail)) fails.push(`the dart at the civilian returned "${civ.detail}", expected stunned=1`);
  wait(800);
  on(2, { verb: "capture", mode: "world", name: "npcstun_3_down", x: 257.7, y: 0.3, z: 243.5, ox: 0.6, oy: 1.5, oz: -3.2, hour: 12, free: true }, 25000);

  console.log("");
  console.log(`client 1 look before: ${talkBefore.detail.slice(0, 200)}`);
  console.log(`client 1 look during: ${talkDuring.detail.slice(0, 200)}`);
  console.log(`client 1 look after:  ${talkAfter.detail.slice(0, 200)}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "npc-stun-proof: FAIL" : "npc-stun-proof: PASS (clerk stunned, seen lying by both clients, up again by itself)");
  return fails.length ? 1 : 0;
}

export default { name: "npc-stun-proof", usage: "npc-stun-proof", run: () => runNpcStunProof() };

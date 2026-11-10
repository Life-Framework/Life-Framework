import { readFileSync } from "node:fs";
import { join } from "node:path";
import { ROB_TARGET, ROOT, arrive, clientBox, sessionDir } from "../session.mjs";

// M1-27: client 1 shoots client 2 at the fuel station (murder, wanted), then on the town square
// (safe zone, refused). `shoot` is one lethal hit through the vanilla HandleDamage path.
async function runMurderProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  // A lethal hit downs a player (M2 #255); the murder hook fires on the real death. This proof
  // shoots to kill, so it runs with the downed state off.
  const medicConf = readFileSync(join(ROOT, "addons", "LifeFramework", "Configs", "Medic", "LF_Medic.conf"), "utf8");
  if (!/m_bDownedEnabled\s+0/.test(medicConf)) {
    console.log("murder proof: lethal hits down a player now; set m_bDownedEnabled to 0 in LF_Medic.conf for the run");
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

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  const start1 = state(1);
  const start2 = state(2);

  // Outside the zone: the fuel station.
  on(2, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  on(1, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  const outside = on(1, { verb: "shoot", playerId: 2 }, 10000);
  wait(600);
  on(1, { verb: "capture", mode: "ui", name: "m1_27_murder_client1" }, 25000);
  const murder1 = state(1);
  if (!outside.detail.includes("refused=0 alive=0")) fails.push(`shot outside: ${outside.status} ${outside.detail}`);
  if (!murder1.includes("wanted=3")) fails.push(`client 1 after the kill: ${murder1}`);

  // Client 2 wakes in hospital.
  wait(3000);
  on(2, { verb: "click", widget: "RespawnButton" });
  wait(12000);
  const respawned2 = state(2);

  // The wanted board now carries a murder poster: a description, no name.
  on(2, { verb: "stand", entityName: "Roadmap_Police_WantedBoard" });
  wait(2500);
  const board = on(2, { verb: "use", className: "LF_WantedBoardReadAction", entityName: "Roadmap_Police_WantedBoard" }, 10000);
  wait(2500);
  on(2, { verb: "capture", mode: "ui", name: "m1_27_board_client2" }, 25000);
  on(2, { verb: "click", widget: "Close" });
  if (board.status !== "ok") fails.push(`board read: ${board.status} ${board.detail}`);

  // Inside the zone: the town square, 3.5 m apart.
  on(2, { verb: "stand", entityName: "Sign_SafeZone_Spawn" });
  wait(2500);
  on(1, { verb: "stand", entityName: "Sign_TownMap" });
  wait(2500);
  const inside = on(1, { verb: "shoot", playerId: 2 }, 10000);
  wait(600);
  on(1, { verb: "capture", mode: "ui", name: "m1_27_safezone_client1" }, 25000);
  const square1 = state(1);
  const square2 = state(2);
  if (!inside.detail.includes("refused=1 alive=1")) fails.push(`shot inside: ${inside.status} ${inside.detail}`);
  if (!square1.includes("wanted=3")) fails.push(`client 1 after the refused shot: ${square1}`);

  // The notice, as a stranger walking up to it reads it.
  on(1, { verb: "look" });
  const read = on(1, { verb: "use", className: "LF_SignReadAction", entityName: "Sign_SafeZone_Spawn" }, 10000);
  wait(2500);
  on(1, { verb: "capture", mode: "ui", name: "m1_27_sign_card_client1" }, 25000);
  on(1, { verb: "click", widget: "Close" });
  if (read.status !== "ok") fails.push(`sign read: ${read.status} ${read.detail}`);
  on(1, { verb: "capture", mode: "world", name: "m1_27_sign_spawn_west", x: 130.5, y: 1.2, z: 250, ox: -4, oy: 0.6, oz: 0 }, 25000);
  on(1, { verb: "capture", mode: "world", name: "m1_27_sign_east_out", x: 145.5, y: 1.2, z: 258, ox: 5, oy: 0.6, oz: 0 }, 25000);
  on(1, { verb: "capture", mode: "world", name: "m1_27_sign_east_in", x: 145.5, y: 1.2, z: 258, ox: -4, oy: 0.6, oz: 0 }, 25000);

  console.log("");
  console.log(`start     client 1: ${start1}`);
  console.log(`start     client 2: ${start2}`);
  console.log(`shot out  client 1: ${outside.detail}`);
  console.log(`murder    client 1: ${murder1}`);
  console.log(`respawn   client 2: ${respawned2}`);
  console.log(`shot in   client 1: ${inside.detail}`);
  console.log(`square    client 1: ${square1}`);
  console.log(`square    client 2: ${square2}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "murder proof: FAIL" : "murder proof: PASS (kill outside -> wanted 3; shot on the square refused)");
  return fails.length ? 1 : 0;
}

export default {
  name: "murder-proof",
  usage: "murder-proof",
  film: true,
  run: (ctx) => runMurderProof(),
};

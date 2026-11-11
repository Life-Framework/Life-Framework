import { arrive, clientBox, sessionDir } from "../session.mjs";

// #596 / #598 on a running `up --profile full --mission playtest` session (one client): the mining loop a player sees.
// Client 1 equips the pickaxe from the miner stand, mines IronOre_Debug (three swings a piece, five pieces: the rock
// shrinks a step per piece, drops four nuggets each time, then vanishes), is handed four iron nuggets (the harness
// `nuggets` verb stands in for picking them up) and smelts them at the boiler. A shot is taken at each step.
// `--regrow` waits out the 5 minute cooldown and shoots the rock grown back. Confirm in the server log:
// `[LFDebug:Mining] iron-debug rock hit`, `rock spent`, `rock grew back`.
const ROCK = { x: 181.72, y: 0.3, z: 171.86 };
const FURNACE = { x: 169.78, y: 0.9, z: 171.5 };
const SWINGS_PER_PIECE = 3;
const PIECES = 5;
const REGROW_MS = 306000;

async function runMiningProof(args) {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (spec, timeout) => {
    h.setBoxes([clientBox(dir, 1)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client 1: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  // The capture shows the miner's own view, so level the aim and holster the pickaxe for it, then take both back.
  let pitch = 0;
  const shoot = (spec) => {
    if (pitch !== 0) on({ verb: "input", action: "CharacterTurnUp", value: String(-pitch) });
    on({ verb: "input", action: "SwitchWeaponCategory2" });
    wait(1800);
    on(spec, 30000);
    on({ verb: "input", action: "SwitchWeaponCategory2" });
    wait(1800);
    if (pitch !== 0) on({ verb: "input", action: "CharacterTurnUp", value: String(pitch) });
  };
  const rockShot = (name) => shoot({ verb: "capture", mode: "world", name: `mine_${name}`, ...ROCK, ox: 3, oy: 0.8, oz: -3, hour: 17.9 });

  arrive((n, spec, timeout) => on(spec, timeout), 1, wait);
  wait(8000);
  rockShot("0_whole");

  // Hold the pickaxe, face the rock, look down at it.
  on({ verb: "stand", entityName: "Pickaxe_Debug" });
  wait(1500);
  const equip = on({ verb: "use", className: "SCR_EquipWeaponAction", entityName: "Pickaxe_Debug" }, 10000);
  if (equip.status !== "ok") fails.push(`equip pickaxe returned ${equip.status} ${equip.detail}`);
  wait(2500);
  on({ verb: "input", action: "SwitchWeaponCategory2" });
  wait(2500);
  on({ verb: "stand", entityName: "IronOre_Debug" });
  wait(2000);
  on({ verb: "input", action: "CharacterTurnUp", value: "-30" });
  pitch = -30;
  wait(1000);

  for (let piece = 1; piece <= PIECES; piece++) {
    // The rock sinks as it shrinks: tip the aim down a step so the swing still meets it.
    // Step back up to the rock too: the nuggets and the hit shake can nudge the miner.
    if (piece > 1) {
      on({ verb: "stand", entityName: "IronOre_Debug" });
      wait(1500);
      on({ verb: "input", action: "CharacterTurnUp", value: "-10" });
      pitch -= 10;
      wait(800);
    }
    for (let swing = 0; swing < SWINGS_PER_PIECE; swing++) {
      const hit = on({ verb: "input", action: "CharacterMelee" });
      if (hit.status !== "ok") fails.push(`swing ${piece}.${swing + 1} returned ${hit.status} ${hit.detail}`);
      wait(3200);
    }
    wait(800);
    rockShot(piece < PIECES ? `${piece}_piece` : `${piece}_spent`);
  }

  // Smelt at the boiler.
  on({ verb: "nuggets" }, 10000);
  wait(1500);
  on({ verb: "stand", entityName: "Furnace" });
  wait(2000);
  const look = on({ verb: "look" });
  if (!look.detail.includes("LF_ProcessAction@Furnace")) fails.push(`the furnace action is not in reach: ${look.detail}`);
  const smelt = on({ verb: "use", className: "LF_ProcessAction", entityName: "Furnace" }, 10000);
  if (smelt.status !== "ok") fails.push(`smelt returned ${smelt.status} ${smelt.detail}`);
  wait(3500);
  shoot({ verb: "capture", mode: "world", name: "mine_7_smelt", ...FURNACE, ox: 1.6, oy: 0.4, oz: -3.2, hour: 17.9 });

  if (args.includes("--regrow")) {
    wait(REGROW_MS);
    rockShot("8_regrown");
  }

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "mining proof: FAIL" : "mining proof: PASS (rock mined piece by piece and spent, nuggets smelted; confirm the Mining lines in the server log)");
  return fails.length ? 1 : 0;
}

export default {
  name: "mining-proof",
  usage: "mining-proof [--regrow]",
  run: (ctx) => runMiningProof(ctx.args),
};

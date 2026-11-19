import { arrive, clientBox, sessionDir } from "../session.mjs";

// Eat, drink and throw, on a running `up --clients 2 --profile full` session (maintainer report
// 2026-11-19: "no eating or drinking animation/action"). Client 1 is the player; client 2's free
// camera shoots client 1 from the front. Each step runs the code a player's input reaches:
//   eat    a baguette in the hands, the use input; hunger before and after
//   drink  a soda from the inventory, the inventory Use (it takes the bottle in hand first)
//   throw  a tin in the hands, the throw key; where it lands (the server's view)
// Output: the verb lines, the hunger and thirst before and after, and PNGs in the session's shots.
const BAGUETTE = "{787AA46A1D92C7F7}Prefabs/Items/Food/LF_Baguette.et";
const SODA = "{EE04C7556D2C9F49}Prefabs/Items/Store1989/LF_SodaBottle.et";
const CAN = "{A0A5D34D6F8232B4}Prefabs/Items/Store1989/LF_CannedMeat.et";

async function runFoodProof() {
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
  const must = (r, what) => {
    if (r.status !== "ok") fails.push(`${what}: ${r.status} ${r.detail}`);
    return r;
  };
  const food = (verb, arg = "") => on(1, { verb: "food", words: [verb, arg].filter(Boolean) }, 12000);
  const num = (text, key) => Number(String(text).match(new RegExp(`${key}=(-?[\\d.]+)`))?.[1] ?? NaN);
  const feet = () => String(on(1, { verb: "state" }, 10000).detail.match(/pos=(\S+)/)?.[1] ?? "").split(",").map(Number);
  const shoot = (name, at, off = { ox: 0.9, oy: -0.6, oz: 0.0 }, up = 1.1) => {
    if (at.length === 3 && at.every(Number.isFinite)) {
      on(2, { verb: "capture", mode: "world", name, x: at[0], y: at[1] + up, z: at[2], ...off, hour: 12, free: true }, 25000);
    } else fails.push(`no position for ${name}`);
  };

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  must(on(1, { verb: "stand", entityName: "Fit_saint_philippe_residents_anchor_bench", side: "w30" }, 20000), "stand in the open street");
  wait(2500);
  const at = feet();

  // Eat: hungry, a baguette in the hands, the use input.
  must(food("low", "40"), "set survival low");
  const hungerBefore = num(food("state").detail, "hunger");
  must(on(1, { verb: "itemgive", prefab: BAGUETTE }, 10000), "give the baguette");
  wait(1500);
  must(on(1, { verb: "hold", item: "LF_Baguette" }, 10000), "hold the baguette");
  wait(3500);
  on(1, { verb: "capture", mode: "ui", name: "food_eat_held_own" }, 25000);
  must(food("use"), "start eating");
  // The use lasts about 5 s and a capture takes about 2 s: client 2's shot goes first, in the first second.
  wait(600);
  shoot("food_eat_mid_front", at);
  wait(5500);
  const afterEat = food("state").detail;
  const hungerAfter = num(afterEat, "hunger");
  console.log(`hunger ${hungerBefore} -> ${hungerAfter}`);
  if (!(hungerAfter > hungerBefore)) fails.push(`eating did not raise hunger (${hungerBefore} -> ${hungerAfter})`);
  if (!/held=none/.test(afterEat)) fails.push(`the baguette is still held after eating: ${afterEat}`);

  // Drink: the inventory Use on a soda that is only carried.
  const thirstBefore = num(food("state").detail, "thirst");
  must(on(1, { verb: "itemgive", prefab: SODA }, 10000), "give the soda");
  wait(1500);
  must(food("invuse"), "inventory Use on the soda");
  wait(1200);
  on(1, { verb: "capture", mode: "ui", name: "food_drink_mid_own" }, 25000);
  wait(5500);
  const thirstAfter = num(food("state").detail, "thirst");
  console.log(`thirst ${thirstBefore} -> ${thirstAfter}`);
  if (!(thirstAfter > thirstBefore)) fails.push(`drinking did not raise thirst (${thirstBefore} -> ${thirstAfter})`);

  // Throw: a tin in the hands, the throw key.
  must(on(1, { verb: "itemgive", prefab: CAN }, 10000), "give the tin");
  wait(1500);
  must(on(1, { verb: "hold", item: "LF_CannedMeat" }, 10000), "hold the tin");
  wait(3500);
  must(food("throw"), "throw the tin");
  // Client 1 looks along the throw: the tin is in the air or has just landed ahead of the player.
  wait(300);
  on(1, { verb: "capture", mode: "ui", name: "food_throw_own_early" }, 25000);
  wait(1500);
  on(1, { verb: "capture", mode: "ui", name: "food_throw_own_landed" }, 25000);
  wait(2000);
  const rolling = food("thrown").detail; // two reads a moment apart: it has stopped when they agree
  console.log(`tin at first read: ${rolling}`);
  wait(2500);
  const landed = must(food("thrown"), "where the tin landed");
  const after = food("state").detail;
  if (!/held=none/.test(after)) fails.push(`the tin is still in the hands after the throw: ${after}`);
  const landMatch = String(landed.detail).match(/<([-\d.e]+),\s*([-\d.e]+),\s*([-\d.e]+)>/);
  const lands = landMatch ? landMatch.slice(1).map(Number) : [];
  if (lands.length === 3 && lands.every(Number.isFinite)) {
    // What each client's own copy of the world holds where the server says the tin lies.
    on(1, { verb: "food", words: ["near", lands.join(",") + ",12"] }, 12000);
    on(2, { verb: "food", words: ["near", lands.join(",") + ",12"] }, 12000);
  }
  if (lands.length === 3 && lands.every(Number.isFinite)) shoot("food_throw_landed", lands, { ox: 0.3, oy: -1.15, oz: 0.25 }, 0);
  else fails.push(`no landing spot in: ${landed.detail}`);

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "food proof: FAIL" : "food proof: done (read the shots)");
  return fails.length ? 1 : 0;
}

export default { name: "food-proof", usage: "food-proof", run: () => runFoodProof() };

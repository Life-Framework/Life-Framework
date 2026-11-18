import { readFileSync } from "node:fs";
import { join } from "node:path";
import { arrive, clientBox, newestLog, parseState, sessionDir, sessionPaths } from "../session.mjs";

// Fishing at the grid pond (#2995), driven the way a player does it, in a `--mission grid` session:
//   node agent-box/session.mjs up --mission grid --profile full --wait
//   node agent-box/session.mjs fishing-proof
//   node agent-box/session.mjs down
// Teleports to the cell `slice-fi4`, buys a rod and a bait tin at the chandler, takes the rod in the hands,
// casts at the pond (the fire input's own path: first water ahead), waits for the server's bite (the float
// dips), strikes inside the window, and checks a trout in the pack, FISHER XP and the `LF_OnFishCaught`
// line in the log; then sells the trout at the fish counter. Pictures: `tmp/session-shots` via capture.
// Confirm `[LFDebug:Fishing]` lines in the server log.
const CELL = "grid:slice-fi4";
const CHANDLER = "Grid_slice_fi4_R0_2";
const COUNTER = "Grid_slice_fi4_R0_1";
const SHOP = "LF_ShopAction";
const POND = { x: 158.5, z: 89.2 };

async function runFishingProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail.slice(0, 260)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const expect = (cond, what) => { if (!cond) fails.push(what); };
  const uiShot = (name) => on(1, { verb: "capture", mode: "ui", name }, 25000);
  const worldShot = (name, o) => on(1, { verb: "capture", mode: "world", name, x: POND.x, y: 0.4, z: POND.z, ox: o[0], oy: o[1], oz: o[2] }, 25000);
  const text = () => on(1, { verb: "ui", mode: "text" }, 10000).detail;
  const fishing = (...words) => on(1, { verb: "fishing", words }, 10000);

  arrive(on, 1, wait);
  on(1, { verb: "input", action: "MenuBack" });
  wait(3000);
  on(1, { verb: "resetplayer", cash: 1000 }, 10000);
  wait(1500);
  on(1, { verb: "teleport", words: [CELL] }, 10000);
  wait(3000);

  const water = fishing("water", String(POND.x), String(POND.z));
  expect(/water=POND/.test(water.detail), `the pond is POND water to the server's query (got: ${water.detail})`);
  expect(/water=NONE/.test(fishing("water", "157", "84").detail), "the ground beside the pond is not water");
  worldShot("fishing_pond_before", [0, 1.6, -5]);

  // Chandler: the rod (row 0) and a bait tin (row 1), two of the tin.
  on(1, { verb: "stand", entityName: CHANDLER });
  wait(2000);
  expect(on(1, { verb: "use", className: SHOP, entityName: CHANDLER }, 10000).status === "ok", "the chandler's price list opens");
  wait(2000);
  const list = text();
  expect(/Item0Row_Action/.test(list), `the chandler lists rows (got: ${list.slice(0, 240)})`);
  uiShot("fishing_chandler_list");
  // The slip holds one line at a time: the rod, pay; then the tin, one more, pay.
  on(1, { verb: "click", widget: "Item0Row_Action" });
  wait(600);
  uiShot("fishing_chandler_slip");
  on(1, { verb: "click", widget: "PayButton" });
  wait(2500);
  uiShot("fishing_chandler_paid");
  on(1, { verb: "click", widget: "Item1Row_Action" });
  wait(600);
  on(1, { verb: "click", widget: "QtyMoreButton" });
  wait(400);
  on(1, { verb: "click", widget: "QtyMoreButton" });
  wait(400);
  on(1, { verb: "click", widget: "PayButton" });
  wait(2500);
  on(1, { verb: "click", widget: "CloseButton" });
  wait(1200);
  expect(on(1, { verb: "carries", words: ["FishingRod"] }, 10000).status === "ok", "the rod is in the pack");
  expect(on(1, { verb: "carries", words: ["BaitTin"] }, 10000).status === "ok", "a bait tin is in the pack");

  // To the pond's south shore, rod in the hands, and cast.
  // (A stand teleports the character and drops what the hands hold, so the rod is taken after it.)
  on(1, { verb: "stand", entityName: "Grid_slice_fi4_PondSpot", side: "s" }, 10000);
  wait(2500);
  expect(on(1, { verb: "hold", item: "FishingRod" }, 10000).status === "ok", "the rod goes into the hands");
  wait(2500);
  fishing("state");
  let landed = false;
  for (let attempt = 1; attempt <= 3 && !landed; attempt++) {
    const fire = fishing("fire");
    expect(fire.status === "ok", `the fire input casts (got ${fire.status} ${fire.detail})`);
    if (fire.status !== "ok") break;
    wait(1200);
    let st = fishing("state").detail;
    expect(/line=1/.test(st) && /float=rest/.test(st), `a cast puts the line out and the float on the water (got: ${st})`);
    if (attempt === 1) uiShot("fishing_float_rest");
    // The first bite is for the picture: the screen capture takes longer than the 2 s window, so that
    // strike is late on purpose (no fish, the bait is gone). Later casts arm `fishing auto`, which
    // strikes the moment the bite reaches the client (a harness round trip is slower than the window).
    if (attempt > 1) fishing("auto");
    // Wait for the server's bite (15 to 60 s).
    let bit = false;
    for (let i = 0; i < 140 && !bit; i++) {
      st = fishing("state").detail;
      if (/bite=1/.test(st) || /auto=fired/.test(st)) bit = true;
      else wait(300);
    }
    expect(bit, "the server's bite reached the client");
    if (!bit) break;
    if (attempt === 1) {
      uiShot("fishing_float_dip");
      expect(fishing("fire").status === "ok", "the late strike goes out");
    }
    wait(2000);
    landed = on(1, { verb: "carries", words: ["FishTrout"] }, 10000).status === "ok";
    if (attempt === 1) expect(!landed, "a strike after the window gives no fish");
    if (!landed) {
      console.log(`attempt ${attempt}: struck after the window, recasting`);
      wait(1500);
    }
  }
  expect(landed, "a trout is in the pack after the strike");
  uiShot("fishing_caught");
  const serverLog = () => {
    const f = newestLog(join(sessionPaths(dir).server, "logs"));
    return f ? readFileSync(f, "utf8") : "";
  };
  let log = serverLog();
  expect(/\+10 FISHER xp/.test(log), "the server booked FISHER xp for the catch");
  expect(/caught \S+ \(trout/.test(log), "the server logged the catch");
  const cashBefore = Number(parseState(on(1, { verb: "state" }, 10000).detail).cash);

  // Sell it at the fish counter: pick the small trout row, then "Sell these".
  on(1, { verb: "stand", entityName: COUNTER });
  wait(2000);
  expect(on(1, { verb: "use", className: SHOP, entityName: COUNTER }, 10000).status === "ok", "the fish counter's price list opens");
  wait(2000);
  // The caught size is the pond table's roll: small is row 1, medium row 2, large row 3.
  let row = 2;
  if (on(1, { verb: "carries", words: ["FishTroutSmall"] }, 10000).status === "ok") row = 1;
  else if (on(1, { verb: "carries", words: ["FishTroutLarge"] }, 10000).status === "ok") row = 3;
  on(1, { verb: "click", widget: `Item${row}Row_Action` });
  wait(600);
  uiShot("fishing_counter_sell_slip");
  on(1, { verb: "click", widget: "SellButton" });
  wait(2500);
  uiShot("fishing_counter_sold");
  on(1, { verb: "click", widget: "CloseButton" });
  wait(1200);
  const cashAfter = Number(parseState(on(1, { verb: "state" }, 10000).detail).cash);
  console.log(`cash ${cashBefore} -> ${cashAfter}`);
  expect(cashAfter > cashBefore, `the counter paid for the trout (cash ${cashBefore} -> ${cashAfter})`);
  expect(on(1, { verb: "carries", words: ["FishTrout"] }, 10000).status !== "ok", "the trout left the pack");

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "fishing-proof: FAIL" : "fishing-proof: PASS");
  return fails.length ? 1 : 0;
}

export default { name: "fishing-proof", usage: "fishing-proof   (run in a --mission grid session)", run: () => runFishingProof() };

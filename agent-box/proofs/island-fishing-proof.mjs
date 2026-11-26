import { openKit, selftestKit } from "./_island-kit.mjs";

// Fishing on real Everon (flows2-1127), the way a player does it: `up --mission island --profile full --hold 20`, one
// client. `fishing-proof` is the grid-pond version; this one runs where players fish.
//   1 buy a rod ($60) and a bait tin ($30) at the seed and feed till (the outlet that stocks them)
//   2 stand on the bank at each spot (`Fit_saint_philippe_pond_spot`, the Spring Pond marker moved to the bank in #4944,
//     and `Fit_fishermansbay_cove_spot`, Fisherman's Bay), rod in the hands, cast with the fire input, wait for the
//     server's bite, strike (`fishing auto` strikes the moment the bite reaches the client) and find a fish in the pack
//   3 sell the catch at the dock wholesale buyer
// Pictures are UI shots of the game screen (the float at rest, the fish in the pack, the sale).
const TILL = process.env.TILL ?? "Fit_saint_philippe_seed_feed_seed_feed_till";
const BUYER = process.env.FISH_BUYER ?? "Fit_saint_philippe_dock_wholesale_wholesale_buyer";
// A harness `stand` faces the marker, which is not the water (the marker sits on the bank). A world capture poses the
// character at an offset from its target and faces the target, so the pose is: on the bank, looking over the water.
// Both water points were found with `fishing water x z` (the water the server reads). Pond: water west of the marker
// (x 4686 and below at z 10507); cove: ocean to the north of the marker (z 10714 and up at x 4030).
const SPOTS = [
  { label: "pond", entity: "Fit_saint_philippe_pond_spot", hour: 10, target: [4676, 15, 10507], offset: [14, 0.5, 0] },
  { label: "cove", entity: "Fit_fishermansbay_cove_spot", hour: 17, target: [4030, 0.9, 10722], offset: [0, 0.5, -12] },
];
const SPECIES = ["Trout", "Perch", "Carp", "Bass", "Mackerel", "Tuna"];
const BITE_BUDGET_S = 100;
const CASTS = 4;

async function run(args) {
  if (args.includes("--selftest")) {
    const f = selftestKit();
    for (const x of f) console.log(`FAIL  ${x}`);
    console.log(f.length ? "island-fishing-proof selftest: FAIL" : "island-fishing-proof selftest: PASS");
    return f.length ? 1 : 0;
  }
  const k = await openKit("island-fishing-proof");
  const { on, wait, step, cash, carried, shot, serverLog } = k;
  on({ verb: "resetplayer", cash: 400 }, 15000);
  wait(1500);
  const fishing = (...words) => on({ verb: "fishing", words }, 10000);
  // `carries Fish` also matches the rod and the bait tin: count fish by species.
  const fishCount = () => SPECIES.reduce((n, sp) => n + carried(`LF_Fish${sp}`), 0);

  // 1. Rod and bait at the till.
  on({ verb: "stand", entityName: TILL });
  wait(2000);
  const open = on({ verb: "use", className: "LF_ShopAction", entityName: TILL }, 10000);
  wait(2000);
  step("1 the seed and feed price list opens", open.status === "ok", `${open.status} ${String(open.detail).slice(0, 80)}`);
  shot("island_fishing_till");
  for (let row = 0; row < 14 && (carried("FishingRod") < 1 || carried("BaitTin") < 1); row++) {
    on({ verb: "click", widget: `Item${row}Row_Action` });
    wait(350);
    const slip = on({ verb: "ui", mode: "text" }).detail;
    if (!/PayButton/.test(String(slip))) continue;
    // Only the rod, the bait tin and what the rows hold ahead of them: pay for the line the slip shows.
    on({ verb: "click", widget: "PayButton" });
    wait(1400);
  }
  on({ verb: "click", widget: "CloseButton" });
  wait(800);
  step("1b a rod and a bait tin are in the pack", carried("FishingRod") >= 1 && carried("BaitTin") >= 1, `rod ${carried("FishingRod")}, tin ${carried("BaitTin")}`);

  // 2. Cast at each spot.
  const pose = (spot) => {
    on({ verb: "capture", mode: "world", name: `island_fishing_${spot.label}_bank`, x: spot.target[0], y: spot.target[1], z: spot.target[2], ox: spot.offset[0], oy: spot.offset[1], oz: spot.offset[2], hour: spot.hour }, 30000);
    wait(2500);
  };
  let landedTotal = 0;
  // One spot a run (`--spot pond|cove`, default pond), each in a fresh session: a second pose in one run leaves the first
  // rod's gadget mode ON_GROUND (`[LFDebug:HandTool] rod left the hands: ON_GROUND`), `hold FishingRod` then takes that
  // stale rod and every cast is refused NO_ROD. A harness teleport artifact, not a player path (flows2-1127).
  const only = args.includes("--spot") ? args[args.indexOf("--spot") + 1] : "pond";
  for (const spot of SPOTS.filter((x) => x.label === only)) {
    on({ verb: "time", hour: spot.hour }, 15000);
    wait(1000);
    pose(spot);
    // A stand drops what the hands hold: the rod is taken after it.
    const held = on({ verb: "hold", item: "FishingRod" }, 10000);
    wait(2500);
    step(`2 ${spot.label}: the rod goes into the hands`, held.status === "ok", `${held.status} ${String(held.detail).slice(0, 80)}`);
    const before = fishCount();
    let landed = false;
    for (let cast = 1; cast <= CASTS && !landed; cast++) {
      const fire = fishing("fire");
      if (fire.status !== "ok") {
        step(`2b ${spot.label}: the fire input casts`, false, `${fire.status} ${fire.detail}`);
        break;
      }
      wait(1500);
      const st = String(fishing("state").detail);
      if (cast === 1) {
        shot(`island_fishing_${spot.label}_float`);
        step(`2b ${spot.label}: a cast puts the line out and the float on the water`, /line=1/.test(st) && /float=rest/.test(st), st);
      }
      if (!/line=1/.test(st)) {
        // The server refused the cast (no water ahead, out of range, no bait): the server log says which.
        const refusal = serverLog().split("\n").filter((l) => /\[LFDebug:Fishing\]/.test(l)).slice(-2).join(" | ");
        console.log(`cast ${cast} not out: ${refusal}`);
        wait(1500);
        continue;
      }
      fishing("auto");
      let fired = false;
      for (let s = 0; s < BITE_BUDGET_S && !fired; s += 1) {
        if (/auto=fired/.test(String(fishing("state").detail))) fired = true;
        else wait(1000);
      }
      wait(2200);
      landed = fishCount() > before;
      if (!landed) console.log(`cast ${cast} on ${spot.label}: bite ${fired ? "struck" : "never came"}, no fish`);
    }
    step(`2c ${spot.label}: a fish is in the pack after the strike`, landed, `${fishCount()} fish in the pack`);
    if (landed) {
      landedTotal += 1;
      shot(`island_fishing_${spot.label}_caught`);
    }
  }
  const log = serverLog();
  step("2d the server booked the catches", (log.match(/\[LFDebug:Fishing\][^\n]*caught/g) ?? []).length >= landedTotal && landedTotal > 0, `${(log.match(/\[LFDebug:Fishing\][^\n]*caught/g) ?? []).length} catch line(s)`);

  // 3. Sell at the dock.
  on({ verb: "time", hour: 10 }, 15000);
  on({ verb: "stand", entityName: BUYER });
  wait(2000);
  const cashBefore = cash();
  let presses = 0;
  for (let p = 0; p < 12 && fishCount() > 0; p++) {
    const before = cash();
    on({ verb: "use", className: "LF_TraderSellAction", entityName: BUYER });
    wait(1000);
    if (cash() - before <= 0) break;
    presses += 1;
    if (presses === 1) shot("island_fishing_sold");
  }
  const earned = cash() - cashBefore;
  step("3 the dock buyer pays for the fish", presses >= 1 && earned > 0, `${presses} sale(s), +$${earned}, ${fishCount()} fish left`);
  return k.finish();
}

export default {
  name: "island-fishing-proof",
  usage: "island-fishing-proof [--spot pond|cove (one a run, default pond)] [--selftest]   (on `up --mission island --profile full --hold 20`: rod and bait, cast at the Spring Pond or Fisherman's Bay, sell at the dock)",
  run: (ctx) => run(ctx?.args ?? []),
};

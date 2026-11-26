import { openKit, plotStage, selftestKit } from "./_island-kit.mjs";

// The farm loop on real Everon (flows2-1127), the way a player plays it: `up --mission island --profile full --hold 20`,
// one client. The Saint-Philippe farm (`Fit_saint_philippe_farm_*`: seed crate, three cabbage plots) is the farm a player
// finds first.
//   1 take three cabbage seed packets from the seed crate (LF_GatherAction)
//   2 plant plots 1 to 3 (LF_FarmPlotAction); an early harvest of a sprouting plot is refused
//   3 the plots grow sprouts, half grown, ripe on the server clock (one minute a stage in the prefab): the server log
//     names each `plot sp_farm_plot_N stage -> S`; a picture at each stage
//   4 harvest the three plots (three cabbages each) and sell them at the market stall trader (the general store and the
//     supermarket tills only sell: the farm board used to send players to the general store, flows2-1127)
//   `--sell-only` skips 1 to 3 for a character that already carries cabbages
// `farm-proof` is the DebugWorld version of this (its coordinates and entity names); this one runs on the island.
const CRATE = "Fit_saint_philippe_farm_farm_seed_crate";
const PLOTS = [1, 2, 3].map((n) => ({ n, entity: `Fit_saint_philippe_farm_farm_plot_${n}`, key: `sp_farm_plot_${n}` }));
const TRADER = process.env.TRADER ?? "Fit_saint_philippe_market_market_trader";
const RIPE_STAGE = 2;
const GROW_BUDGET_S = 240;

async function run(args) {
  if (args.includes("--selftest")) {
    const f = selftestKit();
    for (const x of f) console.log(`FAIL  ${x}`);
    console.log(f.length ? "island-farm-proof selftest: FAIL" : "island-farm-proof selftest: PASS");
    return f.length ? 1 : 0;
  }
  const k = await openKit("island-farm-proof");
  const { on, wait, step, cash, carried, shot, serverLog } = k;
  const sellOnly = args.includes("--sell-only");
  on({ verb: "time", hour: 11 }, 15000);
  if (!sellOnly) on({ verb: "resetplayer", cash: 20 }, 15000);
  wait(1500);
  const plotShot = (name) => on({ verb: "capture", mode: "world", name, x: 4427.6, y: 17.0, z: 10599.0, ox: 1.5, oy: 1.6, oz: -4, hour: 11 }, 30000);
  const plotUse = (p) => on({ verb: "use", className: "LF_FarmPlotAction", entityName: p.entity }, 10000);

  const sellCabbages = () => {
    const cashBefore = cash();
    const had = carried("Cabbage");
    on({ verb: "stand", entityName: TRADER });
    wait(2000);
    let sold = 0;
    for (let press = 0; press < 24; press++) {
      const before = carried("Cabbage");
      if (before < 1) break;
      const r = on({ verb: "use", className: "LF_TraderSellAction", entityName: TRADER });
      wait(900);
      const gone = before - carried("Cabbage");
      if (r.status !== "ok" || gone < 1) break;
      sold += gone;
      if (sold === gone) shot("island_farm_5_sold");
    }
    const earned = cash() - cashBefore;
    step("4c the market stall trader pays for the cabbages", sold >= 1 && earned > 0, `${sold} of ${had} sold, +$${earned},${carried("Cabbage")} left`);
  };
  if (sellOnly) {
    sellCabbages();
    return k.finish();
  }

  plotShot("island_farm_0_empty");
  // 1. Seeds.
  on({ verb: "stand", entityName: CRATE });
  wait(2000);
  on({ verb: "look" });
  for (let i = 0; i < 3; i++) {
    on({ verb: "use", className: "LF_GatherAction", entityName: CRATE }, 10000);
    wait(1500);
  }
  const seeds = carried("CabbageSeeds");
  step("1 three cabbage seed packets are taken from the crate", seeds >= 3, `${seeds} in the pack`);

  // 2. Plant.
  const planted = [];
  for (const p of PLOTS) {
    on({ verb: "stand", entityName: p.entity });
    wait(1800);
    on({ verb: "look" });
    const r = plotUse(p);
    wait(1500);
    if (r.status === "ok") planted.push(p.n);
  }
  step("2 all three plots are planted", planted.length === 3, `planted ${planted.join(",") || "none"}`);
  const plantedLog = serverLog();
  step("2b the server logged each planting", PLOTS.every((p) => new RegExp(`plot ${p.key} planted`).test(plantedLog)), "");
  plotShot("island_farm_1_sprouts");
  on({ verb: "stand", entityName: PLOTS[0].entity });
  wait(1500);
  on({ verb: "look" });
  const early = plotUse(PLOTS[0]);
  wait(1200);
  step("2c a sprouting plot is not harvested early", !/ripe_harvest|harvested/i.test(String(early.detail)) && carried("Cabbage") === 0, `${early.status} ${String(early.detail).slice(0, 100)}`);

  // 3. Grow: wait for every plot's ripe line, a picture at the half-grown stage on the way.
  let halfShot = false;
  let ripe = false;
  for (let s = 0; s < GROW_BUDGET_S && !ripe; s += 5) {
    wait(5000);
    const log = serverLog();
    if (!halfShot && PLOTS.every((p) => plotStage(log, p.key) >= 1)) {
      plotShot("island_farm_2_half");
      halfShot = true;
    }
    ripe = PLOTS.every((p) => plotStage(log, p.key) >= RIPE_STAGE);
  }
  step("3 every plot reaches the ripe stage on the server clock", ripe, PLOTS.map((p) => `${p.key}=${plotStage(serverLog(), p.key)}`).join(" "));
  plotShot("island_farm_3_ripe");

  // 4. Harvest and sell.
  for (const p of PLOTS) {
    on({ verb: "stand", entityName: p.entity });
    wait(1800);
    on({ verb: "look" });
    plotUse(p);
    wait(1800);
  }
  const cabbages = carried("Cabbage");
  step("4 the harvest gives three cabbages a plot", cabbages >= 9, `${cabbages} cabbage(s) in the pack`);
  plotShot("island_farm_4_harvested");

  sellCabbages();
  return k.finish();
}

export default {
  name: "island-farm-proof",
  usage: "island-farm-proof [--sell-only] [--selftest]   (on `up --mission island --profile full --hold 20`: plant, grow, harvest and sell at the Saint-Philippe farm)",
  run: (ctx) => run(ctx?.args ?? []),
};

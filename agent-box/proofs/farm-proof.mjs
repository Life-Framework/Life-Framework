import { arrive, clientBox, sessionDir } from "../session.mjs";

// #614 on a running `up --profile full` session (one client): the farm loop a player sees. Client 1
// takes three cabbage seed packets from Farm_SeedCrate, plants FarmPlot_1..3, and the plots grow
// sprouts -> half grown -> ripe on the server clock (one minute a stage in DebugWorld). A wide and a
// detail shot is taken at each stage, then client 1 harvests plot 1 (three cabbages) and plot 1 is
// shot empty again. An early harvest and a second planting with no packet left must be refused. Confirm in the server
// log: `[LFDebug:Farming] plot farm_plot_1 planted`, `stage -> 1`, `stage -> 2`, `harvested`.
const STAGE_WAIT_MS = 62000;
const ROW = { x: 148.5, y: 0.3, z: 299.5 };
const PLOT = { x: 145, y: 0.3, z: 299.5 };

async function runFarmProof() {
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
  const shots = (stage) => {
    on({ verb: "capture", mode: "world", name: `farm_${stage}_wide`, ...ROW, ox: 2, oy: 0, oz: -9, hour: 17.9 }, 30000);
    on({ verb: "capture", mode: "world", name: `farm_${stage}_detail`, ...PLOT, ox: 1, oy: 0, oz: -2.6, hour: 17.9 }, 30000);
  };
  const plotUse = (n) => ({ verb: "use", className: "LF_FarmPlotAction", entityName: `FarmPlot_${n}` });

  arrive((n, spec, timeout) => on(spec, timeout), 1, wait);
  wait(8000);
  shots("0_empty");
  on({ verb: "capture", mode: "world", name: "farm_seedcrate_detail", x: 143.8, y: 0.3, z: 297.2, ox: 0.8, oy: 0, oz: -2.2, hour: 17.9 }, 30000);
  on({ verb: "capture", mode: "world", name: "farm_board_detail", x: 142.4, y: 1, z: 297, ox: 0.3, oy: 0, oz: -2.4, hour: 17.9 }, 30000);

  on({ verb: "stand", entityName: "Farm_SeedCrate" });
  wait(2000);
  on({ verb: "look" });
  for (let i = 0; i < 3; i++) {
    const take = on({ verb: "use", className: "LF_GatherAction", entityName: "Farm_SeedCrate" }, 10000);
    if (take.status !== "ok") fails.push(`seed take ${i + 1} returned ${take.status} ${take.detail}`);
    wait(1500);
  }

  for (const n of [1, 2, 3]) {
    on({ verb: "stand", entityName: `FarmPlot_${n}` });
    wait(2000);
    on({ verb: "look" });
    const plant = on(plotUse(n), 10000);
    if (plant.status !== "ok") fails.push(`plant plot ${n} returned ${plant.status} ${plant.detail}`);
    wait(1500);
  }
  wait(2000);
  shots("1_sprouts");
  on({ verb: "stand", entityName: "FarmPlot_1" });
  wait(1500);
  const early = on(plotUse(1), 10000);
  if (early.status === "ok") fails.push("the sprouting plot was harvested");

  wait(STAGE_WAIT_MS - 12000);
  shots("2_half");
  wait(STAGE_WAIT_MS - 6000);
  shots("3_ripe");

  on({ verb: "stand", entityName: "FarmPlot_1" });
  wait(2000);
  const harvest = on(plotUse(1), 10000);
  if (harvest.status !== "ok") fails.push(`harvest plot 1 returned ${harvest.status} ${harvest.detail}`);
  wait(2500);
  on({ verb: "state" }, 10000);
  const again = on(plotUse(1), 10000);
  if (again.status === "ok") fails.push("the harvested plot was planted again with no seed packet left");
  shots("4_harvested");

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "farm proof: FAIL" : "farm proof: PASS (seeds taken, three plots planted, grown to ripe, plot 1 harvested and empty; confirm the Farming lines in the server log)");
  return fails.length ? 1 : 0;
}

export default {
  name: "farm-proof",
  usage: "farm-proof",
  run: (ctx) => runFarmProof(),
};

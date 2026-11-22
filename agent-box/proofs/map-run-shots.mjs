import { arrive, clientBox, sessionDir } from "../session.mjs";

// Job runs as chains on the map, one client on the island (`up --mission island --profile full`): the
// client stands at the mine office and the orchard desk so both are places it knows, then opens the map
// and hovers the hardware store (mining run, step 2 of 5), the mine (step 1) and the market (apple run,
// last stop). Each hover draws that run's dashed connectors and numbered stops; the card names the step.
// The client log's `[LFDebug:MapRuns] hover shows N run(s) at <place>` lines back the pictures.
//   node agent-box/session.mjs map-run-shots [--name <prefix>]
// Runtime view centres and the places' own positions, for the pictures only.
const VIEWS = [
  { name: "1_hardware_mining", centre: [4450, 10600], zoom: 4, hover: [4535.4, 10629.6] },
  { name: "2_mine_mining", centre: [4450, 10600], zoom: 4, hover: [4330, 10404] },
  { name: "3_market_apples", centre: [4550, 10740], zoom: 4.6, hover: [4488.3, 10702.3] },
];
const VISIT = ["Fit_coalmans_brow_mine_mine_desk", "Fit_saint_philippe_orchard_orchard_desk"];

async function runMapRunShots() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (spec, timeout) => {
    h.setBoxes([clientBox(dir, 1)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client 1: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const at = process.argv.indexOf("--name");
  const prefix = at >= 0 && process.argv[at + 1] ? process.argv[at + 1] : "maprun";
  const map = (...words) => on({ verb: "map", words: words.map(String) }, 15000);
  const fails = [];

  arrive((n, spec, t) => on(spec, t), 1, wait);
  wait(6000);
  for (const name of VISIT) {
    on({ verb: "stand", entityName: name, side: "s" });
    wait(7000);
  }

  const opened = map("open");
  if (opened.status !== "ok") fails.push(`the map opens (got ${opened.detail})`);
  wait(2500);
  for (const view of VIEWS) {
    map("pan", view.centre[0], view.centre[1], view.zoom);
    wait(2000);
    const hover = map("hover", view.hover[0], view.hover[1]);
    if (hover.status !== "ok") fails.push(`${view.name}: the hover reaches a badge (got ${hover.detail})`);
    wait(1500);
    on({ verb: "capture", mode: "ui", name: `${prefix}_${view.name}` }, 30000);
  }
  map("close");

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "map run shots: FAIL" : "map run shots: PASS (read the shots; grep the client log for '[LFDebug:MapRuns]')");
  return fails.length ? 1 : 0;
}

export default {
  name: "map-run-shots",
  usage: "map-run-shots [--name <prefix>]   (up --mission island --profile full)",
  run: () => runMapRunShots(),
};

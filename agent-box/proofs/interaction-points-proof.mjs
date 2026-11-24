import { arrive, clientBox, sessionDir } from "../session.mjs";

// A desk, counter or bench row is offered when the player looks down at it (#4871: the booking desk's interaction point
// sat 4 mm inside the desk top, so a ray landing on the top offered nothing). One client on `up --mission island
// --profile full`: for each placed prop the client stands beside it (all four sides), looks down (`input CharacterPitch`,
// -45 by default) and reads `aim`. A prop passes when some side offers a row of its own fit-out (the entity `aim` selects
// starts with the prop's first four name parts: a counter's row can sit on its till or its clerk, and a row can be another
// action class).
//   node agent-box/session.mjs interaction-points-proof [--pitch -45] [--targets Entity,Entity] [--verbose]
// The default list is the island props whose point validate-interaction-points moved or flagged: the desks and counters
// of the Saint-Philippe fit-outs. `none` on every side is a FAIL with what `aim` said. Not in the list: a row that is
// offered only in a state (the booking desk needs a cuffed suspect: `booking-desk-proof`; the labour counter needs a job
// state), where `aim none` means "no state", not a hidden row.
const TARGETS = [
  "Fit_saint_philippe_gun_counter_lf_gun_counter",
  "Fit_saint_philippe_clinic_lf_hospital_till",
  "Fit_saint_philippe_clothing_counter_shop_01",
  "Fit_saint_philippe_pharmacy_pharmacy_counter",
  "Fit_saint_philippe_furniture_furniture_counter",
  "Fit_saint_philippe_supermarket_supermarket_till",
  "Fit_saint_philippe_bakery_bakery_till",
  "Fit_saint_philippe_market_market_stall",
  "Fit_saint_philippe_hardware_hardware_tool_station",
  "Fit_saint_philippe_districts_garage_livery_bay",
  "Fit_saint_philippe_jail_jail_kitchen_sink",
  "Fit_saint_philippe_police_police_kit_law_book_lectern",
  "Fit_saint_philippe_barber_barber_chair_1",
];

async function runProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const flag = (name, dflt) => { const i = process.argv.indexOf(name); return i > 0 ? process.argv[i + 1] : dflt; };
  const verbose = process.argv.includes("--verbose");
  const pitch = Number(flag("--pitch", "-45"));
  const targets = flag("--targets", "") ? flag("--targets", "").split(",") : TARGETS;
  const on = (spec, timeout) => {
    h.setBoxes([clientBox(dir, 1)]);
    const r = h.sendCommand(spec, timeout);
    if (verbose) console.log(`client 1: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${String(r.detail).slice(0, 260)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  arrive((n, spec, t) => on(spec, t), 1, wait);
  wait(3000);
  on({ verb: "click", widget: "WelcomeGuideDismissButton" });
  wait(1500);

  const fails = [];
  for (const name of targets) {
    const seen = [];
    let offeredAt = null;
    for (const side of ["e1", "w1", "n1", "s1"]) {
      const s = on({ verb: "stand", entityName: name, side }, 15000);
      if (s.status !== "ok") { seen.push(`${side}: stand ${String(s.detail).slice(0, 60)}`); continue; }
      wait(1200);
      on({ verb: "input", action: "CharacterPitch", value: pitch }, 8000);
      wait(700);
      const aim = String(on({ verb: "aim", words: [] }, 10000).detail);
      const own = aim.startsWith("aim selected=") && new RegExp(`selected=\\S*@${name.split("_").slice(0, 4).join("_")}`).test(aim);
      seen.push(`${side}: ${aim.replace(/ menu=.*$/, "").slice(0, 150)}`);
      if (own) { offeredAt = side; break; }
    }
    on({ verb: "input", action: "CharacterPitch", value: 0 }, 8000);
    console.log(`${offeredAt ? "PASS" : "FAIL"}  ${name}: ${offeredAt ? `offered at pitch ${pitch} from the ${offeredAt} side` : `nothing offered at pitch ${pitch} on any side`}`);
    if (!offeredAt) fails.push(`${name}: ${seen.join(" | ")}`);
  }
  for (const f of fails) console.log(`DETAIL ${f}`);
  console.log(fails.length ? `interaction points proof: FAIL (${fails.length} of ${targets.length})` : `interaction points proof: PASS (${targets.length} props)`);
  return fails.length ? 1 : 0;
}

export default {
  name: "interaction-points-proof",
  usage: "interaction-points-proof [--pitch -45] [--targets Entity,Entity] [--verbose]",
  run: () => runProof(),
};

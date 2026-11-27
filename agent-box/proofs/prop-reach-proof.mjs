import { arrive, clientBox, sessionDir } from "../session.mjs";

// The prompt a player gets standing normally beside a world prop (validate-warnings, 2026-11-27). The action-reach
// check models a prop's context as a sphere and warns that a radius "does not reach a player standing 0.5 m off the
// mesh". This proof is the answer from play: one client on `up --mission island --profile lowspec` stands 1 m from a
// live named instance of each prop kind on four sides (`stand`), and `aim` (the engine's own pick under the crosshair:
// context sphere plus aim ray) must list the prop's own action. A sign or an ATM has one usable face, so those
// need one side of four; the rest need two. The result table decides which radii to widen and which warnings
// are the check's model, not the game.
//   node agent-box/session.mjs prop-reach-proof [--verbose] [--kind <label>[,<label>]]
const KINDS = [
  { label: "LF_Sign_Plate", entity: "Fit_saint_philippe_arrival_lf_sign_rules", action: "LF_SignReadAction", need: 1 },
  { label: "LF_Sign_PlateSmall", entity: "ResidentPlate_saint_philippe_sp_b15", action: "LF_SignReadAction", need: 1 },
  { label: "LF_Sign_PolePlate:carlot_vehicle_lot_sign", entity: "Fit_saint_philippe_carlot_vehicle_lot_sign", action: "LF_SignReadAction", need: 1 },
  { label: "LF_Sign_PolePlate:orchard_orchard_sign", entity: "Fit_saint_philippe_orchard_orchard_sign", action: "LF_SignReadAction", need: 1 },
  { label: "LF_Sign_PolePlate:pond_sign", entity: "Fit_saint_philippe_pond_sign", action: "LF_SignReadAction", need: 1 },
  { label: "LF_Sign_Pub", entity: "Fit_saint_philippe_dock_wholesale_iron_board", action: "LF_SignReadAction", need: 1 },
  { label: "LF_ATM", entity: "Fit_saint_philippe_atms_drop_b79", action: "LF_ATM(Quick)?Action", need: 1 },
  { label: "LF_SeedCrate_Cabbage", entity: "Fit_saint_philippe_farm_farm_seed_crate", action: "LF_GatherAction", need: 2 },
  { label: "FarmPlot_Cabbage", entity: "Fit_saint_philippe_farm_farm_plot_1", action: "LF_FarmPlotAction", need: 2 },
];

async function runProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const verbose = process.argv.includes("--verbose") || !!process.env.LF_PROOF_NOTES;
  const on = (spec, timeout) => {
    h.setBoxes([clientBox(dir, 1)]);
    const r = h.sendCommand(spec, timeout);
    if (verbose) console.log(`client 1: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${String(r.detail).slice(0, 200)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  // A context is a sphere the aim ray must hit, so a small one on a sign hung above eye height or an ATM is found by
  // looking at it, as a player does. After `stand` faces the entity at level pitch, step the view up and down (the pitch
  // is absolute, `input CharacterPitch`) until the prop's own action is offered; the first pitch that offers it is the
  // proof, no pitch at all is a real miss.
  const PITCHES = [0, -10, -25, -40, 10, 25, 40];
  const probe = (kind, side) => {
    const re = new RegExp(`(offered|selected)=[^ ]*${kind.action}`);
    const s = on({ verb: "stand", entityName: kind.entity, side }, 15000);
    if (s.status !== "ok") return { ok: false, note: `stand ${s.status} ${String(s.detail).slice(0, 60)}`, standing: false };
    wait(1500);
    let aim = "";
    for (const pitch of PITCHES) {
      on({ verb: "input", action: "CharacterPitch", value: pitch }, 8000);
      wait(700);
      aim = String(on({ verb: "aim", words: [] }, 10000).detail);
      if (re.test(aim)) return { ok: true, note: `pitch ${pitch}: ${aim.slice(0, 140)}`, standing: true, pitch };
    }
    on({ verb: "input", action: "CharacterPitch", value: 0 }, 8000);
    return { ok: false, note: aim.slice(0, 160), standing: true };
  };
  const at = process.argv.indexOf("--kind");
  const only = at >= 0 && process.argv[at + 1] ? process.argv[at + 1].split(",") : null;
  const kinds = only ? KINDS.filter((k) => only.includes(k.label)) : KINDS;
  arrive((n, spec, t) => on(spec, t), 1, wait);
  wait(3000);
  on({ verb: "click", widget: "WelcomeGuideDismissButton" });
  const fails = [];
  for (const kind of kinds) {
    const shown = [];
    let stood = 0;
    for (const side of ["e", "w", "n", "s"]) {
      const r = probe(kind, `${side}1`);
      if (r.standing) stood++;
      if (r.ok) shown.push(`${side}@${r.pitch}`);
      else if (verbose) console.log(`  ${kind.label} ${side}1: ${r.note}`);
    }
    const pass = stood > 0 && shown.length >= kind.need;
    console.log(`${pass ? "PASS" : "FAIL"}  ${kind.label}: ${kind.action} at 1 m [${shown.join(" ")}] (needs ${kind.need} of 4)${stood === 0 ? " (never stood: not live?)" : ""}`);
    if (!pass) fails.push(`${kind.label}: ${shown.length} of 4 sides offer ${kind.action} at 1 m (needs ${kind.need})`);
  }
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? `prop-reach-proof: FAIL (${fails.length})` : `prop-reach-proof: PASS (${kinds.length} kinds)`);
  return fails.length ? 1 : 0;
}

export default {
  name: "prop-reach-proof",
  usage: "prop-reach-proof [--verbose] [--kind <label>[,<label>]]   (one client on `up --mission island`)",
  run: () => runProof(),
};

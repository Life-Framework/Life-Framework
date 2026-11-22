import { arrive, clientBox, sessionDir } from "../session.mjs";

// The "Talk to the clerk" prompt across a counter (#4078), one client on `up --mission island`: for each Saint-Philippe
// clerk the client stands about 2 m out on each side of the clerk and `look` (the list the interact key offers) must
// show LF_DialogueTalkAction on at least one side (the customer side), then stands 3.5 m out and the prompt must be
// gone (reach is 3 m). A clerk whose rows all fail is reported with what `look` offered.
//   node agent-box/session.mjs counter-talk-proof
// The layer-named clerk markers are not the NPCs (a resident spawns when due and a player is near), so the proof
// addresses residents by their LF_ResidentDef id (`Resident_<id>`).
const CLERKS = ["Resident_teller", "Resident_store_clerk", "Resident_cafe_owner", "Resident_old_man"];

async function runProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const quiet = process.argv.includes("--verbose") ? false : true;
  const on = (spec, timeout) => {
    h.setBoxes([clientBox(dir, 1)]);
    const r = h.sendCommand(spec, timeout);
    if (!quiet) console.log(`client 1: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${String(r.detail).slice(0, 200)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const talks = (name, side) => {
    const s = on({ verb: "stand", entityName: name, side }, 15000);
    if (s.status !== "ok") return { ok: false, note: `stand ${s.detail}` };
    wait(2000);
    const look = String(on({ verb: "look" }, 15000).detail);
    return { ok: look.includes("LF_DialogueTalkAction"), note: look.slice(0, 120) };
  };
  arrive((n, spec, t) => on(spec, t), 1, wait);
  wait(3000);
  on({ verb: "click", widget: "WelcomeGuideDismissButton" });
  const fails = [];
  for (const clerk of CLERKS) {
    const near = ["e2", "w2", "n2", "s2"].map((side) => [side, talks(clerk, side)]);
    const shown = near.filter(([, r]) => r.ok).map(([side]) => side);
    const far = ["e", "w", "n", "s"].map((side) => talks(clerk, `${side}4`));
    // A 4 m ring is the nearest whole-metre stand beyond reach; a prompt there means the reach grew.
    const farShown = far.filter((r) => r.ok).length;
    const okNear = shown.length > 0;
    console.log(`${okNear && farShown === 0 ? "PASS" : "FAIL"}  ${clerk}: prompt at 2 m on [${shown.join(" ")}], at 4 m on ${farShown} of 4 sides`);
    if (!okNear) fails.push(`${clerk}: no side offers Talk at 2 m (${near.map(([s, r]) => `${s}:${r.note}`).join(" | ").slice(0, 300)})`);
    if (farShown > 0) fails.push(`${clerk}: the prompt still shows at 4 m on ${farShown} side(s)`);
  }
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? `counter talk proof: FAIL (${fails.length})` : `counter talk proof: PASS (${CLERKS.length} clerks)`);
  return fails.length ? 1 : 0;
}

export default {
  name: "counter-talk-proof",
  usage: "counter-talk-proof [--verbose]",
  run: () => runProof(),
};

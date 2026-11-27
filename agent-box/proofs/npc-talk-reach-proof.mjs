import { arrive, clientBox, sessionDir } from "../session.mjs";

// The Talk prompt for a player who stands normally beside an NPC (validate-warnings, 2026-11-27). The action-reach check
// models a prop's context as a sphere a player must be inside, and so warns that a character's 0.6 m chest sphere
// "does not reach a player 0.5 m off the body". This proof is the answer from play: one client on `up --mission island`
// stands 1 m and 2 m from each resident on all four sides (the stand a player takes beside someone),
// and `aim` (what the crosshair offers the interact key) must list Talk or Quest. A counter clerk is
// customer-side only, so a clerk needs one side; everyone else needs at least three of four at 1 m.
//   node agent-box/session.mjs npc-talk-reach-proof [--verbose] [--resident <id>[,<id>]]
const OPEN = ["neighbour_mireille", "dock_fishmonger", "dock_chandler"];
const COUNTER = ["store_clerk", "teller"];
const PROMPT_RE = /(offered|selected)=[^ ]*LF_(DialogueTalk|Quest)Action/;

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
  // `aim` is the engine's own pick under the crosshair (context sphere plus aim ray), the list the interact key
  // offers. `look` is not: it lists every action whose own check passes and never tests the sphere.
  const prompt = (name, side) => {
    const s = on({ verb: "stand", entityName: `Resident_${name}`, side }, 15000);
    if (s.status !== "ok") return { ok: false, note: `stand ${s.status} ${String(s.detail).slice(0, 60)}`, standing: false };
    wait(1500);
    let aim = String(on({ verb: "aim", words: [] }, 10000).detail);
    for (let i = 0; i < 2 && !PROMPT_RE.test(aim); i++) {
      wait(1000);
      aim = String(on({ verb: "aim", words: [] }, 10000).detail);
    }
    return { ok: PROMPT_RE.test(aim), note: aim.slice(0, 160), standing: true };
  };
  const at = process.argv.indexOf("--resident");
  const only = at >= 0 && process.argv[at + 1] ? process.argv[at + 1].split(",") : null;
  arrive((n, spec, t) => on(spec, t), 1, wait);
  wait(3000);
  on({ verb: "click", widget: "WelcomeGuideDismissButton" });
  const fails = [];
  const rows = only ? only.map((id) => [id, COUNTER.includes(id)]) : [...OPEN.map((id) => [id, false]), ...COUNTER.map((id) => [id, true])];
  for (const [id, counter] of rows) {
    const line = [];
    let oneMetre = 0;
    let stood = 0;
    for (const dist of [1, 2]) {
      const shown = [];
      for (const side of ["e", "w", "n", "s"]) {
        const r = prompt(id, `${side}${dist}`);
        if (r.standing) stood++;
        if (r.ok) shown.push(side);
        else if (verbose || process.env.LF_PROOF_NOTES) console.log(`  ${id} ${side}${dist}: ${r.note}`);
        if (dist === 1 && r.ok) oneMetre++;
      }
      line.push(`${dist} m: [${shown.join(" ")}]`);
    }
    const need = counter ? 1 : 3;
    const pass = stood > 0 && oneMetre >= need;
    console.log(`${pass ? "PASS" : "FAIL"}  Resident_${id}: prompt at ${line.join(", ")}${stood === 0 ? " (never stood: not live?)" : ""}`);
    if (!pass) fails.push(`Resident_${id}: ${oneMetre} of 4 sides offer Talk at 1 m (needs ${need})`);
  }
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? `npc-talk-reach-proof: FAIL (${fails.length})` : `npc-talk-reach-proof: PASS (${rows.length} residents)`);
  return fails.length ? 1 : 0;
}

export default {
  name: "npc-talk-reach-proof",
  usage: "npc-talk-reach-proof [--verbose] [--resident <id>[,<id>]]   (one client on `up --mission island`)",
  run: () => runProof(),
};

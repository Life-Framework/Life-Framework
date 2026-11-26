import { arrive, clientBox, sessionDir } from "../session.mjs";

// Newly voiced NPC lines play (npc-voice-audit-1127). On a running island session
//   node agent-box/session.mjs up --mission island --profile full --sound --clients 1 --wait
//   node agent-box/session.mjs npc-voice-proof [--client 1]
// client 1 walks to NPCs whose answers were recorded in the audit and takes the path that makes each say one:
//   tally clerk   asks for the key and pays with $100 in the pocket: the fee is $400, so he says `fee_short`;
//   barkeeper     asks "Heard anything?" twice in one game day: the second time he says `rumor_asked_today`
//                 (or `rumor_nothing_new` when he has no rumor left).
//   shopkeeper    the supermarket keeper, same question twice: `rumor_asked_today` or `rumor_nothing_new` in his own voice.
// An agent cannot listen: the proof prints each scene's state, then read the client console.log for
// `[LFDebug:Dialogue] line '<npc>/<line>' plays: event ... s, mouth envelope` (a valid sound handle) for the new lines.
const SCENARIOS = [
  { id: "tally_clerk", entity: "Fit_saint_philippe_dock_containers_tally_clerk", talks: [[[0, 3000], [0, 4000]]] },
  { id: "barkeeper", entity: "Fit_chotain_pub_bar_barkeeper", talks: [[[1, 6000]], [[1, 6000]]] },
  { id: "shopkeeper", entity: "Fit_saint_philippe_supermarket_village_supermarket_shopkeeper", talks: [[[2, 6000]], [[2, 6000]]] },
];

async function runNpcVoiceProof() {
  const h = await import("../harness.mjs");
  const at = process.argv.indexOf("--client");
  const client = at >= 0 && process.argv[at + 1] ? Number(process.argv[at + 1]) : 1;
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, client)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${client}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  arrive(on, client, wait);
  wait(6000);
  const fails = [];
  for (const s of SCENARIOS) {
    for (const picks of s.talks) {
      on(client, { verb: "stand", entityName: s.entity, side: "w" });
      wait(2500);
      const use = on(client, { verb: "use", className: "LF_DialogueTalkAction", entityName: s.entity }, 10000);
      if (use.status !== "ok") { fails.push(`${s.id}: talk returned ${use.status} ${use.detail}`); continue; }
      for (const [index, hold] of picks) {
        // A pick only lands once the line has finished and the reply list shows: wait for it (at most 40 s).
        for (const until = Date.now() + 40000; Date.now() < until; wait(500)) {
          if (/phase=choosing/.test(h.sendCommand({ verb: "dialogue" }, 5000).detail || "")) break;
        }
        on(client, { verb: "dialogue", arg: "pick", value: String(index) }, 10000);
        wait(hold);
      }
      console.log(`${s.id}: scene ${on(client, { verb: "dialogue" }, 10000).detail}`);
      on(client, { verb: "dialogue", arg: "skip" }, 5000);
      wait(500);
      on(client, { verb: "input", action: "MenuBack" }, 5000);
      wait(1500);
    }
  }
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "npc-voice-proof: FAIL" : "npc-voice-proof: PASS (now grep the client console.log for the new lines' 'plays' entries)");
  return fails.length ? 1 : 0;
}

export default {
  name: "npc-voice-proof",
  usage: "npc-voice-proof [--client N]",
  run: () => runNpcVoiceProof(),
};

import { arrive, clientBox, sessionDir } from "../session.mjs";

// #4611: the first talk after a fresh boot works on the first try. One client on a fresh
// `up --mission island` session arrives, stands by the placed Chotain barkeeper and uses Talk once:
// the scene opens and reaches the replies. No retry: a miss is the bug. `--npc <EntityName>` talks to
// another placed dialogue NPC, `--side` picks the side to stand on (a counter clerk talks only from the front).
//   node agent-box/session.mjs first-talk-proof [--npc <EntityName>] [--side e|w|n|s]
const DEFAULT_NPC = "Fit_chotain_pub_bar_barkeeper";

async function run() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const at = process.argv.indexOf("--npc");
  const npc = at >= 0 && process.argv[at + 1] ? process.argv[at + 1] : DEFAULT_NPC;
  const sideAt = process.argv.indexOf("--side");
  const side = sideAt >= 0 && process.argv[sideAt + 1] ? process.argv[sideAt + 1] : "w";
  const on = (spec, timeout) => {
    h.setBoxes([clientBox(dir, 1)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client 1: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${String(r.detail).slice(0, 200)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const scene = () => String(on({ verb: "dialogue" }, 10000).detail);
  const fails = [];

  arrive((n, spec, t) => on(spec, t), 1, wait);
  const stood = on({ verb: "stand", entityName: npc, side }, 20000);
  if (stood.status !== "ok") {
    console.log(`FAIL  stand at ${npc}: ${stood.status} ${stood.detail}`);
    return 1;
  }

  // One talk, the same action a player's key runs.
  const use = on({ verb: "use", className: "LF_DialogueTalkAction", entityName: npc }, 10000);
  if (use.status !== "ok") fails.push(`the first Talk on ${npc} returned ${use.status} ${use.detail}`);
  let replies = "";
  for (let waited = 0; waited < 30000 && !/phase=choosing|^closed/.test(replies); waited += 1000) {
    wait(1000);
    replies = scene();
  }

  if (!/phase=choosing/.test(replies)) fails.push(`the first talk reaches the replies (got: ${replies})`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "first-talk-proof: FAIL" : `first-talk-proof: PASS (${npc} answered the first Talk: ${replies})`);
  return fails.length ? 1 : 0;
}

export default {
  name: "first-talk-proof",
  usage: "first-talk-proof [--npc <EntityName>] [--side e|w|n|s]   (run once on a fresh `up --mission island` session)",
  run,
};

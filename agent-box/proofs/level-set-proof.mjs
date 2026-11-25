import { copyFileSync, existsSync, mkdirSync, readdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { arrive, clientBox, sessionDir } from "../session.mjs";

// The harness level set (#4773) and the labour office perk pick (#4416, #4684). On a running island session
// (`node agent-box/session.mjs up --mission island --profile full --wait`, one client): `levelset MINER 10`
// puts the character at Mining 10 past the 2,000 XP daily cap that stops `levelupcard grant` at level 6,
// the character stands at the town hall's labour counter from the customer side (side tokens with 2 m, #4776),
// `look` must list `LF_PerkPickAction`, and `use LF_PerkPickAction` takes a perk.
//   node agent-box/session.mjs level-set-proof [counterEntity] [JOB]
//   node agent-box/session.mjs down
const COUNTER = "Fit_saint_philippe_labour_labour_counter";
const ACTION = "LF_PerkPickAction";
const SIDES = ["n2", "s2", "e2", "w2", "n", "s", "e", "w"];
const TARGET_LEVEL = 10;

// Pure: the level to set first so that the set to `target` is a real level-up (one below it, never under 1).
export function stepDownLevel(target) {
  return Math.max(1, target - 1);
}

// Pure: whether the `levelupcard state` detail names this job at this level as the last card.
export function cardReached(detail, job, level) {
  return new RegExp(`last=${job} ${level}\\b`).test(String(detail));
}

function selftest() {
  const ok = stepDownLevel(10) === 9 && stepDownLevel(1) === 1
    && cardReached("card=none last=MINER 10", "MINER", 10) && !cardReached("card=none last=DELIVERY 10", "MINER", 10)
    && !cardReached("card=none last=MINER 100", "MINER", 10) && !cardReached("card=none last=MINER 9", "MINER", 10);
  console.log(ok ? "level-set-proof selftest: OK" : "level-set-proof selftest: FAIL");
  return ok ? 0 : 1;
}

async function runLevelSetProof(args) {
  const counter = String(args?.[0] ?? COUNTER);
  const job = String(args?.[1] ?? "MINER");
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${String(r.detail).slice(0, 300)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  arrive(on, 1, wait);
  wait(4000);
  on(1, { verb: "guide", words: ["close"] });

  // A `checkpoint N` character may already hold the target level, and setting the level it already has raises no card
  // (#4962). Step to one below the target first so the set to the target is always a level-up.
  on(1, { verb: "levelset", words: [job, String(stepDownLevel(TARGET_LEVEL))] }, 10000);
  wait(2500);
  on(1, { verb: "levelset", words: [job, String(TARGET_LEVEL)] }, 10000);
  wait(2500);
  const state = on(1, { verb: "levelupcard", words: ["state"] }, 10000);
  if (!cardReached(state.detail, job, TARGET_LEVEL)) fails.push(`the level set did not reach ${job} ${TARGET_LEVEL}: ${state.detail}`);
  on(1, { verb: "capture", mode: "ui", name: "levelset_card" }, 25000);

  let offered = false;
  for (const side of SIDES) {
    const stand = on(1, { verb: "stand", entityName: counter, side }, 15000);
    if (stand.status !== "ok") continue;
    wait(1500);
    on(1, { verb: "capture", mode: "ui", name: `levelset_counter_${side}` }, 25000);
    const look = on(1, { verb: "look" }, 10000);
    if (String(look.detail).includes(ACTION)) {
      console.log(`customer side found: ${side}`);
      offered = true;
      break;
    }
  }
  if (!offered) {
    fails.push(`${ACTION} never showed at ${counter} from any side (${SIDES.join(", ")}): the counter is out of view or unreachable from the customer side`);
  } else {
    on(1, { verb: "guide", words: ["close"] });
    const take = on(1, { verb: "use", className: ACTION, entityName: counter }, 15000);
    wait(2500);
    const ui = on(1, { verb: "ui", mode: "text" }, 10000);
    on(1, { verb: "capture", mode: "ui", name: "levelset_perk_taken" }, 25000);
    console.log(`perk pick: use -> ${take.status} ${take.detail}; sheet: ${String(ui.detail).slice(0, 300)}`);
    if (take.status !== "ok") fails.push(`the perk pick did not run: ${take.status} ${take.detail}`);
  }
  // `session down` removes the client profile, so keep the pictures first (tmp/level-set-proof/, gitignored).
  try {
    const logs = join(dirname(dirname(clientBox(dir, 1))), "logs");
    const keep = join(process.cwd(), "tmp", "level-set-proof");
    mkdirSync(keep, { recursive: true });
    for (const run of readdirSync(logs)) {
      const caps = join(logs, run, "el-captures");
      if (!existsSync(caps)) continue;
      for (const f of readdirSync(caps).filter((n) => n.startsWith("levelset_"))) copyFileSync(join(caps, f), join(keep, f));
    }
    console.log(`pictures kept in ${keep}`);
  } catch (e) {
    console.log(`could not keep the pictures: ${e.message}`);
  }
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "level set proof: FAIL" : `level set proof: PASS (${job} 10 by levelset, perk pick taken at ${counter})`);
  return fails.length ? 1 : 0;
}

export default {
  name: "level-set-proof",
  usage: "level-set-proof [counterEntity] [JOB]   (--selftest runs the pure parts)",
  run: (ctx) => {
    const args = ctx?.args ?? process.argv.slice(2);
    return args.includes("--selftest") ? selftest() : runLevelSetProof(args);
  },
};

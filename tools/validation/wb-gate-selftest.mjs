// Offline proof of where `ci` runs its Workbench compile and build (tools/lib/wb-gate.mjs) with mocked
// machine RAM readings: a RAM-short PC is skipped for a roomy pool machine, roomy machines rank by
// speed, everyone short means wait, a pin is honoured, a remote that does not answer falls back, an
// early Workbench death on a short machine is an environment failure.
// inputs: tools/lib/wb-gate.mjs, tools/lib/pool-queue.mjs, tools/validation/wb-gate-selftest.mjs
// red-proof: 2026-11-14, chooseGate dropping the RAM rule (ready = first ranked row with a free
// slot, room ignored) failed "everything short: wait", "remote mode, remotes short: wait (not this
// PC)", "short PC, its slot free, roomy remote busy: queue on the remote", "roomy PC busy, remote
// short: queue here" and "no workbench cap is not ranked".

import { chooseGate, earlyDeath, parseWbState, ENV_SHORT_CODE, WB_RESERVE_MB } from "../lib/wb-gate.mjs";

const failures = [];
const check = (name, ok) => {
  if (!ok) failures.push(name);
};

// The 2026-11-14 machines: this PC 15.6 GB with ~1.5 GB free, lf-cloud 31.6 GB with ~18 GB free (2x
// slower, Workbench limit 2), lf-testbox (2x slower, limit 1).
const pc = (freeMb, busy = 0, queued = 0) => ({ busy, queued, limit: 1, freeMb, multiplier: 1 });
const box = (name, freeMb, { wbAlive = 0, wbLimit = 1, multiplier = 2 } = {}) => ({ name, caps: ["server", "workbench"], multiplier, state: { freeMb, wbAlive, wbLimit } });
const down = (name) => ({ name, caps: ["workbench"], multiplier: 2, state: null });
const at = (o) => chooseGate(o).where;

// 1. RAM before slots: the short PC with a free slot is skipped for the roomy remote
check("short PC is skipped for the roomy remote", at({ local: pc(1500), remotes: [box("lf-cloud", 18000, { wbLimit: 2 })] }) === "lf-cloud");
check("the skip names the PC's free RAM", /skipped .*this PC 1\.5 GB free/.test(chooseGate({ local: pc(1500), remotes: [box("lf-cloud", 18000)] }).why));
check("short PC, short laptop, roomy cloud: cloud", at({ local: pc(1200), remotes: [box("lf-testbox", 2000), box("lf-cloud", 18000)] }) === "lf-cloud");
check("just under reserve + floor is short", at({ local: pc(WB_RESERVE_MB + 1500 - 1), remotes: [box("lf-cloud", 18000)] }) === "lf-cloud");

// 2. Two roomy machines: speed decides (score = min(headroom, roomy) / multiplier)
check("roomy PC beats a roomy 2x-slower remote", at({ local: pc(12000), remotes: [box("lf-cloud", 18000)] }) === "local");
check("two roomy remotes: the faster wins", at({ local: pc(1000), remotes: [box("slow", 20000, { multiplier: 3 }), box("fast", 9000, { multiplier: 1 })] }) === "fast");
check("roomy remotes, equal speed: more headroom wins (below roomy)", at({ local: pc(1000), remotes: [box("a", 5000), box("b", 7000)] }) === "b");

// 3. Everyone short: wait (with the RAM of each in the reason), never start a Workbench that dies
const short = chooseGate({ local: pc(1500), remotes: [box("lf-testbox", 2500), box("lf-cloud", 3000)] });
check("everything short: wait", short.where === "wait");
check("wait names each machine's free RAM", /this PC 1\.5 GB free/.test(short.why) && /lf-cloud 2\.9 GB free/.test(short.why));
check("remote mode, remotes short: wait (not this PC)", at({ mode: "remote", local: pc(12000), remotes: [box("lf-cloud", 2000)] }) === "wait");

// 4. Slots: a full Workbench limit queues on the roomiest machine; RAM still rules
check("short PC, its slot free, roomy remote busy: queue on the remote", at({ local: pc(1500), remotes: [box("lf-cloud", 18000, { wbAlive: 2, wbLimit: 2 })] }) === "lf-cloud");
check("roomy PC busy, roomy remote free: remote", at({ local: pc(12000, 1), remotes: [box("lf-cloud", 18000)] }) === "lf-cloud");
check("roomy PC busy, remote short: queue here", at({ local: pc(12000, 1), remotes: [box("lf-cloud", 1000)] }) === "local");
check("queued counts as busy", at({ local: pc(12000, 0, 1), remotes: [box("lf-cloud", 18000)] }) === "lf-cloud");
check("Workbench limit 2 with one alive has room", at({ local: pc(1000), remotes: [box("lf-cloud", 18000, { wbAlive: 1, wbLimit: 2 })] }) === "lf-cloud");

// 5. Pins are honoured (a human or the lead), even onto a short machine
check("--local forces this PC even when short", at({ mode: "local", local: pc(500, 1, 3), remotes: [box("lf-cloud", 18000)] }) === "local");
check("--machine local pins this PC", at({ machine: "local", local: pc(500), remotes: [box("lf-cloud", 18000)] }) === "local");
check("--remote lf-testbox pinned while short", at({ mode: "remote", machine: "lf-testbox", local: pc(12000), remotes: [box("lf-testbox", 1000), box("lf-cloud", 18000)] }) === "lf-testbox");
check("--remote lf-cloud pinned while this PC is roomy", at({ mode: "remote", machine: "lf-cloud", local: pc(12000), remotes: [box("lf-cloud", 18000)] }) === "lf-cloud");

// 6. Fallbacks when a remote is down
check("pinned remote down: this PC", at({ mode: "remote", machine: "lf-cloud", local: pc(1000), remotes: [down("lf-cloud")] }) === "local");
check("--remote, every remote down: this PC", at({ mode: "remote", local: pc(1000), remotes: [down("lf-cloud"), down("lf-testbox")] }) === "local");
check("down remote is not ranked", at({ local: pc(12000), remotes: [down("lf-cloud")] }) === "local");
check("no workbench cap is not ranked", at({ local: pc(1000), remotes: [{ ...box("x", 18000), caps: ["server"] }] }) === "wait");
check("RAM read missing (ssh answered): usable", at({ local: pc(1000), remotes: [{ ...box("x", 0), state: { freeMb: null, wbAlive: 0, wbLimit: 1 } }] }) === "x");

// 7. The retry after an early death excludes the machine that died; nothing left is "none"
check("retry skips the machine that died", at({ local: pc(12000), remotes: [box("lf-cloud", 18000)], exclude: ["local"] }) === "lf-cloud");
check("retry with nothing left: none", at({ local: pc(12000), remotes: [down("lf-cloud")], exclude: ["local"] }) === "none");

// 8. Early death: no verdict within seconds on a short machine is the environment, not the PR
const nv = "validate-scripts: NO VERDICT - Workbench exited -1 but wrote no console.log";
check("6 s, no verdict, 1.5 GB free: environment", earlyDeath({ status: 1, seconds: 6, text: nv, freeMb: 1500 }) !== null);
check("the reason names the free RAM", /1\.5 GB free/.test(earlyDeath({ status: 1, seconds: 6, text: nv, freeMb: 1500 }) ?? ""));
check("a real compile error is a compile failure", earlyDeath({ status: 1, seconds: 6, text: "SCRIPT (E): foo.c(3): Unknown type", freeMb: 1500 }) === null);
check("no verdict with RAM to spare is not blamed on RAM", earlyDeath({ status: 1, seconds: 6, text: nv, freeMb: 12000 }) === null);
check("a slow no-verdict is not an early death", earlyDeath({ status: 1, seconds: 400, text: nv, freeMb: 1500 }) === null);
check("a pass is never an environment failure", earlyDeath({ status: 0, seconds: 6, text: nv, freeMb: 1500 }) === null);
check("env code is distinct from 0, 1 and the remote fallback code 3", ![0, 1, 3].includes(ENV_SHORT_CODE));

check("parse box state", JSON.stringify(parseWbState("x\nlf-wb: 1 6123\n", undefined)) === JSON.stringify({ wbAlive: 1, freeMb: 6123, wbLimit: 1 }));

if (failures.length) {
  console.log(`wb-gate-selftest: FAILED\n  ${failures.join("\n  ")}`);
  process.exit(1);
}
console.log("wb-gate-selftest: OK (RAM-ranked Workbench machine, speed, wait, pins, fallbacks, early death)");

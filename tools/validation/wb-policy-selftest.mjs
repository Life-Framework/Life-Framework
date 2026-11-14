// Offline proof that no headless Workbench starts on a PC whose pool caps lack "workbench" (the
// maintainer's editor lives on Gaming-Box): the policy read from the pool config, chooseGate and
// chooseRefresh never choosing this PC, the wait-with-a-budget for a pool machine, the routed run (the
// compile, build and look steps go to syncAndRun, never to the local runner), the backstop that makes the
// wb-lock slot refuse, the explicit --local (LF_WB_LOCAL=1) that still works, and the static rule that a
// new launcher must consult the cap. The ordinary choice rules are in wb-gate-selftest.mjs and
// rdb-refresh-where-selftest.mjs.
// inputs: tools/lib/wb-route.mjs, tools/lib/wb-gate.mjs, tools/lib/wb-lock.mjs, tools/visual/look.mjs, tools/validation/validate-wb-policy.mjs, tools/validation/wb-policy-selftest.mjs
// red-proof: 2026-11-14, four breaks, each restored. chooseGate with `localOk` always true failed 11 cases
// ("no cap: roomy free PC is never chosen", "no cap, nobody answers: wait, not this PC", "no cap, pinned
// remote down: wait", "no cap: the retry after an early death never lands here" ...); chooseRefresh's
// no-cap branch switched off failed 5 ("no cap: refresh goes to the rdb-cap machine though this PC is
// free", "no cap, no rdb machine: wait" ...); localPolicy ignoring the caps failed 13 ("caps without
// workbench: not allowed", "the slot refuses without the cap and takes no lock", "routeWorkbench never
// runs the local step without the cap" ...); requireLocalWorkbench never throwing failed "the slot refuses
// without the cap and takes no lock" and "the sync slot refuses too".

import { mkdtempSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { spawnSync } from "node:child_process";
import { chooseGate, chooseRefresh } from "../lib/wb-gate.mjs";
import * as wb from "../lib/wb-lock.mjs";
import * as route from "../lib/wb-route.mjs";
import * as pool from "../remote/pool.mjs";
import { check as policyCheck } from "./validate-wb-policy.mjs";
import { rebasePaths, remoteLookArgs } from "../visual/look.mjs";

const failures = [];
const ok = (name, cond) => {
  if (!cond) failures.push(name);
};

// This PC with no cap: roomy and idle, the worst case for a rule that merely prefers remotes.
const pc = (extra = {}) => ({ busy: 0, queued: 0, limit: 1, freeMb: 12000, multiplier: 1, hasWb: false, ...extra });
const box = (name, freeMb = 18000, { wbAlive = 0, wbLimit = 2, caps = ["server", "workbench", "rdb"] } = {}) => ({ name, caps, multiplier: 2, state: { freeMb, wbAlive, wbLimit } });
const down = (name) => ({ name, caps: ["server", "workbench", "rdb"], multiplier: 2, state: null });
const gate = (o) => chooseGate(o);

// 1. chooseGate without the cap: this PC is never a candidate and never the fallback
ok("no cap: roomy free PC is never chosen", gate({ local: pc(), remotes: [box("lf-cloud", 4000)] }).where === "lf-cloud");
ok("no cap: even when the box is short, wait (not this PC)", gate({ local: pc(), remotes: [box("lf-cloud", 1000)] }).where === "wait");
ok("no cap, nobody answers: wait, not this PC", gate({ local: pc(), remotes: [down("lf-cloud"), down("lf-testbox")] }).where === "wait");
ok("no cap, no remotes at all: wait", gate({ local: pc(), remotes: [] }).where === "wait");
ok("no cap, mode remote, nobody answers: wait", gate({ local: pc(), mode: "remote", remotes: [down("lf-cloud")] }).where === "wait");
ok("no cap, pinned remote down: wait", gate({ local: pc(), mode: "remote", machine: "lf-cloud", remotes: [down("lf-cloud")] }).where === "wait");
ok("no cap, Workbench slots busy: queue on the box", (() => { const g = gate({ local: pc(), remotes: [box("lf-cloud", 18000, { wbAlive: 2, wbLimit: 2 })] }); return g.where === "lf-cloud" && g.queue === true; })());
ok("no cap: the retry after an early death never lands here", gate({ local: pc(), remotes: [box("lf-cloud")], exclude: ["lf-cloud"] }).where === "none");
ok("no cap, retry with nothing excluded but the PC: wait", gate({ local: pc(), remotes: [down("lf-cloud")], exclude: ["local"] }).where === "wait");
ok("the wait says why", /no workbench cap/.test(gate({ local: pc(), remotes: [down("lf-cloud")] }).why));
// the explicit ask still works (a human's --local reaches chooseGate as mode "local")
ok("--local still returns this PC without the cap", gate({ local: pc(), mode: "local", remotes: [box("lf-cloud")] }).where === "local");
ok("--machine local still pins this PC", gate({ local: pc(), machine: "local", remotes: [box("lf-cloud")] }).where === "local");
// a PC with the cap is unchanged
ok("with the cap (hasWb true) a roomy PC wins", gate({ local: pc({ hasWb: true }), remotes: [box("lf-cloud")] }).where === "local");
ok("hasWb unset means the cap (older callers)", gate({ local: { busy: 0, queued: 0, limit: 1, freeMb: 12000, multiplier: 1 }, remotes: [box("lf-cloud")] }).where === "local");
ok("with the cap and every remote down, this PC", gate({ local: pc({ hasWb: true }), mode: "remote", remotes: [down("lf-cloud")] }).where === "local");

// 2. chooseRefresh without the cap: the rdb-cap machine, even when only queueing; else wait
const ref = (o) => chooseRefresh(o);
ok("no cap: refresh goes to the rdb-cap machine though this PC is free", ref({ local: pc({ buildLocked: false }), remotes: [box("lf-cloud")] }).where === "lf-cloud");
ok("no cap: a busy rdb machine is queued on, not this PC", (() => { const r = ref({ local: pc(), remotes: [box("lf-cloud", 18000, { wbAlive: 2, wbLimit: 2 })] }); return r.where === "lf-cloud" && r.queue === true; })());
ok("no cap, no rdb machine: wait", ref({ local: pc(), remotes: [box("lf-cloud", 18000, { caps: ["server", "workbench"] })] }).where === "wait");
ok("no cap, remote down: wait", ref({ local: pc(), remotes: [down("lf-cloud")] }).where === "wait");
ok("no cap, mode local (LF_RDB_REFRESH_WHERE=local) is not honoured by itself", ref({ local: pc(), mode: "local", remotes: [box("lf-cloud")] }).where !== "local");
ok("with the cap, mode local forces here", ref({ local: pc({ hasWb: true }), mode: "local", remotes: [box("lf-cloud")] }).where === "local");
ok("no cap: the reason names the cap", /no workbench cap/.test(ref({ local: pc(), remotes: [box("lf-cloud")] }).why));

// 3. the policy: caps, the explicit ask, a registry without a local entry
const machines = (caps) => [{ name: "local", via: "local", caps }, { name: "lf-cloud", via: "ssh", caps: ["server", "workbench"] }];
ok("caps without workbench: not allowed", route.localPolicy({ machines: machines(["server"]), env: {} }).allowed === false);
ok("caps with workbench: allowed", route.localPolicy({ machines: machines(["server", "workbench"]), env: {} }).allowed === true);
ok("LF_WB_LOCAL=1 allows it (forced, not hasCap)", (() => { const p = route.localPolicy({ machines: machines(["server"]), env: { LF_WB_LOCAL: "1" } }); return p.allowed && p.forced && !p.hasCap; })());
ok("LF_WB_LOCAL=0 does not", route.localPolicy({ machines: machines(["server"]), env: { LF_WB_LOCAL: "0" } }).allowed === false);
ok("no local entry keeps the old behaviour", route.localPolicy({ machines: [{ name: "x", via: "ssh", caps: [] }], env: {} }).allowed === true);

// 4. the real registry: a temp checkout whose lf.local.json removes the cap, like this PC's
pool.setHostnameForTest("selftest-host");
const dir = mkdtempSync(join(tmpdir(), "lf-wb-policy-"));
spawnSync("git", ["init", "-q", dir], { windowsHide: true });
writeFileSync(join(dir, "lf.local.json"), JSON.stringify({ pool: { machines: [{ name: "local", caps: ["server"] }] } }));
ok("registry: this PC's caps lack workbench", route.localWorkbench(dir, { env: {} }).allowed === false);
ok("registry: --local (LF_WB_LOCAL=1) allows it", route.localWorkbench(dir, { env: { LF_WB_LOCAL: "1" } }).allowed === true);
const view = route.workbenchMachines(dir, { mode: "any" }, { loadMachines: () => machines(["server"]).map((m) => ({ ...m, budgetMultiplier: 1 })), remoteState: () => ({ freeMb: 9000, wbAlive: 0, wbLimit: 2 }), env: {} });
ok("workbenchMachines: local.hasWb false, the remote with the cap listed", view.local.hasWb === false && view.remotes.length === 1 && view.remotes[0].name === "lf-cloud");
const view2 = route.workbenchMachines(dir, { mode: "any" }, { loadMachines: () => machines(["server", "workbench"]), remoteState: () => null, env: {} });
ok("workbenchMachines: local.hasWb true with the cap", view2.local.hasWb === true);

// 5. the backstop: the slot refuses and takes no lock; the explicit ask takes it
let refused = null;
try {
  await route.acquireWorkbench(dir, { purpose: "selftest", waitMs: 1000, log: () => {} });
} catch (e) {
  refused = e;
}
ok("the slot refuses without the cap and takes no lock", refused instanceof route.LocalWorkbenchRefused && refused.code === "LF_NO_LOCAL_WB" && wb.holders(dir, 1).filter((h) => h.alive).length === 0 && wb.liveTickets(dir).length === 0);
ok("the refusal says what to do", /pool machine/.test(refused?.message ?? "") && /--local/.test(refused?.message ?? "") && /LF_WB_LOCAL/.test(refused?.message ?? ""));
let syncRefused = null;
try {
  route.acquireWorkbenchSync(dir, { purpose: "selftest", waitMs: 1000, log: () => {} });
} catch (e) {
  syncRefused = e;
}
ok("the sync slot refuses too", syncRefused instanceof route.LocalWorkbenchRefused);
process.env.LF_WB_LOCAL = "1";
const slot = await route.acquireWorkbench(dir, { purpose: "selftest-forced", waitMs: 1000, log: () => {} });
ok("LF_WB_LOCAL=1 takes the slot", !!slot.name);
slot.release();
delete process.env.LF_WB_LOCAL;

// 6. waiting with a budget: a machine turning up within it is used; none means an error naming why
{
  let t = 0;
  const now = () => t;
  const sleep = async (ms) => { t += ms; };
  const logs = [];
  let calls = 0;
  const choose = () => ({ remotes: [{ name: "lf-cloud", m: { name: "lf-cloud" } }], c: ++calls < 4 ? { where: "wait", why: "lf-cloud does not answer" } : { where: "lf-cloud", why: "free slot" } });
  const p = await route.pickWorkbenchMachine(dir, { purpose: "build", budgetS: 600, pollMs: 30000, choose, sleep, now, log: (l) => logs.push(l) });
  ok("a machine that turns up within the budget is picked", p.name === "lf-cloud" && calls === 4);
  ok("one waiting line with the reason, the poll and the budget", logs.length === 1 && /waiting for a pool machine/.test(logs[0]) && /every 30 s/.test(logs[0]) && /10 min/.test(logs[0]));
  t = 0;
  const never = () => ({ remotes: [], c: { where: "wait", why: "no pool machine with the workbench cap answers" } });
  const p2 = await route.pickWorkbenchMachine(dir, { purpose: "build", budgetS: 300, pollMs: 30000, choose: never, sleep, now, log: () => {} });
  ok("past the budget: an error, no machine", !!p2.error && !p2.machine && /within 300 s/.test(p2.error) && /no pool machine/.test(p2.error));
  // the real chooseGate without the cap, nothing answering: never "local"
  t = 0;
  const real = () => ({ remotes: [down("lf-cloud")], c: chooseGate({ local: pc(), remotes: [down("lf-cloud")], mode: "remote" }) });
  const p3 = await route.pickWorkbenchMachine(dir, { purpose: "look bank", budgetS: 90, pollMs: 30000, choose: real, sleep, now, log: () => {} });
  ok("no cap, nothing answers: an error, never this PC", !!p3.error && p3.where !== "local");
}

// 7. routeWorkbench: with the cap the step runs here; without it only the pool runs it
{
  const ran = [];
  const deps = (caps, extra = {}) => ({ machines: machines(caps), env: {}, ...extra });
  const mk = (code = 0) => ({ runLocal: async () => { ran.push("local"); return 0; }, deps: { machines: machines(["server"]), env: {}, syncAndRun: async (_r, m, o) => { ran.push(`pool:${m.name}:${o.args.join(" ")}:${o.suffix}`); return { code, rDir: "C:\\LF\\rt" }; }, pick: { choose: () => ({ remotes: [{ name: "lf-cloud", m: { name: "lf-cloud", host: "lf-cloud" } }], c: { where: "lf-cloud", why: "free slot" } }) } } });
  const quiet = () => {};
  let r = await route.routeWorkbench(dir, { purpose: "build", suffix: "-build", args: ["ci", "--wb-only", "--build-only", "--local"], log: quiet, ...mk() });
  ok("without the cap the step goes to the pool with its args and suffix", r.routed && r.code === 0 && ran.join() === "pool:lf-cloud:ci --wb-only --build-only --local:-build");
  ok("routeWorkbench never runs the local step without the cap", !ran.includes("local"));
  ran.length = 0;
  r = await route.routeWorkbench(dir, { purpose: "build", suffix: "-build", args: ["ci"], log: quiet, runLocal: async () => { ran.push("local"); return 0; }, deps: { machines: machines(["server", "workbench"]), env: {}, syncAndRun: async () => { ran.push("pool"); return { code: 0 }; } } });
  ok("with the cap the step runs here and not on the pool", !r.routed && ran.join() === "local");
  ran.length = 0;
  r = await route.routeWorkbench(dir, { purpose: "build", suffix: "-build", args: ["ci"], log: quiet, runLocal: async () => { ran.push("local"); return 0; }, deps: { machines: machines(["server"]), env: { LF_WB_LOCAL: "1" }, syncAndRun: async () => { ran.push("pool"); return { code: 0 }; } } });
  ok("--local (LF_WB_LOCAL=1) runs it here without the cap", !r.routed && ran.join() === "local");
  ran.length = 0;
  const lines = [];
  r = await route.routeWorkbench(dir, { purpose: "look bank", suffix: "-look", args: ["look"], log: (l) => lines.push(l), runLocal: async () => { ran.push("local"); return 0; }, deps: { machines: machines(["server"]), env: {}, syncAndRun: async () => { ran.push("pool"); return { code: 0 }; }, pick: { budgetS: 60, pollMs: 30000, sleep: async () => {}, now: ((t0) => () => (t0 += 20000))(0), choose: () => ({ remotes: [], c: { where: "wait", why: "no pool machine with the workbench cap answers" } }) } } });
  ok("no machine within the budget: code 3, nothing ran anywhere", r.routed && r.code === 3 && ran.length === 0 && lines.some((l) => /^FAIL/.test(l)));
  ran.length = 0;
  r = await route.routeWorkbench(dir, { purpose: "build", suffix: "-build", args: ["ci"], log: quiet, ...mk(1) });
  ok("a failing remote step is a failing code, not a local retry", r.code === 1 && !ran.includes("local"));
}

// 8. look: the remote args and the paths made local
ok("remote look args: --out dropped, --serve dropped, --local --fresh --clean added", JSON.stringify(remoteLookArgs(["bank", "--out", "x", "--serve", "--close", "4"])) === JSON.stringify(["look", "bank", "--close", "4", "--local", "--fresh", "--clean"]));
const idx = rebasePaths({ contact: "C:\\LF\\rt-look\\tmp\\look\\bank\\contact.jpg", shots: [{ file: "C:\\LF\\rt-look\\tmp\\look\\bank\\wide.png" }, { file: null }], area: "bank" }, "C:\\LF\\rt-look\\tmp\\look", "D:\\w\\tmp\\look");
ok("index.json paths move under the local out dir", idx.contact.startsWith("D:\\w\\tmp\\look") && /bank/.test(idx.contact) && idx.shots[0].file.endsWith("wide.png") && idx.shots[0].file.startsWith("D:\\w\\tmp\\look") && idx.shots[1].file === null && idx.area === "bank");

// 9. the static rule: a launcher that skips the cap fails
const probe = (path, text) => policyCheck([{ path, text }]);
ok("rule 1: a direct wb-lock slot fails", probe("tools/x.mjs", 'import { acquireWorkbench } from "./lib/wb-lock.mjs";\nawait acquireWorkbench(root, {});').length === 1);
ok("rule 1: the route's own import is fine", probe("tools/lib/wb-route.mjs", 'import * as wbLock from "./wb-lock.mjs";\nwbLock.acquireWorkbench(root);').length === 0);
ok("rule 1: a comment naming wb-lock is fine", probe("tools/x.mjs", '// tools/lib/wb-lock.mjs\nawait wbRoute.acquireWorkbench(root, {});').length === 0);
ok("rule 2: an unguarded launcher fails", probe("tools/y.mjs", 'spawn("C:/Tools/ArmaReforgerWorkbenchSteamDiag.exe", args)').length === 1);
ok("rule 2: a launcher that consults the route passes", probe("tools/y.mjs", 'import * as route from "./lib/wb-route.mjs";\nspawn("ArmaReforgerWorkbenchSteamDiag.exe", args)').length === 0);
ok("rule 2: a script with the guard variable passes", probe("tools/validation/z.ps1", 'if ($env:LF_WB_GUARDED -ne "1") { exit 2 }\n$wb = "ArmaReforgerWorkbenchSteamDiag.exe"').length === 0);

if (failures.length) {
  console.log(`wb-policy-selftest: FAILED\n  ${failures.join("\n  ")}`);
  process.exit(1);
}
console.log("wb-policy-selftest: OK (no Workbench on a PC without the cap: choice, wait budget, routed run, slot backstop, --local, static launcher rule)");

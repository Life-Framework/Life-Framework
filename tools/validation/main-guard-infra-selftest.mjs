// Proves #2759: the main guard treats an infrastructure refusal (a held lock, no Workbench slot, no pool machine, a
// Workbench that died before compiling) as INFRA, never red: no recheck, no bisect, no revert, no red mark. Only a
// real failure (the compiler's words, a named failing test, any other FAIL line) is red. Also the cleanup of the reds
// set before the fix: back to compiled-ok in the hub file, and a main-guard/suite pending status that withdraws it.
// inputs: tools/lib/main-guard.mjs, tools/lib/guard-pin.mjs, tools/main-guard.mjs
// red-proof 2026-11-18: on main before the fix this printed "FAIL  cannot load: classifyStep missing" (the guard
//   judged the 2026-11-17 build transcript below by exit code alone, red, and reverted #2369 in #2736); with the
//   lock refusal pattern removed it printed "FAIL  lock-held build: infra, got {\"verdict\":\"red\"...".
import { readFileSync } from "node:fs";
import { scratchDir } from "../lib/scratch.mjs";
import * as g from "../lib/main-guard.mjs";
import * as pin from "../lib/guard-pin.mjs";

let failed = 0;
const check = (c, m) => { if (!c) { console.log(`FAIL  ${m}`); failed++; } };
if (typeof g.classifyStep !== "function" || typeof g.infraRedHeads !== "function" || typeof pin.clearInfraRed !== "function") {
  console.log("FAIL  cannot load: classifyStep missing");
  process.exit(1);
}

// The 2026-11-17 22:34Z transcript (tmp/logs/main-guard-7587bfd5-build-*.log), verbatim.
const LOCK = [
  "SUMMARY build fail exit=1 147.6s lines=3 failures=1 warnings=0 log=tmp\\logs\\build-20261117-232542.log",
  "FAIL  lock 'build' is held (C:\\LF\\agents\\Life-Framework\\tmp\\wt\\locks\\build.lock); retry with --wait to queue (a headless Workbench build is running in another worktree)",
].join("\n");

// 1. Infra refusals.
const v = g.classifyStep("build", 1, LOCK);
check(v.verdict === "infra" && /lock/.test(v.why), `lock-held build: infra, got ${JSON.stringify(v)}`);
check(g.classifyStep("build", 1, "FAIL  build lock is held by pid 12 (build in D:/x); gave up after the 900 s wait budget").verdict === "infra", "lock wait budget spent: infra");
check(g.classifyStep("build", 1, "FAIL  workbench: no free Workbench slot within 600 s (busy: a, b)").verdict === "infra", "no Workbench slot: infra");
check(g.classifyStep("build", 1, "FAIL  pool: no pool machine could take the Workbench steps after 900 s (all busy)").verdict === "infra", "no pool machine: infra");
check(g.classifyStep("build", 3, "build: lf-cloud did not answer").verdict === "infra", "build exit 3 (nobody took it): infra");
check(g.classifyStep("build", 1, "ENV   validate/validate-scripts.ps1 on box: Workbench died in 4 s; an environment failure").verdict === "infra", "Workbench died before compiling: infra");
check(g.classifyStep("pre-move e2e", 1, "FAIL  e2e: the resource database is still stale after two refreshes, so the server cannot load the pre-move tree. This is the machine (Workbench could not register the resources), not the saved build").verdict === "infra", "e2e stale rdb (the machine): infra");
check(g.classifyStep("build", 0x40010004, "").verdict === "env", "a logoff kill stays env");

// 2. Real failures stay red.
check(g.classifyStep("build", 1, `${LOCK}\nSCRIPT (E): LF_Foo.c(12): Undefined variable 'x'`).verdict === "red", "a compile error beside a lock line: red");
check(g.classifyStep("placement audit", 1, "FAIL  lock 'rdb' is held (x)\nFAIL  placement: LF_Bench floats 0.4 m above ground").verdict === "red", "a real FAIL line beside a lock line: red");
check(g.classifyStep("suite", 1, "FAIL  lock 'build' is held (x)", ["LF_Test_Shop"]).verdict === "red", "a named failing test: red");
check(g.classifyStep("build", 1, "FAIL  build: Workbench exited 1 after 300 s").verdict === "red", "an unexplained build failure: red");
check(g.classifyStep("build", 3, "SCRIPT (E): LF_Foo.c(1): broken").verdict === "red", "exit 3 with a compile error: red");
check(g.COMPILE_ERROR.test("SCRIPT    (E): LF_Foo.c(1)") && g.compileVerdict(1, "SCRIPT (E): x") === "fail", "COMPILE_ERROR matches SCRIPT (E)");

// 3. The guard acts on the class, not the exit code: a non-red first run returns before the recheck, the recheck and
// every bisect probe are classified too, and the build step queues on the lock.
const src = readFileSync(new URL("../main-guard.mjs", import.meta.url), "utf8");
const body = src.slice(src.indexOf("async function checkHead"), src.indexOf("function openFixCard"));
const iClass = body.indexOf("mg.classifyStep(step.name, r.code, r.text");
const iRecheck = body.indexOf("runCli(root, dir, step.recheck(failing)");
check(iClass > 0 && iRecheck > iClass && /if \(v1\.verdict !== "red"\)[\s\S]{0,400}return;/.test(body), "checkHead returns on a non-red first run before rechecking");
check(/classifyStep\(step\.name, again\.code/.test(body), "the recheck is classified");
check((body.match(/classifyStep\(step\.name, x\.code/g) ?? []).length === 2, "both bisect probes are classified");
check(!/mg\.isEnvExit\((r|again|x)\.code\)/.test(body), "no exit-code-only judgement left in checkHead");
check(g.STEPS.find((s) => s.name === "build")?.args.includes("--wait") && g.STEPS.find((s) => s.name === "build")?.recheck([]).includes("--wait"), "build step waits for the lock");

// 4. Stale infra reds: found from their logs, put back to compiled-ok, withdrawn on GitHub.
const heads = {
  aaaa1111: { status: "red", step: "build" },
  bbbb2222: { status: "red", step: "build" },
  cccc3333: { status: "red", step: "compile" },
  dddd4444: { status: "red", step: "suite" },
  eeee5555: { status: "green" },
};
const logs = { aaaa1111: [LOCK, LOCK], bbbb2222: [LOCK, "SCRIPT (E): broken"], cccc3333: [LOCK], dddd4444: [] };
const found = g.infraRedHeads(heads, (sha) => logs[sha]);
check(found.length === 1 && found[0].sha === "aaaa1111" && found[0].step === "build", `infraRedHeads: only the all-refusal build red, got ${JSON.stringify(found)}`);
const hub = scratchDir("lf-guard-infra-");
pin.recordHead(hub, "s0", "compiled-ok");
pin.recordHead(hub, "s1", "red", { step: "build", what: "build" });
pin.recordHead(hub, "s2", "red", { step: "compile" });
check(pin.clearInfraRed(hub, "s1", "a lock was held")?.status === "compiled-ok", "clearInfraRed: build red -> compiled-ok");
check(pin.clearInfraRed(hub, "s2", "x") === null && pin.readHeads(hub).heads.s2.status === "red", "clearInfraRed never touches a compile red");
const h = pin.readHeads(hub).heads;
check(pin.pickChecked(["s2", "s1", "s0"], (s) => h[s]?.status)?.sha === "s1", "the cleared head pins again");
const t = (i) => `2026-11-17T22:0${i}:00Z`;
const stat = [
  { context: pin.COMPILE_CTX, state: "success", created_at: t(1) },
  { context: pin.SUITE_CTX, state: "failure", created_at: t(2) },
  { ...pin.infraToGh("a lock was held"), created_at: t(3) },
];
check(pin.guardFromStatuses(stat) === "compiled-ok", `GitHub: a pending suite status over the infra red reads compiled-ok, got ${pin.guardFromStatuses(stat)}`);
check(pin.infraToGh("x".repeat(300)).description.length <= 140, "status description fits");

if (failed) process.exit(1);
console.log("PASS  main-guard-infra-selftest");

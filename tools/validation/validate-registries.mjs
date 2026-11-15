// Shared registries are per-feature files; never append to a central list.
// inputs: addons/**, agent-box/**, tools/lib/static-reset-hooks.mjs
//
// Two lists used to be conflict hotspots that every feature PR appended to:
//   - agent-box/session.mjs: the usage line and the switch of proof verbs. Each verb is now
//     agent-box/proofs/<verb>.mjs (default export { name, usage, run(ctx) }); session.mjs
//     discovers the folder and builds its usage text from it.
//   - Core/LF_StaticReset.c: every feature's world-end ResetStatics call. Each owning file now adds a
//     `modded class LF_StaticResetHooks` (tools/lib/static-reset-hooks.mjs); `tools\cli heal-hotspots`
//     moves an old branch's central entry there.
//   - Core/Events/LF_Events.c: every feature's bus events. Each event class now lives in its
//     feature, Feature/<Area>/LF_<Area>Events.c. Enforce Script compiles every file, so no include.
// This check fails a new proof verb inside session.mjs, a proof module with the wrong shape, and an
// `LF_On* : LF_Event` class outside Feature/**/LF_*Events.c (so also in Core/Events/).
//
// red-proof: 2026-11-10, a scratch `if (cmd === "zz-proof") return 0;` in session.mjs printed
// `FAIL agent-box/session.mjs:N proof verb "zz-proof"` and exited 1; a scratch
// `class LF_OnZZ : LF_Event {}` in Core/Events/LF_EventBus.c printed `FAIL ...LF_EventBus.c:N`.
import { execFileSync } from "node:child_process";
import { existsSync, readFileSync, readdirSync } from "node:fs";
import { basename, dirname, join } from "node:path";
import { fileURLToPath } from "node:url";
import { CENTRAL, centralCalls, staticResetProblems } from "../lib/static-reset-hooks.mjs";

const root = join(dirname(fileURLToPath(import.meta.url)), "..", "..");
const bad = [];

const session = readFileSync(join(root, "agent-box", "session.mjs"), "utf8").split("\n");
session.forEach((line, i) => {
  const verb = /cmd === "([a-z-]+-proof)"/.exec(line);
  if (verb) bad.push(`FAIL agent-box/session.mjs:${i + 1} proof verb "${verb[1]}": add agent-box/proofs/${verb[1]}.mjs instead`);
  if (/^(?:export )?(?:async )?function run\w*Proof\b/.test(line)) bad.push(`FAIL agent-box/session.mjs:${i + 1} ${line.trim()}: a proof is a module in agent-box/proofs/`);
});

const proofsDir = join(root, "agent-box", "proofs");
for (const f of existsSync(proofsDir) ? readdirSync(proofsDir).filter((n) => n.endsWith(".mjs") && !n.startsWith("_")) : []) {
  const text = readFileSync(join(proofsDir, f), "utf8");
  const name = basename(f, ".mjs");
  if (!text.includes(`name: ${JSON.stringify(name)}`)) bad.push(`FAIL agent-box/proofs/${f}: default export needs name: "${name}"`);
  if (!/\busage:/.test(text) || !/\brun[:(]/.test(text)) bad.push(`FAIL agent-box/proofs/${f}: default export needs { name, usage, run(ctx) }`);
}

const files = execFileSync("git", ["ls-files", "-z", "--cached", "--others", "--exclude-standard", "addons/LifeFramework/Scripts"], {
  cwd: root,
  encoding: "utf8",
  maxBuffer: 1 << 28,
}).split("\0").filter((f) => f.endsWith(".c") && existsSync(join(root, f)));
let events = 0;
for (const f of files) {
  const ok = /\/Game\/Feature\/[^/]+\/LF_[A-Za-z0-9]+Events\.c$/.test(f);
  readFileSync(join(root, f), "utf8").split("\n").forEach((line, i) => {
    if (!/^class LF_On\w+\s*:\s*LF_Event\b/.test(line)) return;
    events++;
    if (!ok) bad.push(`FAIL ${f}:${i + 1} ${line.trim()}: events live in Feature/<Area>/LF_<Area>Events.c`);
  });
}

// World-end static resets: nothing in the central file; every hook block chains super.Run.
let hooks = 0;
if (existsSync(join(root, CENTRAL))) {
  for (const c of centralCalls(readFileSync(join(root, CENTRAL), "utf8")))
    bad.push(`FAIL ${CENTRAL}:${c.line} ${c.cls}.${c.method}(): register it in ${c.cls}'s own file with a modded class LF_StaticResetHooks (tools\\cli heal-hotspots does it)`);
}
for (const f of files) {
  const text = readFileSync(join(root, f), "utf8");
  if (!text.includes("LF_StaticResetHooks")) continue;
  const r = staticResetProblems(text);
  hooks += r.hooks;
  for (const msg of r.problems) bad.push(`FAIL ${f}: ${msg}`);
}

// Harness verbs: LF_AgentHarness.Dispatch's own list is frozen at this many `cmd.Verb == "` tests (code
// lines); a new verb is a `modded class LF_AgentHarnessVerbs` in its feature's file. Only ever lower it.
const HARNESS = "addons/LifeFramework/Scripts/Game/Feature/AgentHarness/LF_AgentHarness.c";
const HARNESS_VERB_TESTS = 135;
let harnessVerbs = 0;
if (existsSync(join(root, HARNESS))) {
  for (const l of readFileSync(join(root, HARNESS), "utf8").split("\n")) if (!/^\s*\/\//.test(l)) harnessVerbs += l.split('cmd.Verb == "').length - 1;
  if (harnessVerbs > HARNESS_VERB_TESTS)
    bad.push(`FAIL ${HARNESS}: ${harnessVerbs} cmd.Verb tests, the frozen list has ${HARNESS_VERB_TESTS}; add the verb with a modded class LF_AgentHarnessVerbs in its feature's file`);
}

if (bad.length) {
  console.log(bad.slice(0, 40).join("\n"));
  console.log(`validate-registries: ${bad.length} problem(s). Shared registries are per-feature files; never append to a central list.`);
  process.exit(1);
}
console.log(`validate-registries: OK (${events} event class(es) in feature files; ${hooks} static-reset hook(s) in their own files; proofs discovered from agent-box/proofs)`);

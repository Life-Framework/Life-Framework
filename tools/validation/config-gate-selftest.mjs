// Proves the trigger logic of the gate's config harness step (#301), offline (the stages are stubbed),
// and that the release runs the full harness.
// inputs: tools/**, agent-box/**, .githooks/**, addons/**, docs/**
// red-proof: with `.conf` dropped from configRelevant this printed "a .conf diff did not run the sessions".
import { configRelevant, runConfigGate } from "../lib/config-gate.mjs";
import { gateSteps } from "../release/release.mjs";

const fails = [];
const check = (ok, msg) => { if (!ok) fails.push(msg); };
const quiet = () => {};

async function run(files, { staticCode = 0, sessionCode = 0 } = {}) {
  const calls = [];
  const code = await runConfigGate(files, {
    static: async () => { calls.push("static"); return staticCode; },
    sessions: async () => { calls.push("sessions"); return sessionCode; },
  }, quiet);
  return { calls, code };
}

let r = await run(["docs/features.md", "website/src/a.tsx", "tools/cli.mjs", "addons/LifeFramework/Prefabs/Props/Crate.et"]);
check(r.calls.join() === "static" && r.code === 0, `an unrelated diff must run static only, ran ${r.calls}`);
r = await run([]);
check(r.calls.join() === "static", "an empty diff must still run static");
for (const f of [
  "addons/LifeFramework/Configs/Shops/General.conf",
  "addons/LifeFramework/Scripts/Game/Core/Config/LF_ConfigMigration.c",
  "addons/LifeFramework/Scripts/Game/Feature/Packs/LF_PackConfigSection.c",
  "docs/config/config.schema.json",
  "tools/config/harness.mjs",
  ["tools", "test", "config-fixtures", "presets", "empty.json"].join(String.fromCharCode(92)),
  "agent-box/proofs/config-harness-proof.mjs",
]) {
  r = await run([f]);
  check(r.calls.join() === "static,sessions", `${f} did not run the sessions`);
}
r = await run(["addons/LifeFramework/Configs/a.conf"]);
check(r.calls.includes("sessions"), "a .conf diff did not run the sessions");
check(!configRelevant("addons/LifeFramework/Scripts/Game/Tests/LF_Test_X.c"), "a test triggered");
check(!configRelevant("docs/config/owner-config.md"), "owner-config.md prose triggered");
r = await run(["addons/LifeFramework/Configs/a.conf"], { sessionCode: 1 });
check(r.code === 1, "a failing session stage did not fail the gate");
r = await run(["addons/LifeFramework/Configs/a.conf"], { staticCode: 1 });
check(r.code === 1 && r.calls.join() === "static", "a failing static stage must fail the gate before any session boots");
r = await run(["docs/features.md"], { staticCode: 1 });
check(r.code === 1, "static must fail the gate even when nothing config changed");

const rel = gateSteps().find(([n]) => /config harness/.test(n));
check(rel && rel[1].includes("config-gate") && rel[1].includes("--full"), "the release does not run config-gate --full");

if (fails.length) {
  console.log("config-gate-selftest: FAIL");
  for (const f of fails) console.log("  " + f);
  process.exit(1);
}
console.log("config-gate-selftest: OK (static always, sessions only on config diffs, failures exit 1, release runs --full)");

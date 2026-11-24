import { copyFileSync, existsSync, mkdirSync, readdirSync, readFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { arrive, clientBox, newestLog, sessionDir, sessionPaths } from "../session.mjs";

// Saint-Philippe at night (#4831): the window glows and lamp heads of Lights_saint-philippe.layer switch on.
// On a running island session (`node agent-box/session.mjs up --mission island --profile full --wait`, one
// client; the playtest mission has no Lights layer): the clock is set to 22:00 through the server's `time`
// verb, the client stands beside a glow and a lamp head, takes a picture of each, and the client log must hold
// `[LFDebug:NightLight] ... on at hour` lines and no `has no LightEntity child` error. Then 12:00: the same
// lights read `off`.
//   node agent-box/session.mjs night-light-proof [glowEntity] [lampEntity]
//   node agent-box/session.mjs down
const GLOW = "Glow_saint_philippe_sp_b03_0";
const LAMP = "LampHead_saint_philippe_1";

async function runNightLightProof(args) {
  const glow = String(args?.[0] ?? GLOW);
  const lamp = String(args?.[1] ?? LAMP);
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${String(r.detail).slice(0, 300)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const nightLines = () => {
    const f = newestLog(sessionPaths(dir).clients[0].logs);
    return f ? readFileSync(f, "utf8").split("\n").filter((l) => l.includes("[LFDebug:NightLight]")).map((l) => l.replace(/^.*\[LFDebug:NightLight\]\s*/, "").trim()) : [];
  };
  const fails = [];
  arrive(on, 1, wait);
  wait(4000);
  on(1, { verb: "guide", words: ["close"] });

  on(1, { verb: "time", hour: 22 }, 10000);
  wait(3000);
  for (const [name, label] of [[glow, "glow"], [lamp, "lamp"]]) {
    on(1, { verb: "stand", entityName: name }, 20000);
    wait(32000); // one 30 s look at the clock, so the light has been evaluated at 22:00 whatever the retry did
    on(1, { verb: "capture", mode: "ui", name: `nightlight_${label}_22` }, 30000);
  }
  const night = nightLines();
  const onLines = night.filter((l) => / on at hour /.test(l));
  const missing = night.filter((l) => /could not be built/.test(l));
  console.log(`NightLight lines at 22:00: ${night.length} (${onLines.length} on, ${missing.length} errors)`);
  for (const l of night.slice(0, 12)) console.log(`    ${l}`);
  if (!onLines.length) fails.push("no [LFDebug:NightLight] `on at hour` line at 22:00: no light switched on (is the Lights layer in this mission?)");
  if (missing.length) fails.push(`${missing.length} light(s) logged "no LightEntity child"`);

  on(1, { verb: "time", hour: 12 }, 10000);
  wait(33000);
  const day = nightLines().filter((l) => / off at hour /.test(l));
  console.log(`NightLight lines at 12:00: ${day.length} off`);
  if (!day.length) fails.push("no `off at hour` line after the clock went to 12:00");

  try {
    const logs = join(dirname(dirname(clientBox(dir, 1))), "logs");
    const keep = join(process.cwd(), "tmp", "night-light-proof");
    mkdirSync(keep, { recursive: true });
    for (const run of readdirSync(logs)) {
      const caps = join(logs, run, "el-captures");
      if (!existsSync(caps)) continue;
      for (const f of readdirSync(caps).filter((n) => n.startsWith("nightlight_"))) copyFileSync(join(caps, f), join(keep, f));
    }
    console.log(`pictures kept in ${keep}`);
  } catch (e) {
    console.log(`could not keep the pictures: ${e.message}`);
  }
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "night light proof: FAIL" : `night light proof: PASS (${onLines.length} lights on at 22:00, ${day.length} off at 12:00)`);
  return fails.length ? 1 : 0;
}

export default {
  name: "night-light-proof",
  usage: "night-light-proof [glowEntity] [lampEntity]",
  run: (ctx) => runNightLightProof(ctx?.args ?? []),
};

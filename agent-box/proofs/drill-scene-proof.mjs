import { spawnSync } from "node:child_process";
import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { ROOT, arrive, clientBox, newestLog, parseState, sessionDir, sessionPaths } from "../session.mjs";

// The animated drill (#340) on a running `up --clients 1 --profile full [--sound]` session.
//  1. Late join: client 1 has the server hold a running drill in the air (`animhold`); client 2 then
//     joins and must show it from the replicated state alone: held, the sparks and both sound loops
//     running here, the chuck turning between two looks.
//  2. The vault: client 1 is handed a drill and drills the bank vault while client 2 films it from an
//     orbiting camera (with the game's sound when the session has it). During the work both clients
//     show the drill held at chest height and running; when the work ends both show it off, released
//     and back on the ground, and the server logs the scene starting and ending.
// `drill-scene-proof [--audio] [--name <clip name>]`

// What `animseen` answers, as fields.
export function parseSeen(detail) {
  const out = {};
  for (const m of String(detail).matchAll(/(\w+)=(\S+)/g)) out[m[1]] = m[2];
  return out;
}

const truthy = (v) => v === "1" || v === "true";

// Problems with one client's view of a drill that should be running in someone's hands.
export function runningFails(who, a, b) {
  const out = [];
  if (a.asset !== "LF_Drill.et") return [`${who} sees no drill (${a.asset ?? "nothing"})`];
  if (!truthy(a.running)) out.push(`${who}: the drill is not switched on`);
  if (!truthy(a.shown)) out.push(`${who}: the drill is on but this machine does not show it`);
  if (!truthy(a.posed)) out.push(`${who}: the drill is not held`);
  if (Number(a.effects) < 1) out.push(`${who}: no sparks run here`);
  if (Number(a.sounds) < 2) out.push(`${who}: ${a.sounds} of 2 sound loops run here`);
  if (!(Number(a.height) > 0.9)) out.push(`${who}: the drill is ${a.height} m above the looker's feet, not at chest height`);
  if (b && a.chuck_x === b.chuck_x) out.push(`${who}: the chuck did not turn between two looks (${a.chuck_x})`);
  return out;
}

// Problems with one client's view of a drill after the work ended.
export function stoppedFails(who, a) {
  const out = [];
  if (a.asset !== "LF_Drill.et") return [`${who} sees no drill after the work (${a.asset ?? "nothing"})`];
  if (truthy(a.running) || truthy(a.shown)) out.push(`${who}: the drill still runs after the work`);
  if (truthy(a.posed)) out.push(`${who}: the drill still hangs in the air after the work`);
  if (Number(a.effects) > 0 || Number(a.sounds) > 0) out.push(`${who}: ${a.effects} effect(s) and ${a.sounds} sound(s) still run after the work`);
  if (!(Number(a.height) < 0.6)) out.push(`${who}: the drill is ${a.height} m up after the work, not on the ground`);
  return out;
}

function readLog(logsDir) {
  const f = newestLog(logsDir);
  return f && existsSync(f) ? readFileSync(f, "utf8") : "";
}

async function runDrillSceneProof(args) {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const flag = (name, dflt) => { const i = args.indexOf(name); return i >= 0 ? args[i + 1] : dflt; };
  const name = flag("--name", `drill_scene_${Date.now() % 100000000}`);
  const send = (n) => (spec, timeout) => { h.setBoxes([clientBox(dir, n)]); return h.sendCommand(spec, timeout); };
  const on = (n, spec, timeout) => {
    const r = send(n)(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^[0-9]+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
  const seen = (n) => parseSeen(on(n, { verb: "animseen" }, 10000).detail);
  const fails = [];
  const VAULT = "Roadmap_Bank_Vault";

  // 1. Late join.
  arrive(on, 1, wait);
  wait(8000);
  on(1, { verb: "stand", entityName: VAULT });
  wait(2500);
  on(1, { verb: "look" });
  const hold = on(1, { verb: "animhold" }, 10000);
  if (hold.status !== "ok") fails.push(`animhold returned ${hold.status} ${hold.detail}`);
  const held = /<([-0-9.]+),([-0-9.]+),([-0-9.]+)>/.exec(hold.detail)?.slice(1).map(Number) ?? [0, 0, 1];
  const stood = String(parseState(on(1, { verb: "state" }, 10000).detail).pos ?? "0,0,0").split(",").map(Number);
  const ahead = [held[0] - stood[0], held[2] - stood[2]];
  const fwd = ahead.map((v) => v / (Math.hypot(...ahead) || 1));
  wait(1500);
  fails.push(...runningFails("client 1 (held drill)", seen(1), (wait(300), seen(1))));
  // On a session that already has client 2 (`up --clients 2`, when the pool has no RAM to spare for a
  // join mid-run) the late-join part is skipped and said so; the vault part still needs both clients.
  const already = (JSON.parse(readFileSync(join(dir, "session.json"), "utf8")).clients || []).some((c) => c.n === 2);
  if (already) console.log("NOTE  client 2 was in from the start: the late join is not proven by this run");
  const joined = already ? { status: 0 } : spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "session", "join", "2"], { windowsHide: true, stdio: "inherit" });
  if (joined.status !== 0) {
    fails.push("client 2 did not join late");
    for (const f of fails) console.log(`FAIL  ${f}`);
    console.log("drill-scene proof: FAIL (no second client; the vault part needs one)");
    return 1;
  } else {
    arrive(on, 2, wait);
    wait(8000);
    on(2, { verb: "stand", entityName: VAULT });
    wait(4000);
    fails.push(...runningFails("client 2 (joined late)", seen(2), (wait(300), seen(2))));
    on(2, { verb: "capture", mode: "ui", name: `${name}_late_join` }, 25000);
  }
  on(1, { verb: "animoff" }, 10000);
  wait(1500);

  // 2. The vault, filmed by client 2.
  on(1, { verb: "stand", entityName: VAULT });
  wait(2500);
  on(1, { verb: "look" });
  const [wx, wy, wz] = String(parseState(on(1, { verb: "state" }, 10000).detail).pos ?? "0,0,0").split(",").map(Number);
  // The watcher steps aside (the kit stand, a few metres along the wall) so the film shows one person.
  on(2, { verb: "stand", entityName: "Roadmap_Bank_RobKit" });
  wait(2500);
  // The camera: from the worker's left and a little behind, moving in towards the drill.
  // The worker's heading: the held drill of part 1 was straight ahead of the same standing spot.
  const right = [fwd[1], -fwd[0]];
  const p = (f, r, up) => `${(wx + fwd[0] * f + right[0] * r).toFixed(2)} ${(wy + up).toFixed(2)} ${(wz + fwd[1] * f + right[1] * r).toFixed(2)}`;
  const shot = `drone ${p(-1.1, -2.6, 1.5)} ${p(0.4, 0, 1.15)} ${p(-0.2, -1.5, 1.4)} ${p(0.6, 0.1, 1.2)} 9 50`;
  on(1, { verb: "drill" }, 10000);
  wait(1500);

  let during1 = {}, during2 = {}, during2b = {};
  const body = async () => {
    on(2, { verb: "camera", args: shot }, 10000);
    const rob = on(1, { verb: "use", className: "LF_BankRobAction", entityName: VAULT }, 10000);
    if (rob.status !== "ok") fails.push(`the drill rob returned ${rob.status} ${rob.detail}`);
    await sleep(2500);
    during2 = seen(2);
    await sleep(300);
    during2b = seen(2);
    during1 = seen(1);
    on(2, { verb: "capture", mode: "ui", name: `${name}_drilling` }, 25000);
    await sleep(6500);
  };
  const eng = await import("../../tools/film/engine.mjs");
  const resolveAlias = (alias) => { h.setBoxes([clientBox(dir, 2)]); return h.resolveCapture(alias); };
  let clip = null;
  if (args.includes("--audio")) {
    const audio = await import("../../tools/film/audio.mjs");
    const pid = audio.clientPid(join(dir, "session.json"), 2);
    const r = await audio.shootWithAudio({ send: send(2), resolveAlias, name, fps: 30, seconds: 40, pid, body, label: "drill-scene" });
    if (r.ok) clip = r.out;
    else {
      fails.push(`the clip with sound failed: ${r.error}`);
      clip = r.clip?.out ?? null;
    }
    if (r.ok) console.log(`clip with sound: ${r.out} (mean ${r.mean} dB, max ${r.max} dB)`);
  } else {
    const start = eng.engineStart(send(2), name, { fps: 30, seconds: 40 });
    if (!start.ok) fails.push(`film start failed: ${start.error}`);
    await body();
    const stop = eng.engineStop(send(2), resolveAlias, { label: "drill-scene", fps: 30 });
    if (stop.ok) clip = stop.out;
    else fails.push(`film stop failed: ${stop.error}`);
  }
  if (clip) console.log(`clip: ${clip}`);

  fails.push(...runningFails("client 2 (watching the vault)", during2, during2b));
  fails.push(...runningFails("client 1 (drilling)", during1));
  wait(1500);
  fails.push(...stoppedFails("client 2", seen(2)));
  fails.push(...stoppedFails("client 1", seen(1)));
  on(2, { verb: "capture", mode: "ui", name: `${name}_after` }, 25000);

  const paths = sessionPaths(dir, 2);
  const server = readLog(join(paths.server, "logs")) || readLog(paths.server);
  if (!server.includes("drill scene started for")) fails.push("the server never logged 'drill scene started'");
  if (!server.includes("drill scene ended: work finished")) fails.push("the server never logged 'drill scene ended: work finished'");
  for (const [i, c] of paths.clients.entries()) {
    const text = readLog(c.logs);
    for (const bad of ["did not spawn on", "has no socket", "work animation not visible"]) if (text.includes(bad)) fails.push(`client ${i + 1} logged '${bad}'`);
  }
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "drill-scene proof: FAIL" : "drill-scene proof: PASS (a late joiner showed the running drill; at the vault both clients showed it held, spinning, sparking and sounding, then off and on the ground)");
  return fails.length ? 1 : 0;
}

export default {
  name: "drill-scene-proof",
  usage: "drill-scene-proof [--audio] [--name <clip name>]",
  run: (ctx) => runDrillSceneProof(ctx.args ?? []),
};

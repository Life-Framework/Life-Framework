import { readFileSync } from "node:fs";
import { join } from "node:path";
import { ROB_TARGET, arrive, clientBox, newestLog, sessionDir, sessionPaths } from "../session.mjs";

// Robbery alarms (#740) on a running session, driven the way a player does it.
//   node agent-box/session.mjs up            (one client; a sound profile only to hear it)
//   node agent-box/session.mjs robbery-alarm-proof
//   node agent-box/session.mjs down
// Robs the test till (a fuel station till, alarm kind "fuel") and reads the logs: the server spawned the
// fuel alarm prefab, and the client's alarm entity started the fuel alarm's own sound event. Also the
// demo: stand at the till and listen (the fuel pulses), then try the bank vault and the store till.
async function runRobberyAlarmProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail.slice(0, 200)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const paths = sessionPaths(dir);
  const read = (logsDir) => {
    const f = newestLog(logsDir);
    return f ? readFileSync(f, "utf8") : "";
  };
  const fails = [];

  arrive(on, 1, wait);
  wait(4000);
  on(1, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  on(1, { verb: "look" });
  const rob = on(1, { verb: "use", className: "LF_RobAction", entityName: ROB_TARGET }, 10000);
  if (rob.status !== "ok") fails.push(`rob returned ${rob.status} ${rob.detail}`);
  wait(6000);

  const server = read(join(paths.server, "logs"));
  if (!/alarm sounding at .*kind 'fuel', prefab \S*LF_SiteAlarm_Fuel(Loud)?\.et/.test(server)) fails.push("the server log has no fuel alarm spawn line (kind 'fuel', prefab LF_SiteAlarm_Fuel)");
  const client = read(paths.clients[0].logs);
  if (!/site alarm sounding: \S*Alarm_Robbery_Fuel\.acp SOUND_LF_ALARM_FUEL_LP/.test(client)) fails.push("the client log has no 'site alarm sounding' line for SOUND_LF_ALARM_FUEL_LP");

  if (fails.length) {
    console.log("robbery-alarm-proof: FAIL");
    for (const f of fails) console.log(`  ${f}`);
    return 1;
  }
  console.log("robbery-alarm-proof: OK (the fuel alarm spawned and its own sound started)");
  return 0;
}

export default { name: "robbery-alarm-proof", usage: "robbery-alarm-proof", run: () => runRobberyAlarmProof() };

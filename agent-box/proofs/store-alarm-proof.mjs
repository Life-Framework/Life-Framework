import { readFileSync } from "node:fs";
import { join } from "node:path";
import { arrive, clientBox, newestLog, sessionDir, sessionPaths } from "../session.mjs";

// The general store till rings the shop burglar bell, not the till buzzer (kind "shopbell").
//   node agent-box/session.mjs up
//   node agent-box/session.mjs store-alarm-proof
//   node agent-box/session.mjs down
// Robs the DebugWorld general store till and reads the logs: the server spawned the shop bell alarm prefab and the
// client's alarm entity started SOUND_LF_ALARM_SHOPBELL_LP. Standing at the till is the way to listen.
async function runStoreAlarmProof() {
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
  const STORE_TILL = "Shop_GeneralStore_Till";
  const fails = [];

  arrive(on, 1, wait);
  wait(4000);
  on(1, { verb: "stand", entityName: STORE_TILL });
  wait(2500);
  on(1, { verb: "look" });
  const rob = on(1, { verb: "use", className: "LF_RobAction", entityName: STORE_TILL }, 10000);
  if (rob.status !== "ok") fails.push(`rob returned ${rob.status} ${rob.detail}`);
  wait(6000);

  const server = read(join(paths.server, "logs"));
  if (!/alarm sounding at .*kind 'shopbell', prefab \S*LF_SiteAlarm_ShopBell(Loud)?\.et/.test(server)) fails.push("the server log has no shop bell alarm spawn line (kind 'shopbell', prefab LF_SiteAlarm_ShopBell)");
  const client = read(paths.clients[0].logs);
  if (!/site alarm sounding: \S*Alarm_Robbery_ShopBell\.acp SOUND_LF_ALARM_SHOPBELL_LP/.test(client)) fails.push("the client log has no 'site alarm sounding' line for SOUND_LF_ALARM_SHOPBELL_LP");

  if (fails.length) {
    console.log("store-alarm-proof: FAIL");
    for (const f of fails) console.log(`  ${f}`);
    return 1;
  }
  console.log("store-alarm-proof: OK (the shop bell spawned and its own sound started)");
  return 0;
}

export default { name: "store-alarm-proof", usage: "store-alarm-proof", run: () => runStoreAlarmProof() };

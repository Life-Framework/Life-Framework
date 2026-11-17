import { spawnSync } from "node:child_process";
import { existsSync, readFileSync, readdirSync } from "node:fs";
import { join } from "node:path";
import { ROOT, arrive, clientBox, newestLog, sessionDir, sessionPaths } from "../session.mjs";

// #2475: a client that joins after the Annex police UAZ (`Annex_Escort_Car`) spawned holds the registration the
// server issued for it. The server logs `[LFDebug:LicensePlate] server applied plate <X> on Annex_Escort_Car`
// when it issues the plate; a late client logs `client applied plate <X> on ` (the entity name is empty on a
// client) when the vehicle's initial state (RplLoad) reaches it. Plates are unique (LF_LicensePlateRegistry), so
// client 2 must log exactly the plate the server issued for that car. Only the newest server log and client-2 log folders created after the join are read, so a line from
// an earlier boot or join cannot pass the proof.
//
//   session up --clients 1 ; session plate-late-proof [--vehicle Annex_Escort_Car] [--seconds 60]
//   plate-late-proof --selftest      the parsers on made-up log text, no game
//
// Red: without the client apply (RplLoad / the onRpl handler) client 2 never logs `client applied plate`, and
// the proof fails at "client 2 applied a plate".
const DEFAULT_VEHICLE = "Annex_Escort_Car";
const DEFAULT_SECONDS = 60;

const escape = (s) => s.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");

// The last plate the server applied on the named vehicle, or null.
export function serverPlate(text, vehicle) {
  const re = new RegExp(`\\[LFDebug:LicensePlate\\][^\\n]*?server applied plate (.+?) on ${escape(vehicle)} \\(`, "g");
  let last = null;
  for (const m of String(text).matchAll(re)) last = m[1];
  return last;
}

// True when a client log shows exactly that plate applied.
export function clientApplied(text, plate) {
  return new RegExp(`\\[LFDebug:LicensePlate\\][^\\n]*?client applied plate ${escape(plate)} on `).test(String(text));
}

// console.log text of the log folders under logsDir, skipping the names in `skip`.
function readLogs(logsDir, skip = new Set()) {
  const out = [];
  if (!existsSync(logsDir)) return "";
  for (const d of readdirSync(logsDir).sort()) {
    if (skip.has(d)) continue;
    const p = join(logsDir, d, "console.log");
    if (existsSync(p)) {
      try { out.push(readFileSync(p, "utf8")); } catch { /* being written */ }
    }
  }
  return out.join("\n");
}

const logDirs = (logsDir) => new Set(existsSync(logsDir) ? readdirSync(logsDir) : []);

function selftest() {
  const fails = [];
  const server = "12:00:01 SCRIPT : [LFDebug:LicensePlate] server applied plate KV LB 4821 on Annex_Escort_Car (1 plates)\n"
    + "12:00:01 SCRIPT : [LFDebug:LicensePlate] server applied plate AA BB 0001 on Other_Car (1 plates)\n";
  const client = "12:03:10 SCRIPT : [LFDebug:LicensePlate] client applied plate AA BB 0001 on  (1 plates)\n"
    + "12:03:10 SCRIPT : [LFDebug:LicensePlate] client applied plate KV LB 4821 on  (1 plates)\n";
  if (serverPlate(server, DEFAULT_VEHICLE) !== "KV LB 4821") fails.push("server plate");
  if (!clientApplied(client, "KV LB 4821")) fails.push("client plate");
  if (clientApplied(client, "KV LB 482")) fails.push("plate is matched whole");
  if (clientApplied(server, "KV LB 4821")) fails.push("side is respected");
  if (serverPlate(server, "Missing_Car") !== null) fails.push("vehicle is respected");
  if (clientApplied("", "KV LB 4821")) fails.push("empty log");
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "plate-late-proof selftest: FAIL" : "plate-late-proof selftest: PASS");
  return fails.length ? 1 : 0;
}

async function runPlateLate(args) {
  if (args.includes("--selftest")) return selftest();
  const val = (f, d) => (args.includes(f) ? args[args.indexOf(f) + 1] : d);
  const vehicle = val("--vehicle", DEFAULT_VEHICLE);
  const seconds = Number(val("--seconds", DEFAULT_SECONDS));

  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const paths = sessionPaths(dir);
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    return h.sendCommand(spec, timeout);
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const serverText = () => {
    const f = newestLog(join(paths.server, "logs"));
    try { return f ? readFileSync(f, "utf8") : ""; } catch { return ""; }
  };
  const client2Logs = paths.clients[1].logs;
  const fails = [];
  const done = () => {
    for (const f of fails) console.log(`FAIL  ${f}`);
    console.log(fails.length ? "plate-late-proof: FAIL" : `plate-late-proof: PASS (a late joiner holds the plate the server issued for ${vehicle})`);
    return fails.length ? 1 : 0;
  };

  arrive(on, 1, wait);
  const issued = serverPlate(serverText(), vehicle);
  console.log(`server issued ${vehicle}: ${issued ?? "(no line)"}`);
  if (!issued) { fails.push(`server issue: the newest server log has no 'server applied plate ... on ${vehicle}' line`); return done(); }

  const before = logDirs(client2Logs);
  const joined = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "session", "join", "2"], { windowsHide: true, stdio: "inherit" });
  if (joined.status !== 0) { fails.push("join: client 2 did not join late"); return done(); }
  arrive(on, 2, wait);

  // Bounded: the line lands when the vehicle's initial state reaches client 2.
  let seen = false;
  for (let t = 0; t < seconds && !seen; t += 2) {
    seen = clientApplied(readLogs(client2Logs, before), issued);
    if (!seen) wait(2000);
  }
  console.log(`client 2 applied ${issued}: ${seen ? "yes" : "no"}`);
  if (!seen) fails.push(`client 2 holds the plate: no 'client applied plate ${issued}' line in a log from this join within ${seconds} s`);
  return done();
}

export default {
  name: "plate-late-proof",
  noReset: true,
  usage: "plate-late-proof [--vehicle Annex_Escort_Car] [--seconds 60] | --selftest   (session up --clients 1)",
  run: (ctx) => runPlateLate(ctx.args ?? []),
};

import { existsSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { arrive, clientBox, newestLog, parseState, sessionDir, sessionPaths } from "../session.mjs";

// The log is the truth about the animation (#513): the server must start and stop it and never
// time it out, and every client must report the worker loitering. A server line alone proves nothing.
export function judgeWorkAnimLogs(serverText, clientTexts) {
  const out = [];
  const bad = [
    ["work animation not visible", "the worker is not in the work loiter"],
    ["the worker has no character controller", "no character controller"],
    ["the worker is already in a loiter", "the work pose was refused (already loitering)"],
  ];
  for (const kind of ["PICK", "DRILL"]) {
    if (!serverText.includes(`: ${kind} for`)) out.push(`server never logged 'work animation started ... ${kind}'`);
  }
  if (serverText.includes("STOP_TIMEOUT")) out.push("server stopped a work animation with STOP_TIMEOUT");
  if (serverText.includes("STOP_RANGE")) out.push("server stopped a work animation with STOP_RANGE");
  if (!serverText.includes("work animation stopped: work finished")) out.push("the drill animation never finished on its own timer");
  if (!serverText.includes("work animation stopped: pick attempt")) out.push("the pick animation never stopped with its attempt");
  const all = [["server", serverText], ...clientTexts.map((t, i) => [`client ${i + 1}`, t])];
  for (const [who, text] of all) for (const [needle, label] of bad) if (text.includes(needle)) out.push(`${who} logged: ${label}`);
  clientTexts.forEach((t, i) => {
    const playing = (t.match(/work animation playing/g) ?? []).length;
    // The worker's own machine may check a 6 s job after it ended when the box lags: that counts, the watcher must see all four.
    const finished = (t.match(/work animation finished before the check/g) ?? []).length;
    if (i > 0 && playing < 4) out.push(`client ${i + 1} logged 'work animation playing' ${playing} time(s), expected 4`);
    if (i === 0 && playing + finished < 4) out.push(`client ${i + 1} logged 'work animation playing' ${playing} and 'finished before the check' ${finished} time(s), expected 4 together (vault drill, door pick, car pick, van box drill)`);
  });
  return out;
}

function readLog(logsDir) {
  const f = newestLog(logsDir);
  return f && existsSync(f) ? readFileSync(f, "utf8") : "";
}

function readLogs(dir) {
  const paths = sessionPaths(dir, 2);
  return [readLog(join(paths.server, "logs")) || readLog(paths.server), ...paths.clients.map((c) => readLog(c.logs))];
}

// M2 #273, #411: the robber drills the bank vault, then picks the lock-up door, the locked car and drills the cash van box, and the other
// client watches each. Client 1 is handed the tools (dev gate, standing in for the fence). Client 2
// orbits a camera on the worker and takes UI captures, so neither body moves. The server and both
// clients must log `work animation started / playing / stopped`.
// `work-anim-proof [--name <shot prefix>]`
async function runWorkAnimProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const flag = (name, dflt) => { const i = process.argv.indexOf(name); return i > 0 ? process.argv[i + 1] : dflt; };
  const prefix = flag("--name", "m2_513_work");
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const state = (n) => on(n, { verb: "state" }, 10000).detail;
  const at = (n) => String(parseState(state(n)).pos ?? "0,0,0").split(",").map(Number);
  const fails = [];
  const need = (cond, text) => { if (!cond) fails.push(text); };
  const before = readLogs(dir).map((text) => text.length);

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  // Client 2 closes a leftover bill, then both stand at the vault.
  on(2, { verb: "click", widget: "BillClose" });

  const watch = (door, orbitSeconds) => {
    on(2, { verb: "stand", entityName: door });
    wait(2500);
    on(1, { verb: "stand", entityName: door });
    wait(2500);
    on(1, { verb: "look" });
    const [wx, wy, wz] = at(1);
    on(2, { verb: "camera", args: `orbit ${wx.toFixed(2)} ${(wy + 1).toFixed(2)} ${wz.toFixed(2)} 3 ${orbitSeconds}` }, 10000);
    wait(1500);
  };

  // The vault drill (6 s of work).
  watch("Roadmap_Bank_Vault", 8);
  on(1, { verb: "drill" }, 10000);
  wait(1500);
  const rob = on(1, { verb: "use", className: "LF_BankRobAction", entityName: "Roadmap_Bank_Vault" }, 10000);
  need(rob.status === "ok", `the drill rob returned ${rob.status} ${rob.detail}`);
  wait(2200);
  on(2, { verb: "capture", mode: "ui", name: `${prefix}_drill` }, 25000);
  wait(5000);

  // The lock-up door pick (6 to 20 s of work).
  watch("Roadmap_Lockpick_Door", 24);
  on(1, { verb: "lockpick" }, 10000);
  wait(1500);
  const pick = on(1, { verb: "use", className: "LF_LockpickAction", entityName: "Roadmap_Lockpick_Door" }, 10000);
  need(pick.status === "ok", `the pick returned ${pick.status} ${pick.detail}`);
  wait(2200);
  on(2, { verb: "capture", mode: "ui", name: `${prefix}_pick` }, 25000);
  // The attempt runs at most 20 s; wait for its own finish, then for the clients to log the end.
  wait(21000);

  // The locked car (#513 follow-up): the same pick, on a vehicle.
  watch("Roadmap_Lockpick_Car", 10);
  const carPick = on(1, { verb: "use", className: "LF_LockpickAction", entityName: "Roadmap_Lockpick_Car" }, 10000);
  need(carPick.status === "ok", `the car pick returned ${carPick.status} ${carPick.detail}`);
  wait(2200);
  on(2, { verb: "capture", mode: "ui", name: `${prefix}_car` }, 25000);
  wait(21000);

  // The cash van box (#475): the same drill, on the box that rides the van.
  watch("Roadmap_Cash_Van_Box", 10);
  on(1, { verb: "drill" }, 10000);
  wait(1500);
  const box = on(1, { verb: "use", className: "LF_HeistRobAction", entityName: "Roadmap_Cash_Van_Box" }, 10000);
  need(box.status === "ok", `the van box drill returned ${box.status} ${box.detail}`);
  wait(2200);
  on(2, { verb: "capture", mode: "ui", name: `${prefix}_vanbox` }, 25000);
  wait(8000);

  const [serverText, ...clientTexts] = readLogs(dir).map((text, i) => text.slice(before[i] ?? 0));
  fails.push(...judgeWorkAnimLogs(serverText, clientTexts));
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "work-anim proof: FAIL" : "work-anim proof: PASS (drill and pick worked on screen; the server and both clients logged the animation playing and stopping)");
  return fails.length ? 1 : 0;
}

export default {
  name: "work-anim-proof",
  usage: "work-anim-proof [--name <shot prefix>]",
  run: (ctx) => runWorkAnimProof(),
};

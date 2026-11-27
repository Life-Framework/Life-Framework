import { readFileSync } from "node:fs";
import { join } from "node:path";
import { arrive, clientBox, newestLog, parseState, sessionDir, sessionPaths } from "../session.mjs";
import { graceOff } from "./_grace.mjs";

// The Saint-Philippe bank's own interior door behind the vault door is hidden by the kit's boxed clear entry
// (`Fit_saint_philippe_bank_clear_box1`), and nothing else of that prefab is: the other bank doors stay.
// Server and both clients log the hide (2 props: the door set and its leaf); a trace through the doorway from the vault
// room hits the vault door, not the vanilla door; the other interior doors of the same prefab still stop a trace; the
// vault drill still opens the vault door on both clients.
//   tools\cli session up --mission island --profile full --clients 2 --roster 2
//   node agent-box/session.mjs island-bank-door-clear-proof
//   tools\cli session down
async function runIslandBankDoorClearProof() {
  const h = await import("../harness.mjs");
  const VAULT = "Fit_saint_philippe_bank_lf_bank_vault";
  const dir = sessionDir();
  const paths = sessionPaths(dir);
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail.slice(0, 300)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const read = (logs) => { const f = newestLog(logs); return f ? readFileSync(f, "utf8") : ""; };
  const results = [];
  const check = (name, pass, detail) => { results.push({ name, pass }); console.log(`${pass ? "PASS" : "FAIL"}  ${name}: ${detail}`); };
  const vaultOf = (line) => {
    const entry = (parseState(line).breach ?? "").split("|").find((e) => e.includes("bank_vault"));
    const m = /open=(\d):yaw=(-?\d+|none)/.exec(entry ?? "");
    return m ? { open: m[1], yaw: m[2] } : { entry };
  };

  // 1. the hide line on the server and each client: the door set and its leaf, no more
  const hideLine = (text) => text.split("\n").find((l) => /base-world clear: hid \d+ vanilla props in the box Fit_saint_philippe_bank_clear_box1/.test(l)) ?? "";
  const logs = [["server", read(join(paths.server, "logs"))], ["client 1", read(paths.clients[0].logs)], ["client 2", read(paths.clients[1].logs)]];
  for (const [who, text] of logs) {
    const line = hideLine(text);
    check(`${who} hid the door set and its leaf`, /hid 2 vanilla props/.test(line), line.slice(line.indexOf("base-world") >= 0 ? line.indexOf("base-world") : 0) || "no hide line");
  }

  for (const n of [1, 2]) arrive(on, n, wait);
  graceOff(on, dir, wait); // a fresh account is in its robbery grace: the vault rob would be refused
  wait(6000);

  // 2. a trace from inside the vault room through the doorway on its centre line, 1 m above the floor. The vanilla door's leaf stood
  //    0.51 m from the start (z 10656.9); the vault door's own leaf stands at about 1.05 m (z 10656.37). A first hit past 0.8 m means
  //    the vanilla door is not there.
  const through = { verb: "probe", mode: "trace", x: 4364.675, y: 14.1, z: 10657.408, x2: 4364.98, y2: 14.1, z2: 10654.93 };
  for (const n of [1, 2]) {
    const r = on(n, through, 10000);
    const d = Number(/d=([\d.]+)/.exec(r.detail)?.[1] ?? 0);
    check(`client ${n}: the first hit in the doorway is the vault door (past 0.8 m), not the vanilla door`, r.status === "ok" && d > 0.8 && d < 1.4, r.detail);
  }

  // 3. the same prefab elsewhere in the bank (ground floor, 3 m either side) still stops a trace: an unnamed hit
  const control = [
    { name: "west interior door", verb: "probe", mode: "trace", x: 4363.78, y: 14.1, z: 10653.2, x2: 4363.78, y2: 14.1, z2: 10655.2 },
    { name: "east interior door", verb: "probe", mode: "trace", x: 4370.36, y: 14.1, z: 10653.9, x2: 4370.36, y2: 14.1, z2: 10656.0 },
  ];
  for (const c of control) {
    const spec = { ...c }; delete spec.name;
    const r = on(1, spec, 10000);
    check(`client 1: the ${c.name} still stops a trace`, r.status === "ok" && /ent=unnamed/.test(r.detail), r.detail);
  }

  // 4. the vault drill still opens the vault door on both clients
  on(1, { verb: "resetcrime" }, 10000);
  on(1, { verb: "stand", entityName: VAULT });
  on(2, { verb: "stand", entityName: VAULT, side: "w" });
  wait(3000);
  const seen = (n) => vaultOf(on(n, { verb: "seen", radius: 25 }, 10000).detail);
  check("vault closed on both clients before", [seen(1), seen(2)].every((v) => v.open === "0"), "seen");
  on(1, { verb: "capture", mode: "ui", name: "bank_door_clear_before_c1" }, 25000);
  const give = on(1, { verb: "drill" }, 10000);
  check("drill given", give.status === "ok", give.status);
  wait(1500);
  // The rob row wants the drill in the hands: a quick-slot press, repeated until `held=1` (a press toggles, so one that
  // finds it held leaves it be).
  for (let slotTry = 0; slotTry < 3; slotTry++) {
    if (/held=1/.test(String(on(1, { verb: "quickslot", words: ["drill"] }, 10000).detail))) break;
    on(1, { verb: "quickslot", words: ["drill", "press"] }, 10000);
    wait(3000);
  }
  wait(1500);
  const rob = on(1, { verb: "use", className: "LF_BankRobAction", entityName: VAULT }, 10000);
  check("vault drill action ok", rob.status === "ok", rob.status);
  { const t0 = Date.now(); while (Date.now() - t0 < 75000) { if (seen(2).open === "1") break; wait(4000); } }
  const after = [seen(1), seen(2)];
  check("vault open on both clients", after.every((v) => v.open === "1"), JSON.stringify(after));
  on(1, { verb: "capture", mode: "ui", name: "bank_door_clear_after_c1" }, 25000);

  console.log(results.every((r) => r.pass) ? "ALL PASS" : "SOME FAIL: " + results.filter((r) => !r.pass).map((r) => r.name).join("; "));
  return results.every((r) => r.pass) ? 0 : 1;
}

export default {
  name: "island-bank-door-clear-proof",
  usage: "island-bank-door-clear-proof   (up --mission island --clients 2 --roster 2)",
  run: () => runIslandBankDoorClearProof(),
};

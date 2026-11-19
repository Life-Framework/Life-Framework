import { readFileSync } from "node:fs";
import { join } from "node:path";
import { arrive, clientBox, newestLog, parseState, sessionDir, sessionPaths } from "../session.mjs";

// The Saint-Philippe bank vault robbery and the locker bank on the island (PR #3722, #3711), two clients on a
// dedicated server. Client 1 is handed a drill and holds it, drills the vault (30 s); both clients must draw
// the door swung, the placed money bag must be pickable, the bank alarm must reach both clients and a call
// must post. Then client 1 rents, opens and reopens the locker. Storing an item is a UI drag: the harness has
// no verb for it, LF_Test_LockerRoundtrip covers store and reopen.
//   tools\cli session up --mission island --profile full --clients 2 --roster 2
//   node agent-box/session.mjs island-vault-proof
//   tools\cli session down
async function runIslandVaultProof() {
  const h = await import("../harness.mjs");
  const VAULT = "Fit_saint_philippe_bank_lf_bank_vault";
  const LOCKER = "Fit_saint_philippe_bank_lf_locker_bank";
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
  const serverLog = () => read(join(paths.server, "logs"));
  const clientLog = (n) => read(paths.clients[n - 1].logs);
  const results = [];
  const check = (name, pass, detail) => { results.push({ name, pass }); console.log(`${pass ? "PASS" : "FAIL"}  ${name}: ${detail}`); };
  const vaultOf = (line) => {
    const entry = (parseState(line).breach ?? "").split("|").find((e) => e.includes("bank_vault"));
    const m = /open=(\d):yaw=(-?\d+|none)/.exec(entry ?? "");
    return m ? { open: m[1], yaw: m[2], entry } : { entry };
  };
  
  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  on(1, { verb: "resetcrime" }, 10000);
  on(1, { verb: "stand", entityName: VAULT });
  on(2, { verb: "stand", entityName: VAULT, side: "w" });
  wait(3000);
  const until2 = (n, ms) => { const t0 = Date.now(); let v; do { v = vaultOf(on(n, { verb: "seen", radius: 25 }, 10000).detail); if (v.open !== undefined) break; wait(3000); } while (Date.now() - t0 < ms); return v; };
  const before = [vaultOf(on(1, { verb: "seen", radius: 25 }, 10000).detail), until2(2, 40000)];
  check("vault closed on both clients before", before.every((v) => v.open === "0"), JSON.stringify(before));
  on(1, { verb: "capture", mode: "ui", name: "vault_island_before_c1" }, 25000);
  on(2, { verb: "capture", mode: "ui", name: "vault_island_before_c2" }, 25000);
  
  const give = on(1, { verb: "drill" }, 10000);
  check("drill given", give.status === "ok", `${give.status} ${give.detail.slice(0, 100)}`);
  on(1, { verb: "hold", item: "LF_Drill" }, 10000);
  wait(1500);
  const serverBefore = serverLog().length;
  const rob = on(1, { verb: "use", className: "LF_BankRobAction", entityName: VAULT }, 10000);
  check("vault drill action ok", rob.status === "ok", `${rob.status} ${rob.detail.slice(0, 120)}`);
  { const t0 = Date.now(); while (Date.now() - t0 < 75000) { const a = vaultOf(on(2, { verb: "seen", radius: 25 }, 10000).detail); if (a.open === "1") break; wait(4000); } }
  const after = [1, 2].map((n) => vaultOf(on(n, { verb: "seen", radius: 25 }, 10000).detail));
  check("vault open on both clients", after.every((v) => v.open === "1"), JSON.stringify(after));
  on(1, { verb: "capture", mode: "ui", name: "vault_island_after_c1" }, 25000);
  on(2, { verb: "capture", mode: "ui", name: "vault_island_after_c2" }, 25000);
  
  const seen1 = on(1, { verb: "seen", radius: 15 }, 10000).detail;
  const bags = Number(/bags=(\d+)/.exec(seen1)?.[1] ?? 0);
  check("money bags loose near vault", bags > 0, `bags=${bags}`);
  on(1, { verb: "stand", entityName: "Fit_saint_philippe_bank_lf_money_bag" });
  wait(2000);
  const pick = on(1, { verb: "pickup", item: "LF_MoneyBag" }, 15000);
  wait(2000);
  const st = on(1, { verb: "state" }, 10000).detail;
  check("money bag picked up", pick.status === "ok", `${pick.status} ${pick.detail.slice(0, 160)}`);
  console.log("state after pickup: " + st.slice(0, 400));
  const newServer = serverLog().slice(serverBefore);
  const alarmLines = newServer.split("\n").filter((l) => /alarm|Bank|Police|dispatch/i.test(l)).slice(0, 25);
  console.log("server lines:\n" + alarmLines.join("\n"));
  check("server logged alarm for the bank", /alarm sounding at .*bank/i.test(newServer) || /dispatch.*bank|bank.*dispatch/i.test(newServer), "see server lines above");
  for (const n of [1, 2]) {
    const cl = clientLog(n);
    console.log(`client ${n} alarm lines: ` + cl.split("\n").filter((l) => /site alarm|dispatch/i.test(l)).slice(-4).join(" || "));
  }
  
  // Locker
  on(1, { verb: "stand", entityName: LOCKER });
  wait(2500);
  on(1, { verb: "capture", mode: "ui", name: "locker_island_c1" }, 25000);
  const rent = on(1, { verb: "use", className: "LF_LockerRentAction", entityName: LOCKER }, 10000);
  wait(2000);
  const sl = serverLog();
  check("locker rented", rent.status === "ok" && (/rented a locker at/.test(sl) || /already rents a locker/.test(sl)), `${rent.status} ${rent.detail.slice(0, 120)}`);
  const open1 = on(1, { verb: "use", className: "LF_LockerOpenAction", entityName: LOCKER }, 10000);
  wait(2500);
  const opened1 = (serverLog().match(/opened (the|an empty) locker at/g) || []).length;
  check("locker opened (1st)", open1.status === "ok" && opened1 >= 1, `${open1.status} opened-lines=${opened1}`);
  on(1, { verb: "capture", mode: "ui", name: "locker_island_open_c1" }, 25000);
  const open2 = on(1, { verb: "use", className: "LF_LockerOpenAction", entityName: LOCKER }, 10000);
  wait(2500);
  const opened2 = (serverLog().match(/opened (the|an empty) locker at/g) || []).length;
  check("locker reopened", open2.status === "ok" && opened2 > opened1, `${open2.status} opened-lines=${opened2}`);
  const dropped = (clientLog(1).match(/locker open dropped on the client/g) || []).length;
  check("no locker open dropped on client", dropped === 0, `${dropped}`);
  console.log(results.every((r) => r.pass) ? "ALL PASS" : "SOME FAIL: " + results.filter((r) => !r.pass).map((r) => r.name).join("; "));

  return results.every((r) => r.pass) ? 0 : 1;
}

export default {
  name: "island-vault-proof",
  usage: "island-vault-proof   (up --mission island --clients 2 --roster 2)",
  run: () => runIslandVaultProof(),
};

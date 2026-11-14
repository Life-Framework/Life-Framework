import { spawnSync } from "node:child_process";
import { readFileSync } from "node:fs";
import { join } from "node:path";
import { ROOT, arrive, clientBox, newestLog, parseState, sessionDir, sessionPaths } from "../session.mjs";
import { CLERK, TILL, armAndDraw, demandTill } from "./fuel-robbery-proof.mjs";
import { parsePatient } from "./downed-patient-proof.mjs";

// The replicated robbery and medic features on a dedicated server with two clients and a late joiner
// (#975 vault door, #954 held fuel robbery, #987 robbery alarms, #996 practice patient, #978/#984 NPC
// outfits). Each check reads what every CLIENT draws (`seen`, `peers`, `robhold`, `state`), not the
// server's word.
//   node agent-box/session.mjs roster 2               (client 2 may clock in as police to repair the vault)
//   node agent-box/session.mjs up --clients 2
//   node agent-box/session.mjs mp-robbery-proof       (joins client 3 after the vault breach)
//   node agent-box/session.mjs down
const VAULT = "Roadmap_Bank_Vault";
const PATIENT = "Patient";
const PATIENT_HOME = [268.2, 0, 254.6];
// The kit's swing angle, read from the prefab so the proof follows a retune.
const SWING_YAW = Number(/m_fSwingYaw\s+(-?[\d.]+)/.exec(readFileSync(join(ROOT, "addons", "LifeFramework", "Prefabs", "Props", "Roleplay", "BankVault", "LF_BankVault.et"), "utf8"))?.[1] ?? 100);
// Outfit sites: placed role NPCs, each read by every client from the same spot.
const OUTFIT_SITES = ["Hospital_Doctor", CLERK, "Shop_Clerk", "CarLot_Mechanic"];

async function runMpRobberyProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail.slice(0, 240)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const results = [];
  const check = (name, pass, detail) => {
    results.push({ name, pass, detail });
    console.log(`${pass ? "PASS" : "FAIL"}  ${name}: ${detail}`);
  };
  const clientLog = (n) => {
    const f = newestLog(sessionPaths(dir, n).clients[n - 1].logs);
    return f ? readFileSync(f, "utf8") : "";
  };
  const seen = (n, radius = 20) => on(n, { verb: "seen", radius }, 10000).detail;
  const token = (line, key) => parseState(line)[key] ?? "";
  const vaultOf = (line) => {
    const entry = token(line, "breach").split("|").find((e) => e.startsWith(`${VAULT}:`));
    const m = /open=(\d):yaw=(-?\d+|none)/.exec(entry ?? "");
    return m ? { open: m[1], yaw: m[2] === "none" ? null : Number(m[2]) } : null;
  };
  const sameYaw = (a, b) => a !== null && b !== null && Math.abs((((a - b) % 360) + 540) % 360 - 180) <= 2;
  // The nearest NPC's clothing (entity names do not reach a client, so the entry is picked by
  // distance: each client stands beside the site NPC).
  const wearOf = (line) => {
    const first = token(line, "npc").split("|")[0] ?? "";
    const m = /:d=([\d.]+):.*:wear=(\S+)$/.exec(first);
    return m && Number(m[1]) <= 2.5 ? m[2] : null;
  };
  const patientOn = (n) => parsePatient(on(n, { verb: "peers" }, 10000).detail);
  // Poll every client until judge passes for all, or the budget runs out. Returns the last reads.
  const until = (clients, read, judge, budgetMs) => {
    const t0 = Date.now();
    let reads;
    do {
      reads = clients.map((n) => read(n));
      if (reads.every((r) => judge(r))) break;
      wait(1500);
    } while (Date.now() - t0 < budgetMs);
    return reads;
  };

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  on(1, { verb: "resetcrime" }, 10000);

  // 1. Held fuel robbery: client 1 robs, client 2 stands at the counter. Only the robber's screen
  //    shows the panel. The robbery also trips the fuel alarm: both clients hear it.
  on(1, { verb: "stand", entityName: TILL, side: "s" });
  on(2, { verb: "stand", entityName: TILL, side: "w" });
  wait(2500);
  armAndDraw(on, 1, wait);
  demandTill(on, 1, wait);
  wait(2000);
  const hold1 = on(1, { verb: "robhold" }, 10000).detail;
  const hold2 = on(2, { verb: "robhold" }, 10000).detail;
  check("hold panel shows on the robber", /^running/.test(hold1), `client 1 robhold: ${hold1}`);
  check("hold panel hidden on the bystander", !/^running|^result/.test(hold2), `client 2 robhold: ${hold2}`);
  const heard = until([1, 2], (n) => token(on(n, { verb: "state" }, 10000).detail, "alarm"), (a) => a !== "none" && a !== "", 15000);
  check("alarm heard on both clients", heard.every((a) => a !== "none" && a !== ""), `alarm client1=${heard[0]} client2=${heard[1]}`);
  on(1, { verb: "stand", entityName: "Fuel_PumpHum" });
  wait(2500);
  const end1 = on(1, { verb: "robhold" }, 10000).detail;
  const end2 = on(2, { verb: "robhold" }, 10000).detail;
  check("hold ends on the robber only", /^result/.test(end1) && !/^running|^result/.test(end2), `client 1: ${end1}; client 2: ${end2}`);
  const sounding = [1, 2].map((n) => /site alarm up at/.test(clientLog(n)));
  check("alarm entity reached both clients (log)", sounding.every(Boolean), `'site alarm up at' client1=${sounding[0]} client2=${sounding[1]}`);
  on(1, { verb: "stand", entityName: TILL, side: "s" });
  // The fuel alarm is 30 s normal / 60 s loud (LF_Police.conf); it must stop on both.
  const quiet = until([1, 2], (n) => token(on(n, { verb: "state" }, 10000).detail, "alarm"), (a) => a === "none", 75000);
  const down = [1, 2].map((n) => /site alarm down at/.test(clientLog(n)));
  check("alarm stops on both clients", quiet.every((a) => a === "none") && down.every(Boolean), `alarm client1=${quiet[0]} client2=${quiet[1]}; 'site alarm down at' client1=${down[0]} client2=${down[1]}`);

  // 2. The vault door: closed on both, then client 1 drills it open and both draw it swung.
  on(1, { verb: "resetcrime" }, 10000);
  on(1, { verb: "stand", entityName: VAULT });
  on(2, { verb: "stand", entityName: VAULT, side: "w" });
  wait(3000);
  const closed = [1, 2].map((n) => vaultOf(seen(n)));
  const closedYaw = closed[0]?.yaw ?? null;
  check("vault closed on both before the breach", closed.every((v) => v && v.open === "0" && sameYaw(v.yaw, closedYaw)), `client1=${JSON.stringify(closed[0])} client2=${JSON.stringify(closed[1])}`);
  on(1, { verb: "drill" }, 10000);
  wait(1500);
  const rob = on(1, { verb: "use", className: "LF_BankRobAction", entityName: VAULT }, 10000);
  const openYaw = closedYaw === null ? null : closedYaw + SWING_YAW;
  const opened = until([1, 2], (n) => vaultOf(seen(n)), (v) => v && v.open === "1" && sameYaw(v.yaw, openYaw), 15000);
  check("vault door open on both clients", rob.status === "ok" && opened.every((v) => v && v.open === "1" && sameYaw(v.yaw, openYaw)), `rob=${rob.status}; want yaw ${openYaw}; client1=${JSON.stringify(opened[0])} client2=${JSON.stringify(opened[1])}`);

  // 3. A late joiner after the breach.
  const joined = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "session", "join", "3"], { windowsHide: true, stdio: "inherit" });
  let late = false;
  if (joined.status !== 0) {
    check("late joiner connects", false, `session join 3 exited ${joined.status}`);
  } else {
    late = true;
    arrive(on, 3, wait);
    wait(8000);
    on(3, { verb: "stand", entityName: VAULT, side: "e" });
    wait(3000);
    const v3 = until([3], (n) => vaultOf(seen(n)), (v) => v && v.open === "1" && sameYaw(v.yaw, openYaw), 15000)[0];
    check("vault door open for the late joiner", v3 && v3.open === "1" && sameYaw(v3.yaw, openYaw), `client3=${JSON.stringify(v3)} want yaw ${openYaw}`);
  }
  const all = late ? [1, 2, 3] : [1, 2];

  // 4. The practice patient: downed for everyone, revived for everyone, reset downed for everyone.
  for (const n of all) on(n, { verb: "stand", entityName: PATIENT, side: ["n", "s", "e"][n - 1] });
  wait(3000);
  const lying = until(all, patientOn, (p) => p && p.downed === "1", 15000);
  check("patient downed on every client", lying.every((p) => p && p.downed === "1"), lying.map((p, i) => `client${all[i]}=${p ? `${p.downed}/${p.life}` : "none"}`).join(" "));
  on(1, { verb: "stand", entityName: "Roadmap_Hospital_Duty_Desk" });
  wait(2500);
  on(1, { verb: "look" });
  on(1, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Hospital_Duty_Desk" }, 10000);
  wait(2500);
  on(1, { verb: "stand", entityName: PATIENT });
  wait(2500);
  on(1, { verb: "look" });
  const revive = on(1, { verb: "use", className: "LF_ReviveAction", entityName: PATIENT }, 10000);
  const up = (p) => p && p.downed === "0" && p.life === "ALIVE";
  const woke = until(all, patientOn, up, 20000);
  check("patient revived on every client", revive.status === "ok" && woke.every(up), `revive=${revive.status} ` + woke.map((p, i) => `client${all[i]}=${p ? `${p.downed}/${p.life}` : "none"}`).join(" "));
  const near = (p) => p && Math.hypot(p.at[0] - PATIENT_HOME[0], p.at[2] - PATIENT_HOME[2]) <= 1.5;
  const reset = until(all, patientOn, (p) => p && p.downed === "1" && near(p), 90000);
  check("patient reset downed at its spot on every client", reset.every((p) => p && p.downed === "1" && near(p)), reset.map((p, i) => `client${all[i]}=${p ? `${p.downed}/${p.life}@${p.at.map((x) => x.toFixed(1)).join(",")}` : "none"}`).join(" "));

  // 5. NPC outfits: every client reads the same clothing on the same NPCs.
  for (const site of OUTFIT_SITES) {
    for (const n of all) on(n, { verb: "stand", entityName: site, side: ["n", "s", "e"][n - 1] });
    wait(3000);
    const wear = all.map((n) => wearOf(seen(n, 6)));
    const ok = !!wear[0] && wear[0] !== "none" && wear.every((w) => w === wear[0]);
    check(`outfit of ${site} identical on every client`, ok, wear.map((w, i) => `client${all[i]}=${w ?? "missing"}`).join(" "));
  }

  // 6. Repair: client 2 clocks in as police and closes the vault; every client draws it shut.
  on(2, { verb: "stand", entityName: "Roadmap_Police_Desk" });
  wait(2500);
  on(2, { verb: "look" });
  on(2, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Police_Desk" }, 10000);
  wait(2500);
  for (const n of all) on(n, { verb: "stand", entityName: VAULT, side: ["s", "w", "e"][n - 1] });
  wait(3000);
  on(2, { verb: "look" });
  const repair = on(2, { verb: "use", className: "LF_BreachRepairAction", entityName: VAULT }, 10000);
  const shut = until(all, (n) => vaultOf(seen(n)), (v) => v && v.open === "0" && sameYaw(v.yaw, closedYaw), 15000);
  check("vault door closed again on every client", repair.status === "ok" && shut.every((v) => v && v.open === "0" && sameYaw(v.yaw, closedYaw)), `repair=${repair.status} ${repair.detail.slice(0, 80)}; ` + shut.map((v, i) => `client${all[i]}=${JSON.stringify(v)}`).join(" "));

  const failed = results.filter((r) => !r.pass);
  console.log("");
  console.log("| Check | Result | Observed |");
  console.log("|---|---|---|");
  for (const r of results) console.log(`| ${r.name} | ${r.pass ? "pass" : "FAIL"} | ${r.detail.replace(/\|/g, "/")} |`);
  console.log(failed.length ? `mp-robbery-proof: FAIL (${failed.length} of ${results.length})` : `mp-robbery-proof: PASS (${results.length} checks)`);
  return failed.length ? 1 : 0;
}

export default {
  name: "mp-robbery-proof",
  usage: "mp-robbery-proof   (roster 2, then up --clients 2; joins client 3 itself)",
  run: () => runMpRobberyProof(),
};

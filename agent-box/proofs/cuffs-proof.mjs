import { readFileSync } from "node:fs";
import { join } from "node:path";
import { arrive, clientBox, clientSees, newestLog, parseState, sessionDir, sessionPaths } from "../session.mjs";

// #664 handcuffs on a running `up --clients 2 --profile full` session, client 1 on the police roster
// (`session.mjs roster 1` before `up`): client 1 clocks in (the starter kit carries the cuffs),
// downs client 2 (harness `shoot`), takes the cuffs in hand (harness `hold`, what a quick-slot key
// does) and cuffs client 2 from the interaction menu. Both clients read client 2 cuffed, the cuffs
// have left the officer, and client 2's own client has weapon controls off (its own log). Then the
// officer uncuffs: both clients read client 2 free and the cuffs are back in the officer's hands.
// Shots: client 2's own view while cuffed, the cuffs on client 2's body (client 1's free camera),
// and last the cuffs in the officer's hand (client 2's free camera).
async function runCuffsProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const state = (n) => parseState(on(n, { verb: "state" }, 10000).detail);
  const serverLog = () => {
    const f = newestLog(join(sessionPaths(dir).server, "logs"));
    return f ? readFileSync(f, "utf8") : "";
  };
  const fails = [];
  const need = (cond, text) => { if (!cond) fails.push(text); };
  // Each client's own copy of player `id`'s cuffed flag, polled until it lands.
  const cuffedSeen = (want, label) => {
    for (const n of [1, 2]) {
      const r = clientSees(on, wait, n, (line) => {
        const m = new RegExp(`player=${victim} [^|]*cuffed=(\\d)`).exec(line);
        if (!m) return [`no cuffed= for player ${victim}: ${line}`];
        return m[1] === want ? [] : [`player ${victim} cuffed=${m[1]}, expected ${want}`];
      }, { verb: "peers", parse: (line) => line, label, budgetMs: 15000 });
      fails.push(...r.fails);
    }
  };

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);

  // Client 1 clocks in; a first shift gets the police kit with the handcuffs.
  on(1, { verb: "stand", entityName: "Roadmap_Police_Desk", side: "s" });
  wait(2500);
  on(1, { verb: "look" });
  const clockIn = on(1, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Police_Desk" }, 10000);
  need(clockIn.status === "ok", `clock-in returned ${clockIn.status} ${clockIn.detail}`);
  wait(4000);

  // Client 2 comes to the other side of the desk and is downed by client 1.
  on(2, { verb: "stand", entityName: "Roadmap_Police_Desk", side: "n" });
  wait(3000);
  const s2 = state(2);
  const victim = s2.player;
  const body = `Player${victim}`;
  const shot = on(1, { verb: "shoot", playerId: Number(victim) }, 10000);
  need(/downed=1/.test(shot.detail), `the hit returned "${shot.detail}", expected downed=1`);
  wait(2500);

  // Hold the cuffs and cuff the downed player.
  on(1, { verb: "stand", entityName: body });
  wait(2500);
  on(1, { verb: "look" });
  const held = on(1, { verb: "hold", item: "Handcuffs" }, 10000);
  need(held.status === "ok", `holding the cuffs returned ${held.status} ${held.detail} (the kit should carry them)`);
  wait(3000);
  const cuff = on(1, { verb: "use", className: "LF_RestrainAction", entityName: body }, 10000);
  need(cuff.status === "ok", `cuffing returned ${cuff.status} ${cuff.detail}`);
  wait(2000);
  need(state(2).cuffed === "1", "the server does not read client 2 cuffed");
  cuffedSeen("1", "after cuffing");
  need(/restrained \S+ by \S+ \(downed, cuffs .*Handcuffs\.et\)/.test(serverLog()), "the server log has no `restrained ... (downed, cuffs ...Handcuffs.et)` line");
  const client2Log = () => {
    const f = newestLog(sessionPaths(dir).clients[1].logs);
    return f ? readFileSync(f, "utf8") : "";
  };
  need(/restrained flag 1 reached the owner: weapon controls disabled 1/.test(client2Log()), "client 2's own log has no `restrained flag 1 reached the owner: weapon controls disabled 1` line");
  const again = on(1, { verb: "hold", item: "Handcuffs" }, 10000);
  need(again.status !== "ok", `the officer still carries cuffs after cuffing (${again.detail}); they should be on the suspect`);
  need(/worn handcuffs not shown/.test(client2Log()) === false, "client 2 logged `worn handcuffs not shown`");
  on(2, { verb: "capture", mode: "ui", name: "cuffs_2_cuffed_own_view" }, 25000);
  // The cuffs on the body, from client 1's free camera (its one free capture: it moves client 1's
  // body away, so client 1 stands at the body again after it).
  const lying = String(state(2).pos ?? "").split(",").map(Number);
  if (lying.every(Number.isFinite)) on(1, { verb: "capture", mode: "world", name: "cuffs_1_worn", x: lying[0], y: lying[1] + 0.2, z: lying[2], ox: 0.7, oy: -0.8, oz: 0.7, hour: 12, free: true }, 25000);
  on(1, { verb: "stand", entityName: body });
  wait(2500);

  // A second cuff is not offered on a cuffed player.
  const twice = on(1, { verb: "use", className: "LF_RestrainAction", entityName: body }, 10000);
  need(twice.status !== "ok", `a second cuff on a cuffed player returned ${twice.status}`);

  // Release: both read client 2 free, and the officer has the cuffs back.
  const release = on(1, { verb: "use", className: "LF_ReleaseRestraintAction", entityName: body }, 10000);
  need(release.status === "ok", `uncuffing returned ${release.status} ${release.detail}`);
  wait(2000);
  need(state(2).cuffed === "0", "the server still reads client 2 cuffed");
  cuffedSeen("0", "after release");
  need(/restraint off \S+ \(released\); cuffs back in the inventory of/.test(serverLog()), "the server log has no `restraint off ... (released); cuffs back in the inventory of` line");
  const back = on(1, { verb: "hold", item: "Handcuffs" }, 10000);
  need(back.status === "ok", `the officer has no cuffs after release: ${back.status} ${back.detail}`);
  wait(3000);

  // Last: the cuffs in the officer's hand, from in front of the officer (between them and the body
  // they face), by client 2's free camera: its one free capture, the last step, as in taser-proof.
  const at = String(state(1).pos ?? "").split(",").map(Number);
  const to = String(state(2).pos ?? "").split(",").map(Number);
  if (at.length === 3 && at.every(Number.isFinite) && to.every(Number.isFinite)) {
    const len = Math.hypot(to[0] - at[0], to[2] - at[2]) || 1;
    const fx = (to[0] - at[0]) / len, fz = (to[2] - at[2]) / len;
    on(2, { verb: "capture", mode: "world", name: "cuffs_3_held", x: at[0], y: at[1] + 1.0, z: at[2], ox: +(fx * 0.9 + fz * 0.3).toFixed(2), oy: -1.0, oz: +(fz * 0.9 - fx * 0.3).toFixed(2), hour: 12, free: true }, 25000);
  } else fails.push("no position for client 1 or 2; the held cuffs were not shot");

  // A cuffed player who dies (here the harness `die`; in play a bleed-out or give-up) gives the
  // cuffs back to the officer, and the hold does not outlive the body.
  on(1, { verb: "stand", entityName: body });
  wait(2500);
  const recuff = on(1, { verb: "use", className: "LF_RestrainAction", entityName: body }, 10000);
  need(recuff.status === "ok", `the second cuffing returned ${recuff.status} ${recuff.detail}`);
  wait(1500);
  on(2, { verb: "die" }, 10000);
  wait(3000);
  need(/restraint off \S+ \(died\); cuffs back in the inventory of/.test(serverLog()), "the server log has no `restraint off ... (died); cuffs back in the inventory of` line");
  const afterDeath = on(1, { verb: "hold", item: "Handcuffs" }, 10000);
  need(afterDeath.status === "ok", `the officer has no cuffs after the cuffed player died: ${afterDeath.status} ${afterDeath.detail}`);

  console.log("");
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "cuffs proof: FAIL" : "cuffs proof: PASS (officer clocked in with cuffs, downed player cuffed and seen cuffed by both clients, cuffs consumed, released and seen free by both, cuffs back with the officer)");
  return fails.length ? 1 : 0;
}

export default { name: "cuffs-proof", usage: "cuffs-proof", run: () => runCuffsProof() };

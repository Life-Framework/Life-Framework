import { readFileSync } from "node:fs";
import { join } from "node:path";
import { arrive, clientBox, clientSees, newestLog, parseState, sessionDir, sessionPaths } from "../session.mjs";

// #664 handcuffs on a running `up --clients 2 --profile full` session, client 1 on the police roster
// (`session.mjs roster 1` before `up`): client 1 clocks in (the starter kit carries the cuffs),
// downs client 2 (harness `shoot`), takes the cuffs in hand (harness `hold`, what a quick-slot key
// does) and cuffs client 2 the way the Cuff action does, as a hold (harness `cuffing`, what
// OnActionStart and the end of the hold send): a completion with no hold behind it is refused; the
// hold opens the cuffs in the officer's hand (both clients log them open; a picture of the open cuffs
// in the officer's view); the end of the hold shuts them with a click, and only then are they on the
// suspect. Both clients read client 2 cuffed, the cuffs have left the officer, and client 2's own
// client has weapon controls off (its own log). Then the officer uncuffs the same way: the cuffs on
// the wrist open (both clients log it; a picture from client 2's own view), then come off: both
// clients read client 2 free and the cuffs are back in the officer's hands.
// Shots: the open cuffs in the officer's own view, client 2's own view while cuffed, the cuffs on
// client 2's body (client 1's free camera), the open cuffs in the officer's hand from client 2's free
// camera (last, a pinned second hold).
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
  const clientLog = (n) => {
    const f = newestLog(sessionPaths(dir).clients[n - 1].logs);
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
  // A completion with no hold behind it (what a modified client would send): the server refuses it.
  on(1, { verb: "use", className: "LF_RestrainAction", entityName: body }, 10000);
  wait(1500);
  need(state(2).cuffed === "0", "a Cuff completion with no hold cuffed client 2: the server must see the hold begin");
  need(/cuffing completion refused: no matching hold was running/.test(serverLog()), "the server log has no `cuffing completion refused: no matching hold was running` line");

  // The hold: pinned, so the pictures can be taken while it runs. The cuffs open in the officer's hand.
  const begin = on(1, { verb: "cuffing", playerId: Number(victim), kind: "cuff", mode: "pin" }, 10000);
  need(begin.status === "ok", `the cuff hold returned ${begin.status} ${begin.detail}`);
  wait(2000);
  need(state(2).cuffed === "0", "client 2 is cuffed while the hold still runs");
  need(/cuffing hold began: \S+ CUFF/.test(serverLog()), "the server log has no `cuffing hold began: ... CUFF` line");
  need(/Handcuffs\.et active=1/.test(serverLog()), "the server did not switch the cuffs in hand open (`Handcuffs.et active=1`)");
  for (const n of [1, 2]) need(/Handcuffs\.et shows active=1 here/.test(clientLog(n)), `client ${n} does not show the cuffs in the officer's hand open`);
  on(1, { verb: "capture", mode: "ui", name: "cuffs_1_open_in_hand_own_view" }, 25000);

  // The end of the hold: the strands shut with a click, then the cuffs are on the wrists.
  const done = on(1, { verb: "cuffing", playerId: Number(victim), kind: "cuff", mode: "finish" }, 10000);
  need(done.status === "ok", `the end of the cuff hold returned ${done.status} ${done.detail}`);
  wait(2500);
  need(/cuffing hold done: the cuffs are closing/.test(serverLog()), "the server log has no `cuffing hold done: the cuffs are closing` line");
  need(/Handcuffs\.et active=0/.test(serverLog()), "the server did not shut the cuffs before they left the hand (`Handcuffs.et active=0`)");
  // Both clients ran the click: it plays where the machine has a sound manager and logs why not where it has none.
  for (const n of [1, 2]) need(/SOUND_LF_CLOSE (played at|not played)/.test(clientLog(n)), `client ${n} did not reach the close click (\`SOUND_LF_CLOSE played at ...\` or \`not played\`)`);
  // The engine resolves the click's project and events (what an agent can know without ears).
  const probe = on(1, { verb: "sound" }, 10000);
  need(/cuffs_close_audible=(1|true)/.test(probe.detail) && /cuffs_open_audible=(1|true)/.test(probe.detail), `the engine does not resolve the handcuffs click project: ${probe.detail}`);
  need(state(2).cuffed === "1", "the server does not read client 2 cuffed");
  cuffedSeen("1", "after cuffing");
  need(/restrained \S+ by \S+ \(downed, cuffs .*Handcuffs\.et\)/.test(serverLog()), "the server log has no `restrained ... (downed, cuffs ...Handcuffs.et)` line");
  const client2Log = () => clientLog(2);
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
  const releaseBegin = on(1, { verb: "cuffing", playerId: Number(victim), kind: "release", mode: "pin" }, 10000);
  need(releaseBegin.status === "ok", `the uncuff hold returned ${releaseBegin.status} ${releaseBegin.detail}`);
  wait(2000);
  need(state(2).cuffed === "1", "client 2 is free while the uncuff hold still runs");
  for (const n of [1, 2]) need(/worn handcuffs shown open=(1|true)/.test(clientLog(n)), `client ${n} does not show the cuffs on the wrist open during the uncuff hold`);
  const release = on(1, { verb: "cuffing", playerId: Number(victim), kind: "release", mode: "finish" }, 10000);
  need(release.status === "ok", `the end of the uncuff hold returned ${release.status} ${release.detail}`);
  wait(2500);
  need(state(2).cuffed === "0", "the server still reads client 2 cuffed");
  cuffedSeen("0", "after release");
  need(/restraint off \S+ \(released\); cuffs back in the inventory of/.test(serverLog()), "the server log has no `restraint off ... (released); cuffs back in the inventory of` line");
  const back = on(1, { verb: "hold", item: "Handcuffs" }, 10000);
  need(back.status === "ok", `the officer has no cuffs after release: ${back.status} ${back.detail}`);
  wait(3000);

  // Last: the cuffs in the officer's hand, from in front of the officer (between them and the body
  // they face), by client 2's free camera: its one free capture, the last step, as in taser-proof.
  const again2 = on(1, { verb: "cuffing", playerId: Number(victim), kind: "cuff", mode: "pin" }, 10000);
  need(again2.status === "ok", `the second cuff hold returned ${again2.status} ${again2.detail}`);
  wait(2500);
  const at = String(state(1).pos ?? "").split(",").map(Number);
  const to = String(state(2).pos ?? "").split(",").map(Number);
  if (at.length === 3 && at.every(Number.isFinite) && to.every(Number.isFinite)) {
    const len = Math.hypot(to[0] - at[0], to[2] - at[2]) || 1;
    const fx = (to[0] - at[0]) / len, fz = (to[2] - at[2]) / len;
    on(2, { verb: "capture", mode: "world", name: "cuffs_3_held", x: at[0], y: at[1] + 1.0, z: at[2], ox: +(fx * 0.55 + fz * 0.2).toFixed(2), oy: -0.6, oz: +(fz * 0.55 - fx * 0.2).toFixed(2), hour: 12, free: true }, 25000);
  } else fails.push("no position for client 1 or 2; the held cuffs were not shot");

  const letGo = on(1, { verb: "cuffing", playerId: Number(victim), kind: "cuff", mode: "cancel" }, 10000);
  need(letGo.status === "ok", `letting go of the second hold returned ${letGo.status} ${letGo.detail}`);
  wait(1500);
  // Client 2's free camera (the shot above) carries its body away from the officer: the server's watch
  // stops the hold on reach and shuts the cuffs, before the cancel above even arrives.
  need(/cuffing hold stopped: (STOP_REACH|the hold was let go)/.test(serverLog().slice(serverLog().lastIndexOf("cuffing hold began"))), "the server log has no `cuffing hold stopped: STOP_REACH` (or `the hold was let go`) line for the second hold");
  need(state(2).cuffed === "0", "letting go of the hold cuffed client 2");

  // A cuffed player who dies (here the harness `die`; in play a bleed-out or give-up) gives the
  // cuffs back to the officer, and the hold does not outlive the body.
  on(1, { verb: "stand", entityName: body });
  wait(2500);
  on(1, { verb: "cuffing", playerId: Number(victim), kind: "cuff", mode: "pin" }, 10000);
  wait(1500);
  const recuff = on(1, { verb: "cuffing", playerId: Number(victim), kind: "cuff", mode: "finish" }, 10000);
  need(recuff.status === "ok", `the second cuffing returned ${recuff.status} ${recuff.detail}`);
  wait(2500);
  need(state(2).cuffed === "1", "the second cuffing did not cuff client 2");
  on(2, { verb: "die" }, 10000);
  wait(3000);
  need(/restraint off \S+ \(died\); cuffs back in the inventory of/.test(serverLog()), "the server log has no `restraint off ... (died); cuffs back in the inventory of` line");
  const afterDeath = on(1, { verb: "hold", item: "Handcuffs" }, 10000);
  need(afterDeath.status === "ok", `the officer has no cuffs after the cuffed player died: ${afterDeath.status} ${afterDeath.detail}`);

  console.log("");
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "cuffs proof: FAIL" : "cuffs proof: PASS (officer clocked in with cuffs; a completion with no hold refused; the hold opened the cuffs in hand on both clients, the end shut them with a click and cuffed the downed player; the uncuff hold opened them on the wrist on both clients, then released; cuffs back with the officer)");
  return fails.length ? 1 : 0;
}

export default { name: "cuffs-proof", usage: "cuffs-proof", run: () => runCuffsProof() };

import { readFileSync } from "node:fs";
import { join } from "node:path";
import { spawnSync } from "node:child_process";
import { arrive, clientBox, newestLog, parseState, readSession, sessionDir, sessionPaths } from "../session.mjs";

// #691 disconnect rules on a running local `up --clients 2 --profile full --machine local` session,
// client 1 on the police roster (`session.mjs roster 1` before `up`). Client 1 clocks in, downs
// client 2 and cuffs them (the cuffing hold). Client 2's game is then closed (a logout), so the
// server applies the rules to a cuffed and downed player: the custody rule jails them with one more
// wanted level, the downed rule kills them, the cuffs go back to the officer, and one incident row
// is written. Client 2 then joins again (`session join 2`) under a new session identity.
async function runDisconnectProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const root = process.cwd();
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

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);

  // Client 1 clocks in with the starter police kit (it carries the handcuffs).
  on(1, { verb: "stand", entityName: "Roadmap_Police_Desk", side: "s" });
  wait(2500);
  on(1, { verb: "look" });
  const clockIn = on(1, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Police_Desk" }, 10000);
  need(clockIn.status === "ok", `clock-in returned ${clockIn.status} ${clockIn.detail}`);
  wait(4000);

  // Client 2 is downed and cuffed.
  on(2, { verb: "stand", entityName: "Roadmap_Police_Desk", side: "n" });
  wait(3000);
  const before = state(2);
  const victim = before.player;
  const wantedBefore = Number(before.wanted ?? 0);
  const shot = on(1, { verb: "shoot", playerId: Number(victim) }, 10000);
  need(/downed=1/.test(shot.detail), `the hit returned "${shot.detail}", expected downed=1`);
  wait(2500);
  on(1, { verb: "stand", entityName: `Player${victim}` });
  wait(2500);
  on(1, { verb: "look" });
  need(on(1, { verb: "hold", item: "Handcuffs" }, 10000).status === "ok", "client 1 could not hold the cuffs");
  wait(2500);
  on(1, { verb: "cuffing", playerId: Number(victim), kind: "cuff", mode: "pin" }, 10000);
  wait(2500);
  on(1, { verb: "cuffing", playerId: Number(victim), kind: "cuff", mode: "finish" }, 10000);
  wait(2500);
  need(state(2).cuffed === "1", "client 2 is not cuffed before the logout");

  // The logout: client 2's game closes.
  const s = readSession(dir);
  const c2 = (s?.clients ?? []).find((c) => c.n === 2);
  need(!!c2, "no client 2 in the session record");
  if (c2) {
    try { process.kill(c2.pid); } catch (e) { fails.push(`could not close client 2 (pid ${c2.pid}): ${e.message}`); }
  }
  wait(15000);
  const afterLeave = serverLog();
  need(/\[LFDebug:Disconnect\] \S+ left cuffed[^:]*downed[^:]*: jail min=\d+s, wanted\+1/.test(afterLeave), "no `[LFDebug:Disconnect] <uid> left cuffed,...downed...: jail min=..., wanted+1` line");
  need(/\[LFDebug:Disconnect\] \S+ jailed for leaving custody: \d+ s/.test(afterLeave), "no `jailed for leaving custody` line");
  need(/sentence started on logout/.test(afterLeave), "no `sentence started on logout` line");
  need(/disconnected while downed: death/.test(afterLeave), "no `disconnected while downed: death` line (downedDies)");
  need(/restraint off \S+ \((died|disconnected)\); cuffs back in the inventory of/.test(afterLeave), "the cuffs did not go back to the officer");
  need(on(1, { verb: "hold", item: "Handcuffs" }, 10000).status === "ok", "the officer has no cuffs after client 2 left");

  // The rejoin.
  const joined = spawnSync(process.execPath, [join(root, "tools", "cli.mjs"), "session", "join", "2"], { windowsHide: true, stdio: "inherit", cwd: root, env: { ...process.env, LF_SESSION_ROUTED: "1" } });
  need(joined.status === 0, `session join 2 exited ${joined.status}`);
  arrive(on, 2, wait);
  wait(10000);
  // A harness session keys a client by its player id for one server run (LF_Utils.BuildSessionIdentity),
  // so the rejoined client is a NEW player, not the one who left: the rejoin into the cell stays a
  // manual step (two real Steam accounts). What this checks: the new player is not caught by the
  // leaver's sentence, and the server kept the leaver's sentence (the jail tick pauses it offline).
  const back = state(2);
  const log = serverLog();
  need(back.cuffed === "0", "the rejoined client is cuffed");
  const leaver = (/\[LFDebug:Disconnect\] (\S+) left cuffed/.exec(log) ?? [])[1];
  need(!!leaver, "no uid for the player who left cuffed");
  if (leaver) {
    need(log.includes(`${leaver} wanted ${wantedBefore} -> ${Math.min(wantedBefore + 1, 5)} for logging out`), `no \`${leaver} wanted ${wantedBefore} -> ${wantedBefore + 1} for logging out\` line`);
    need(log.includes(`sentence paused ${leaver} (offline)`), `the leaver's sentence is not kept paused offline (\`sentence paused ${leaver} (offline)\`)`);
    need(log.includes(`logout: ${leaver}`), `no incident row of kind logout for ${leaver} (\`[LFDebug:Incident] #N logout: ${leaver} ...\`)`);
  }
  need(!/spawned prisoner \S+ returned to the cell/.test(log), "a player was sent to the cell on a spawn: only the leaver's own rejoin may be");
  console.log("");
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "disconnect proof: FAIL" : "disconnect proof: PASS (cuffed and downed client 2 closed its game: jailed with wanted+1, died, cuffs back with the officer; the sentence waits offline; a rejoin from a new identity is free)");
  return fails.length ? 1 : 0;
}

export default { name: "disconnect-proof", usage: "disconnect-proof", run: () => runDisconnectProof() };

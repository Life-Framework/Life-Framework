import { readFileSync, writeFileSync, mkdirSync } from "node:fs";
import { dirname, join } from "node:path";
import { spawnSync } from "node:child_process";
import { arrive, clientBox, newestLog, parseState, readSession, sessionDir, sessionPaths } from "../session.mjs";

const DESK = "Fit_saint_philippe_clinic_lf_medic_duty_desk";

// The downed HUD line after a rejoin into the player's own downed logout body (the #5162 gap of the
// pre-RC review). Two clients, one session:
//   node agent-box/session.mjs up --mission island --clients 2 --profile full --hold 20
//   node agent-box/session.mjs downed-rejoin-proof
//   node agent-box/session.mjs pull <dir>      (pictures; then `down`)
// The owner config turns disconnect.downedDies off, so a downed logout leaves a body instead of a
// death. Client 1 downs client 2 (a harness `shoot`); client 2 shows the line, closes its game, and
// joins again inside the window. The taken-back body is still downed, with no flag change to wake the
// old callback: the line must come back (and its give-up wait is the replicated seconds downed, so a
// rejoin well past the delay reads the "hold X" text, not a fresh wait).
// The listen host (the host's own character gets no replication callback) has no session mode here:
// it stays a manual step in the PR.
const ROOT = process.cwd();

async function runDownedRejoinProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail.slice(0, 400)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const serverLog = () => {
    const f = newestLog(join(sessionPaths(dir).server, "logs"));
    return f ? readFileSync(f, "utf8") : "";
  };
  const fails = [];
  const need = (cond, text) => { if (!cond) fails.push(text); };
  const hud = (n) => /downedhud=(\S+)/.exec(on(n, { verb: "seen" }, 10000).detail)?.[1] ?? "";
  const shot = (n, name) => on(n, { verb: "capture", mode: "ui", name }, 25000);
  const bleedOf = (line) => {
    const m = /Bleeding_out:_(\d+):(\d+)/.exec(line);
    return m ? Number(m[1]) * 60 + Number(m[2]) : -1;
  };

  // A downed logout must leave a body, not a death.
  const ownerFile = join(sessionPaths(dir).server, "profile", "LifeFramework", "config.json");
  const owner = JSON.parse(readFileSync(join(ROOT, "server", "flagship", "config.json"), "utf8"));
  owner.disconnect = { ...(owner.disconnect ?? {}), downedDies: false };
  mkdirSync(dirname(ownerFile), { recursive: true });
  writeFileSync(ownerFile, JSON.stringify(owner, null, 2));

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  on(1, { verb: "configreload" }, 10000);
  wait(3500);

  // The spawn square refuses damage; the clinic desk is outside it (as in downed-flow-proof).
  on(1, { verb: "stand", entityName: DESK });
  on(2, { verb: "stand", entityName: DESK });
  wait(2500);
  const victim = parseState(on(2, { verb: "state" }, 10000).detail).player;
  const shot1 = on(1, { verb: "shoot", playerId: Number(victim) }, 10000);
  need(/downed=1/.test(shot1.detail), `the harness shot did not down client 2: ${shot1.detail}`);
  wait(3000);
  const before = hud(2);
  shot(2, "downed_hud_before_logout");
  need(/Bleeding_out/.test(before), `client 2 HUD line before the logout: "${before}"`);
  wait(9000);

  const c2 = (readSession(dir)?.clients ?? []).find((c) => c.n === 2);
  need(!!c2, "no client 2 in the session record");
  if (c2) {
    try { process.kill(c2.pid); } catch (e) { fails.push(`could not close client 2 (pid ${c2.pid}): ${e.message}`); }
  }
  wait(15000);
  const afterLeave = serverLog();
  need(/\[LFDebug:LogoutBody\] \S+ logged out: body stays/.test(afterLeave), "no `logged out: body stays` line: the downed logout died instead of leaving a body (is downedDies off?)");
  need(!/disconnected while downed: death/.test(afterLeave), "the downed logout was a death (downedDies is still on)");

  const joined = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "session", "join", "2"], { windowsHide: true, stdio: "inherit", cwd: ROOT, env: { ...process.env, LF_SESSION_ROUTED: "1" } });
  need(joined.status === 0, `session join 2 exited ${joined.status}`);
  arrive(on, 2, wait);
  wait(6000);
  const log = serverLog();
  need(/took back their logout body/.test(log), "the rejoin did not take the body back");

  // The line, from the replicated state alone: no flag change happened on this client.
  let after = hud(2);
  for (let i = 0; i < 5 && !/Bleeding_out/.test(after); i++) {
    wait(1500);
    after = hud(2);
  }
  shot(2, "downed_hud_after_rejoin");
  need(/Bleeding_out/.test(after), `client 2 HUD line after the rejoin: "${after}"`);
  const b0 = bleedOf(before);
  const b1 = bleedOf(after);
  need(b1 >= 0 && b0 >= 0 && b1 < b0, `the bleed clock did not run on across the rejoin (before ${b0}s, after ${b1}s)`);
  // The body has been down well past the give-up delay by now: the ready text, not a fresh wait.
  need(/Hold_X_to_give_up/.test(after), `the give-up wait restarted on the rejoin (expected the ready line): "${after}"`);

  console.log("");
  console.log(`hud before logout: ${before}`);
  console.log(`hud after rejoin:  ${after}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "downed rejoin proof: FAIL" : "downed rejoin proof: PASS (the downed line is back after the rejoin, the clocks ran on)");
  return fails.length ? 1 : 0;
}

export default {
  name: "downed-rejoin-proof",
  usage: "downed-rejoin-proof   (up --mission island --clients 2 --profile full --hold 20)",
  run: () => runDownedRejoinProof(),
};

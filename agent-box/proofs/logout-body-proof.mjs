import { readFileSync } from "node:fs";
import { join } from "node:path";
import { spawnSync } from "node:child_process";
import { arrive, clientBox, newestLog, readSession, sessionDir, sessionPaths } from "../session.mjs";

// #1973 logout body on a running `session up --clients 2`: client 2 closes its game (a logout), so the
// server keeps its character as a knocked-out body tagged Logged out instead of deleting it. Client 2
// then joins again as the same identity; the body leaves the world on the rejoin.
async function runLogoutBodyProof() {
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
  const serverLog = () => {
    const f = newestLog(join(sessionPaths(dir).server, "logs"));
    return f ? readFileSync(f, "utf8") : "";
  };
  const fails = [];
  const need = (cond, text) => { if (!cond) fails.push(text); };
  const show = (log, re) => console.log(log.split("\n").filter((l) => re.test(l)).slice(-12).join("\n"));

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);

  const c2 = (readSession(dir)?.clients ?? []).find((c) => c.n === 2);
  need(!!c2, "no client 2 in the session record");
  if (c2) {
    try { process.kill(c2.pid); } catch (e) { fails.push(`could not close client 2 (pid ${c2.pid}): ${e.message}`); }
  }
  wait(15000);
  const afterLeave = serverLog();
  show(afterLeave, /LogoutBody|logout: player|VME|SCRIPT\s+\(E\)/);
  const leaver = (/\[LFDebug:LogoutBody\] (\S+) logged out: body stays until/.exec(afterLeave) ?? [])[1];
  need(!!leaver, "no `[LFDebug:LogoutBody] <uid> logged out: body stays` line");
  need(/logout: player \S+ is knocked out/.test(afterLeave), "the body was not laid down (no `logout: player ... is knocked out` line)");
  need(!/could not take the character/.test(afterLeave), "the character could not be taken from the leaving player");

  const joined = spawnSync(process.execPath, [join(root, "tools", "cli.mjs"), "session", "join", "2"], { windowsHide: true, stdio: "inherit", cwd: root, env: { ...process.env, LF_SESSION_ROUTED: "1" } });
  need(joined.status === 0, `session join 2 exited ${joined.status}`);
  arrive(on, 2, wait);
  wait(8000);
  const log = serverLog();
  show(log, /LogoutBody|VME|SCRIPT\s+\(E\)/);
  if (leaver) need(log.includes(`logout body of ${leaver} despawned (rejoined)`), `the body of ${leaver} did not despawn on the rejoin`);
  need(!/VME|\[LFDebug:LogoutBody\][^\n]*(could not|refused)/.test(log), "a VME or a LogoutBody refusal in the server log");
  console.log("");
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "logout body proof: FAIL" : "logout body proof: PASS (client 2 closed its game: the character stayed as a laid-down Logged out body; the rejoin removed it)");
  return fails.length ? 1 : 0;
}

export default { name: "logout-body-proof", usage: "logout-body-proof", run: () => runLogoutBodyProof() };

import { join } from "node:path";
import { spawnSync } from "node:child_process";
import { arrive, clientBox, readSession, sessionDir } from "../session.mjs";

// #1974 on a running `session up --clients 2 --profile full`: client 2 closes its game, the server keeps
// its character as a "Logged out" body, and client 1 must track and draw the tag (harness verb
// `logoutbodytag`). Client 1 then closes and joins again as the late joiner of that body, and client 2's
// rejoin removes the body and the tag.
async function runLogoutBodyTagProof() {
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
  const fails = [];
  const need = (cond, text) => { if (!cond) fails.push(text); };

  const tag = (n) => on(n, { verb: "logoutbodytag", words: [] }, 20000);
  const closeClient = (n) => {
    const c = (readSession(dir)?.clients ?? []).find((x) => x.n === n);
    need(!!c, `no client ${n} in the session record`);
    if (c) {
      try { process.kill(c.pid); } catch (e) { fails.push(`could not close client ${n} (pid ${c.pid}): ${e.message}`); }
    }
  };
  const rejoin = (n) => {
    const r = spawnSync(process.execPath, [join(root, "tools", "cli.mjs"), "session", "join", String(n)], { windowsHide: true, stdio: "inherit", cwd: root, env: { ...process.env, LF_SESSION_ROUTED: "1" } });
    need(r.status === 0, `session join ${n} exited ${r.status}`);
    arrive(on, n, wait);
    wait(8000);
  };

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(6000);
  const before = tag(1);
  need(/tracked 0 shown 0/.test(before.detail), `before the logout client 1 should track nothing, got: ${before.detail}`);

  // 1. Replication to a client that was there: client 2 logs out, client 1 draws the tag.
  closeClient(2);
  wait(15000);
  const live = tag(1);
  need(/tracked 1/.test(live.detail), `client 1 should track the logged-out body, got: ${live.detail}`);
  on(1, { verb: "capture", mode: "ui", name: "logout_tag_client1" }, 25000);

  // 2. The late joiner: client 1 closes and joins again while client 2's body is still there, so the
  // flag reaches it only with the entity snapshot.
  closeClient(1);
  wait(8000);
  rejoin(1);
  wait(4000);
  const late = tag(1);
  need(/tracked [1-9]/.test(late.detail), `the late joiner (client 1 rejoined) should track the body from the snapshot, got: ${late.detail}`);
  on(1, { verb: "capture", mode: "ui", name: "logout_tag_late_joiner" }, 25000);

  // 3. The rejoin of client 2 removes the body, and the tag with it.
  rejoin(2);
  const after = tag(1);
  need(/tracked 0/.test(after.detail), `after the rejoin client 1 should track nothing, got: ${after.detail}`);
  console.log("");
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "logout body tag proof: FAIL" : "logout body tag proof: PASS (client 1 tracked and drew the tag on the logged-out body; the rejoin cleared it)");
  return fails.length ? 1 : 0;
}

export default { name: "logout-body-tag-proof", usage: "logout-body-tag-proof", run: () => runLogoutBodyTagProof() };

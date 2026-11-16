import { join } from "node:path";
import { spawnSync } from "node:child_process";
import { arrive, clientBox, parseState, readSession, sessionDir } from "../session.mjs";

// #1974 on a running `session up --clients 2 --profile full`: client 2 closes its game, the server keeps
// its character as a "Logged out" body, and client 1 must track and draw the tag (harness verb
// `logoutbodytag`). The observing camera is aimed at the body (a world capture whose target is the spot
// client 2 stood on), so `shown` counts a label on screen, not only a tracked body. Client 1 then closes and joins
// again as the late joiner of that body (tracked and drawn from the entity snapshot), and client 2's rejoin
// removes the body and the tag.
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
  // Put client n's camera 4 m from the body and take a world capture facing it; retry the read until the label is up.
  const aimAtBody = (n, spot, name) => {
    const shot = on(n, { verb: "capture", mode: "world", name, x: spot[0], y: spot[1], z: spot[2], ox: 0, oy: 1.5, oz: 4 }, 25000);
    let read = tag(n);
    for (let i = 0; i < 5 && !/shown [1-9]/.test(read.detail); i++) {
      wait(1000);
      read = tag(n);
    }
    return { shot, read };
  };
  const closeClient = (n) => {
    const c = (readSession(dir)?.clients ?? []).find((x) => x.n === n);
    need(!!c, `no client ${n} in the session record`);
    if (c) {
      try { process.kill(c.pid); } catch (e) { fails.push(`could not close client ${n} (pid ${c.pid}): ${e.message}`); }
    }
  };
  const rejoin = (n) => {
    const r = spawnSync(process.execPath, [join(root, "tools", "cli.mjs"), "session", "join", String(n)], { windowsHide: true, stdio: "inherit", cwd: root, env: { ...process.env, LF_SESSION_ROUTED: "1" } });
    // `session join` judges the whole session record and exits 1 while another client of it is closed (the
    // one this proof closed on purpose), even when the client it launched joins; the checks below read that client.
    if (r.status !== 0) console.log(`note: session join ${n} exited ${r.status} (a closed client is in the session record)`);
    // The relaunched client answers `no_session` until it has joined; poll up to ~2 minutes.
    for (let i = 0; i < 12 && on(n, { verb: "ui" }).status !== "ok"; i++) wait(8000);
    arrive(on, n, wait);
    wait(8000);
  };

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(6000);
  const bodySpot = (parseState(on(2, { verb: "state" }, 10000).detail).pos ?? "").split(",").map(Number);
  need(bodySpot.length === 3 && bodySpot.every(Number.isFinite), "could not read client 2's position before it logged out");
  const before = tag(1);
  need(/tracked 0 shown 0/.test(before.detail), `before the logout client 1 should track nothing, got: ${before.detail}`);

  // 1. Replication to a client that was there: client 2 logs out, client 1 draws the tag.
  closeClient(2);
  wait(15000);
  const live = tag(1);
  need(/tracked 1/.test(live.detail), `client 1 should track the logged-out body, got: ${live.detail}`);
  const liveShot = aimAtBody(1, bodySpot, "logout_tag_client1");
  need(/shown [1-9]/.test(liveShot.read.detail), `client 1 faced the body but drew no tag, got: ${liveShot.read.detail}`);

  // 2. The late joiner: client 1 closes and joins again while client 2's body is still there, so the flag
  // reaches it only with the entity snapshot. It spawns beside the body and must have the label up at once.
  // (A rejoin takes the lowest free session slot, so it is client 1's own account and it takes back its own
  // body, which still lies prone and looks at the sky: aiming it with a world capture does not work, so the
  // picture of this step is whatever its camera shows, and the picture of the tag is the live one above.)
  closeClient(1);
  wait(8000);
  rejoin(1);
  wait(2000);
  const late = tag(1);
  need(/tracked [1-9]/.test(late.detail), `the late joiner (client 1 rejoined) should track the body from the snapshot, got: ${late.detail}`);
  need(/shown [1-9]/.test(late.detail), `the late joiner should have the tag up on the body it spawned beside, got: ${late.detail}`);
  const lateShot = on(1, { verb: "capture", mode: "ui", name: "logout_tag_late_joiner" }, 25000);
  for (const [label, shot] of [["live", liveShot.shot], ["late joiner", lateShot]]) console.log(`image (${label}): ${h.resolveCapture(shot.detail) || "not found: " + shot.detail}`);

  // 3. The rejoin of client 2 removes the body, and the tag with it.
  rejoin(2);
  const after = tag(1);
  need(/tracked 0/.test(after.detail), `after the rejoin client 1 should track nothing, got: ${after.detail}`);
  console.log("");
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "logout body tag proof: FAIL" : "logout body tag proof: PASS (client 1 drew the tag on the logged-out body live and again as a late joiner; the rejoin cleared it)");
  return fails.length ? 1 : 0;
}

export default { name: "logout-body-tag-proof", usage: "logout-body-tag-proof", run: () => runLogoutBodyTagProof() };

import { arrive, clientBox, sessionDir } from "../session.mjs";

// #2686 on a running `session up --profile full`: the client half of the capture bar. Harness verb
// `sitebar` (LF_SiteCaptureHarness) draws the bar the way the server's owner RPC does, at 0%, then 45%,
// and raises the contest alert toast, and a capture of each state is taken. The server push (players in
// the radius, one message per change) is covered by the PR's manual steps, which need two organisations.
async function runSiteCaptureBarProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const need = (cond, text) => { if (!cond) fails.push(text); };

  arrive(on, 1, wait);
  wait(3000);
  const bar = (...words) => on(1, { verb: "sitebar", words }, 20000);
  const hidden = bar("state");
  need(/^hidden/.test(hidden.detail), `the bar should start hidden, got: ${hidden.detail}`);

  const idle = bar("show", "quarry_hideout", "0");
  need(/^shown .*percent=0/.test(idle.detail), `the idle bar should show at 0%, got: ${idle.detail}`);
  wait(800);
  const idleShot = on(1, { verb: "capture", mode: "ui", name: "site_bar_idle" }, 25000);

  const active = bar("show", "quarry_hideout", "45");
  need(/percent=45/.test(active.detail), `the bar should read 45%, got: ${active.detail}`);
  bar("alert");
  wait(1200);
  const activeShot = on(1, { verb: "capture", mode: "ui", name: "site_bar_contesting" }, 25000);
  for (const [label, shot] of [["idle", idleShot], ["contesting with alert", activeShot]]) console.log(`image (${label}): ${h.resolveCapture(shot.detail) || "not found: " + shot.detail}`);

  const gone = bar("hide");
  need(/^hidden/.test(gone.detail), `the bar should hide, got: ${gone.detail}`);
  console.log("");
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "site capture bar proof: FAIL" : "site capture bar proof: PASS (the bar drew at 0% and 45%, the alert toast showed, the bar hid)");
  return fails.length ? 1 : 0;
}

export default { name: "site-capture-bar-proof", usage: "site-capture-bar-proof", run: () => runSiteCaptureBarProof() };

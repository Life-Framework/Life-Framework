import { arrive, clientBox, sessionDir } from "../session.mjs";

// #1924 the race panel on a running `session up --profile full --clients 2`: each client gets the
// panel as the owner RPC would feed it (client 1 leads on lap 2, client 2 is second on lap 1), the
// clock runs, and one UI picture per client. The server side (who is sent what, and when) is
// LF_Test_RaceHud's place order plus the signature; a driven race needs placed checkpoints.
async function runRaceHudProof() {
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

  arrive(on, 1, wait);
  arrive(on, 2, wait);
  wait(4000);

  const panels = [
    { n: 1, words: ["show", "2/3", "4/5", "1/2", "83400"], want: /shown 2\/3 4\/5 1\/2 0 .*running=1/ },
    { n: 2, words: ["show", "1/3", "5/5", "2/2", "83400"], want: /shown 1\/3 5\/5 2\/2 0 .*running=1/ },
  ];
  for (const p of panels) {
    const r = on(p.n, { verb: "racehud", words: p.words }, 10000);
    if (r.status !== "ok" || !p.want.test(r.detail)) fails.push(`client ${p.n}: expected ${p.want}, got ${r.status} ${r.detail}`);
  }
  wait(2500);
  for (const p of panels) on(p.n, { verb: "capture", mode: "ui", name: `racehud_client${p.n}` }, 25000);

  const end = on(1, { verb: "racehud", words: ["end"] }, 10000);
  if (!/running=(0|false)/.test(end.detail)) fails.push(`end: the clock should stop, got ${end.detail}`);
  wait(1000);
  on(1, { verb: "capture", mode: "ui", name: "racehud_over" }, 25000);

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "racehud proof: FAIL" : "racehud proof: PASS (two panels, clock runs and stops)");
  return fails.length ? 1 : 0;
}

export default { name: "racehud-proof", usage: "racehud-proof", run: () => runRaceHudProof() };

import { GIVE_AMOUNT, arriveCivilians, clientBox, judgeInteraction, parseState, sessionDir } from "../session.mjs";

async function runInteraction() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const state = (n) => on(n, { verb: "state" }, 10000).detail;

  // Registration form R-89 (M1-08): each player signs a name (any faction screen first).
  arriveCivilians(on, wait);

  const before1 = state(1);
  const before2 = state(2);
  const other = `Player${parseState(before2).player}`;
  on(1, { verb: "stand", entityName: other });
  wait(2500);
  on(1, { verb: "look" });
  const steps = [];
  steps.push(on(1, { verb: "use", className: "LF_IntroduceAction", entityName: other }, 10000));
  wait(2500);
  steps.push(on(1, { verb: "use", className: "LF_ShowIDAction", entityName: other }, 10000));
  wait(2000);
  steps.push(on(1, { verb: "use", className: "LF_GiveCashAction", entityName: other }, 10000));
  wait(2000);
  steps.push(on(1, { verb: "type", widget: "GiveCashAmount", text: String(GIVE_AMOUNT) }));
  steps.push(on(1, { verb: "click", widget: "GiveCashButton" }));
  wait(3000);
  const after1 = state(1);
  const after2 = state(2);

  console.log("");
  console.log(`before  client 1: ${before1}`);
  console.log(`before  client 2: ${before2}`);
  console.log(`after   client 1: ${after1}`);
  console.log(`after   client 2: ${after2}`);
  const fails = judgeInteraction(before1, before2, after1, after2);
  for (const st of steps) if (st.status !== "ok") fails.unshift(`${st.line} returned ${st.status} ${st.detail}`);
  for (const x of fails) console.log(`FAIL  ${x}`);
  console.log(fails.length ? "interaction: FAIL" : `interaction: PASS (introduced, showed ID, handed over ${GIVE_AMOUNT} cash)`);
  return fails.length ? 1 : 0;
}

export default {
  name: "interaction",
  usage: "interaction",
  film: true,
  run: (ctx) => runInteraction(),
};

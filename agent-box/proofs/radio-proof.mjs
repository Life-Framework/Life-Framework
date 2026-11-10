import { arrive, clientBox, judgeRadio, sessionDir } from "../session.mjs";

async function runRadioProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  on(2, { verb: "stand", entityName: "Roadmap_Hospital_Duty_Desk" });
  wait(2500);
  on(2, { verb: "look" });
  const clockIn = on(2, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Hospital_Duty_Desk" }, 10000);
  wait(3500);
  const onDuty = on(2, { verb: "radio" }, 10000).detail;
  const civilian = on(1, { verb: "radio" }, 10000).detail;
  on(2, { verb: "capture", mode: "ui", name: "m2_252_radio_client2" }, 25000);
  const fails = judgeRadio(onDuty, civilian);
  if (clockIn.status !== "ok") fails.unshift(`clock in returned ${clockIn.status} ${clockIn.detail}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "radio proof: FAIL" : "radio proof: PASS (the medic's radio reads the service frequency on the client, the civilian has none)");
  return fails.length ? 1 : 0;
}

export default {
  name: "radio-proof",
  usage: "radio-proof",
  run: (ctx) => runRadioProof(),
};

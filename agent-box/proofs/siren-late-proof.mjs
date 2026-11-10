import { spawnSync } from "node:child_process";
import { existsSync, readdirSync, readFileSync } from "node:fs";
import { join } from "node:path";
import { ROOT, arrive, clientBox, sessionDir } from "../session.mjs";

// #471: the siren mode is replicated state. Client 1 (the only client at first) clocks in as a medic and
// turns the ambulance siren on; client 2 then joins late and must apply the mode it received with the
// snapshot. Run on `up --clients 1`; client 2 is read from its own log line "replicated mode index" / "mode -> ".
async function runSirenLateProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^[0-9]+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const AMB = "Roadmap_Hospital_Ambulance";
  arrive(on, 1, wait);
  wait(8000);
  on(1, { verb: "stand", entityName: "Roadmap_Hospital_Duty_Desk" });
  wait(2500);
  on(1, { verb: "look" });
  on(1, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Hospital_Duty_Desk" }, 10000);
  wait(2500);
  const onR = on(1, { verb: "siren", entityName: AMB, mode: 1 }, 10000);
  if (!onR.detail.includes("changed=1 on=1")) fails.push(`siren did not turn on: ${onR.status} ${onR.detail}`);
  wait(2000);

  const joined = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "session", "join", "2"], { windowsHide: true, stdio: "inherit" });
  if (joined.status !== 0) fails.push("client 2 did not join late");
  else {
    arrive(on, 2, wait);
    wait(10000);
    const logs = [];
    const clientDir = join(dir, "client2", "logs");
    if (existsSync(clientDir)) {
      for (const d of readdirSync(clientDir).sort()) {
        const p = join(clientDir, d, "console.log");
        if (existsSync(p)) logs.push(readFileSync(p, "utf8"));
      }
    }
    const text = logs.join(String.fromCharCode(10));
    const seen = text.match(/Siren\]? mode -> \S+ \(index 1\)/g) ?? [];
    console.log(`client 2 siren lines with the running mode: ${seen.length}`);
    if (seen.length < 1) fails.push("client 2 never applied mode index 1 after joining late");
  }
  on(1, { verb: "siren", entityName: AMB, mode: 0 }, 10000);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "siren late proof: FAIL" : "siren late proof: PASS (late joiner received the running siren mode)");
  return fails.length ? 1 : 0;
}

export default {
  name: "siren-late-proof",
  usage: "siren-late-proof",
  run: (ctx) => runSirenLateProof(),
};

import { ROB_TARGET, arrive, clientBox, sessionDir } from "../session.mjs";

// M2 #235 on a running `up --clients 2 --wait` session with client 2 on the roster (`session.mjs roster 2`
// before `up`): client 1 robs the fuel till, client 2 clocks in and arrests them into the cell. Client 1
// holds a charge (dev gate, standing in for the fence), breaches the jail door from inside, and leaves the
// cell volume while the door is open. Confirm `[ELDebug:Jail] ... escaped the cell through the open door`
// and a `[ELDebug:Wanted] crime report ... Jailbreak` line in the server log.
async function runJailbreakProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const must = (r, what) => {
    if (r.status !== "ok") fails.push(`${what} returned ${r.status} ${r.detail}`);
    return r;
  };
  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);

  on(2, { verb: "stand", entityName: "Roadmap_Police_Desk" });
  wait(2500);
  on(2, { verb: "look" });
  must(on(2, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Police_Desk" }, 10000), "clock in");
  wait(2000);

  on(1, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  on(1, { verb: "look" });
  must(on(1, { verb: "use", className: "LF_RobAction", entityName: ROB_TARGET }, 10000), "rob");
  wait(2500);

  // The arrest: the officer stands by the robber and takes them to the cell.
  on(2, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  must(on(2, { verb: "use", className: "LF_JailArrestAction", entityName: "Player1" }, 10000), "arrest");
  wait(4000);
  const jailed = on(1, { verb: "state" }, 10000).detail;
  on(1, { verb: "capture", mode: "ui", name: "m2_235_cell_client1" }, 25000);

  // The breach from inside the cell: the door opens and stays open.
  must(on(1, { verb: "charge" }, 10000), "charge");
  wait(1500);
  on(1, { verb: "stand", entityName: "Roadmap_Jail_Door" });
  wait(2500);
  must(on(1, { verb: "use", className: "LF_BreachArmAction", entityName: "Roadmap_Jail_Door" }, 10000), "breach");
  wait(4000);
  on(1, { verb: "capture", mode: "ui", name: "m2_235_door_open_client1" }, 25000);

  // Out through the open door: the street outside the station.
  on(1, { verb: "stand", entityName: "Roadmap_Police_Desk" });
  wait(5000);
  on(1, { verb: "capture", mode: "ui", name: "m2_235_escaped_client1" }, 25000);
  on(2, { verb: "capture", mode: "ui", name: "m2_235_dispatch_client2" }, 25000);
  const out = on(1, { verb: "state" }, 10000).detail;

  if (!jailed.includes("wanted=")) fails.push(`client 1 after the arrest: ${jailed}`);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(`client 1 after the escape: ${out}`);
  console.log(fails.length ? "jailbreak proof: FAIL" : "jailbreak proof: PASS (arrested, breached the door, walked out; confirm the server log for the escape and the Jailbreak crime report)");
  return fails.length ? 1 : 0;
}

export default {
  name: "jailbreak-proof",
  usage: "jailbreak-proof",
  run: (ctx) => runJailbreakProof(),
};

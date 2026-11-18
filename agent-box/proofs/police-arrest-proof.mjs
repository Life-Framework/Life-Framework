import { ROB_TARGET, arrive, clientBox, parseState, sessionDir } from "../session.mjs";

// #2494: the police menu had blank Search and Arrest buttons (removed, #2509/#2535). The real police
// path is the player interaction menu on the suspect: Search (LF_ConfiscateAction) and Arrest
// (LF_JailArrestAction). Client 1 robs the fuel till (wanted), client 2 clocks in, stands beside them,
// the menu is photographed with both entries, then Search and Arrest run and the suspect is in the cell.
// `roster 2` before `up --clients 2 --profile full`. `police-arrest-proof [--name <shot prefix>]`
async function runPoliceArrestProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const flag = (name, dflt) => { const i = process.argv.indexOf(name); return i > 0 ? process.argv[i + 1] : dflt; };
  const prefix = flag("--name", "m5_2494_police");
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
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

  const DESK = "Roadmap_Police_Desk"; // town-layout-ok: playtest-town desk the session proofs clock in at
  on(2, { verb: "stand", entityName: DESK });
  wait(2500);
  on(2, { verb: "look" });
  must(on(2, { verb: "use", className: "LF_DutyAction", entityName: DESK }, 10000), "clock in");
  wait(2000);

  on(1, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  on(1, { verb: "look" });
  must(on(1, { verb: "use", className: "LF_RobAction", entityName: ROB_TARGET }, 10000), "rob");
  wait(7000);
  const wanted = parseState(on(1, { verb: "state" }, 10000).detail).wanted;
  if (!(Number(wanted) > 0)) fails.push(`client 1 is not wanted (${wanted}); nothing to arrest`);

  on(2, { verb: "stand", entityName: "Player1" });
  wait(4000);
  const look = on(2, { verb: "look" }, 10000);
  wait(1500);
  console.log(`officer menu on the suspect: ${look.detail}`);
  for (const label of ["LF_ConfiscateAction", "LF_JailArrestAction"]) {
    if (!look.detail.includes(label)) fails.push(`the interaction menu does not offer ${label}`);
  }
  on(2, { verb: "capture", mode: "ui", name: `${prefix}_menu` }, 25000);

  must(on(2, { verb: "use", className: "LF_ConfiscateAction", entityName: "Player1" }, 10000), "search");
  wait(2500);
  on(2, { verb: "capture", mode: "ui", name: `${prefix}_search` }, 25000);

  let arrest;
  for (let i = 0; i < 4; i++) {
    on(2, { verb: "look" });
    arrest = on(2, { verb: "use", className: "LF_JailArrestAction", entityName: "Player1" }, 10000);
    if (arrest.status === "ok") break;
    wait(2000);
  }
  must(arrest, "arrest");
  wait(4000);
  const jailed = parseState(on(1, { verb: "state" }, 10000).detail);
  if (!(Number(jailed.jail) > 0)) fails.push(`client 1 jail=${jailed.jail} after the arrest`);
  on(1, { verb: "capture", mode: "ui", name: `${prefix}_cell_client1` }, 25000);
  on(2, { verb: "capture", mode: "ui", name: `${prefix}_arrested_client2` }, 25000);

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "police arrest proof: FAIL" : "police arrest proof: PASS (menu offered Search and Arrest, search ran, the suspect is serving a sentence in the cell)");
  return fails.length ? 1 : 0;
}

export default {
  name: "police-arrest-proof",
  usage: "police-arrest-proof [--name <shot prefix>]",
  run: (ctx) => runPoliceArrestProof(),
};

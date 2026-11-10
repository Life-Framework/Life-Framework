import { join } from "node:path";
import { ORG_GATE, ORG_NAME, arriveCivilians, clientBox, judgeOrg, parseState, sessionDir } from "../session.mjs";

async function runOrg(args) {
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

  arriveCivilians(on, wait);
  const before1 = state(1);
  const before2 = state(2);
  const one = `Player${parseState(before1).player}`;
  const two = `Player${parseState(before2).player}`;

  // Client 1 founds the organization at the registry desk and takes the yard lease.
  const steps = [];
  on(1, { verb: "stand", entityName: "Roadmap_Org_Desk" });
  wait(2500);
  steps.push(on(1, { verb: "use", className: "LF_OrganizationDeskAction", entityName: "Roadmap_Org_Desk" }, 10000));
  wait(1500);
  steps.push(on(1, { verb: "type", widget: "OrgName", text: ORG_NAME }));
  steps.push(on(1, { verb: "click", widget: "FoundButton" }));
  wait(3000);
  steps.push(on(1, { verb: "click", widget: "OrgHideoutRow_Action" }));
  wait(2500);
  on(1, { verb: "ui" });
  if (args.includes("--shot")) {
    h.setBoxes([clientBox(dir, 1)]);
    const shot = h.sendCommand({ verb: "capture", mode: "ui", name: "org_ledger" }, 25000);
    console.log(`client 1: shot ui org_ledger -> ${shot.status} ${shot.detail}`);
    wait(3000);
    console.log(`client 1: ledger shot ${h.resolveCapture(shot.detail) || "not written"}`);
  }
  steps.push(on(1, { verb: "click", widget: "CloseButton" }));

  // The lease shut the gate. Client 2 is not a member yet: the gate refuses.
  on(2, { verb: "stand", entityName: ORG_GATE });
  wait(2500);
  const gateBefore = on(2, { verb: "use", className: "SCR_DoorUserAction", entityName: ORG_GATE }, 10000).detail;

  // Face to face: client 1 invites, client 2 joins.
  on(1, { verb: "stand", entityName: two });
  wait(2500);
  on(1, { verb: "look" });
  steps.push(on(1, { verb: "use", className: "LF_PlayerRoleAction", entityName: two }, 10000));
  wait(2000);
  on(2, { verb: "look" });
  steps.push(on(2, { verb: "use", className: "LF_PlayerRoleAction", entityName: one }, 10000));
  wait(2500);

  // Client 2 is a member now: the gate opens.
  on(2, { verb: "stand", entityName: ORG_GATE });
  wait(2500);
  const gateAfter = on(2, { verb: "use", className: "SCR_DoorUserAction", entityName: ORG_GATE }, 10000).detail;
  wait(1500);
  const after1 = state(1);
  const after2 = state(2);

  console.log("");
  console.log(`before  client 1: ${before1}`);
  console.log(`before  client 2: ${before2}`);
  console.log(`after   client 1: ${after1}`);
  console.log(`after   client 2: ${after2}`);
  console.log(`gate    client 2: before join ${gateBefore}, after join ${gateAfter}`);
  const fails = judgeOrg(before1, after1, after2, gateBefore, gateAfter);
  for (const st of steps) if (st.status !== "ok") fails.unshift(`${st.line} returned ${st.status} ${st.detail}`);
  for (const x of fails) console.log(`FAIL  ${x}`);
  console.log(fails.length ? "org: FAIL" : `org: PASS (founded ${ORG_NAME}, invited, joined, the gate opened for the member)`);
  return fails.length ? 1 : 0;
}

export default {
  name: "org",
  usage: "org",
  run: (ctx) => runOrg(ctx.args),
};

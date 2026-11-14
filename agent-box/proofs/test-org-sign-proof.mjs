import { arrive, clientBox, sessionDir } from "../session.mjs";

// Test org sign (#934) on a running session, driven the way a solo tester does it.
//   node agent-box/session.mjs up --profile full     (one client)
//   node agent-box/session.mjs test-org-sign-proof [--shot]
//   node agent-box/session.mjs down
// Stand at the sign by the registry desk, join the Test Organisation (the first one in founds it
// and takes the hideout yard), walk to the hideout door and open it, come back and leave.
const SIGN = "Sign_TestOrg";
const DOOR = "Roadmap_Hideout_Door";
const JOIN = "LF_TestOrgJoinAction";
const LEAVE = "LF_TestOrgLeaveAction";
const ORG = "Test_Organisation"; // the harness state prints the name with underscores

async function runTestOrgSignProof(args = []) {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail.slice(0, 240)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const must = (r, what) => {
    if (r.status !== "ok") fails.push(`${what} returned ${r.status} ${r.detail}`);
    return r;
  };
  const shot = (name) => {
    if (!args.includes("--shot")) return;
    h.setBoxes([clientBox(dir, 1)]);
    const s = h.sendCommand({ verb: "capture", mode: "ui", name }, 25000);
    wait(3000);
    console.log(`client 1: shot ${name} -> ${h.resolveCapture(s.detail) || "not written"}`);
  };

  arrive(on, 1, wait);
  if (!on(1, { verb: "state" }, 10000).detail.includes("org=none")) fails.push("the tester starts with an organisation");

  // At the sign: both actions are offered.
  must(on(1, { verb: "stand", entityName: SIGN }), "stand at the sign");
  wait(2500);
  const look = must(on(1, { verb: "look" }), "look at the sign");
  for (const action of [JOIN, LEAVE]) {
    if (!look.detail.includes(action)) fails.push(`the F key does not offer ${action} at ${SIGN}: ${look.detail.slice(0, 200)}`);
  }
  shot("test_org_sign");

  // Join: the tester is in the Test Organisation.
  must(on(1, { verb: "use", className: JOIN, entityName: SIGN }, 10000), "use Join");
  wait(2500);
  if (!on(1, { verb: "state" }, 10000).detail.includes(`org=${ORG}`)) fails.push(`after Join the tester is not in ${ORG}`);

  // The hideout door now opens for them.
  must(on(1, { verb: "stand", entityName: DOOR }), "stand at the hideout door");
  wait(2500);
  must(on(1, { verb: "look" }), "look at the door");
  shot("test_org_door");
  must(on(1, { verb: "use", className: "SCR_DoorUserAction", entityName: DOOR }, 10000), "open the hideout door");
  wait(1500);

  // Leave: back at the sign, the tester belongs to nothing again.
  must(on(1, { verb: "stand", entityName: SIGN }), "stand at the sign again");
  wait(2500);
  must(on(1, { verb: "use", className: LEAVE, entityName: SIGN }, 10000), "use Leave");
  wait(2500);
  if (!on(1, { verb: "state" }, 10000).detail.includes("org=none")) fails.push("after Leave the tester still has an organisation");

  if (fails.length) {
    console.log("test-org-sign-proof: FAIL");
    for (const f of fails) console.log(`  ${f}`);
    return 1;
  }
  console.log("test-org-sign-proof: OK");
  return 0;
}

export default { name: "test-org-sign-proof", usage: "test-org-sign-proof [--shot]", run: (ctx) => runTestOrgSignProof(ctx.args) };

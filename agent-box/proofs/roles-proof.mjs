import { arrive, clientBox, sessionDir } from "../session.mjs";

// NPC outfits and role dialogue on a running session, driven the way a player does it.
//   node agent-box/session.mjs up --profile full     (one client)
//   node agent-box/session.mjs roles-proof
//   node agent-box/session.mjs down
// Stands at four placed NPCs (a farmer, a market trader, a police officer, the fuel station clerk),
// shoots each dressed for the job, then talks to the farmer: the greeting, a reply, goodbye.
const SITES = [
  { name: "FarmerNpc", shot: "roles_farmer" },
  { name: "TraderChainNpc", shot: "roles_trader" },
  { name: "PoliceNpc", shot: "roles_police" },
  { name: "Roadmap_Fuel_Clerk", shot: "roles_fuel_clerk" },
];
const TALK = "LF_RolesAction";

async function runRolesProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail.slice(0, 200)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const expect = (cond, what) => { if (!cond) fails.push(what); };

  arrive(on, 1, wait);
  on(1, { verb: "input", action: "MenuBack" });
  wait(6000);
  for (const site of SITES) {
    const r = on(1, { verb: "stand", entityName: site.name, side: "n" });
    expect(r.status === "ok", `stand at ${site.name}: ${r.status} ${r.detail}`);
    wait(2500);
    on(1, { verb: "capture", mode: "ui", name: site.shot }, 25000);
  }

  on(1, { verb: "stand", entityName: "FarmerNpc", side: "n" });
  wait(2500);
  const look = on(1, { verb: "look" });
  expect(look.detail.includes(TALK), `the F key offers ${TALK} at the farmer: ${look.detail.slice(0, 200)}`);
  const use = on(1, { verb: "use", className: TALK, entityName: "FarmerNpc" }, 10000);
  expect(use.status === "ok", `talking to the farmer returned ${use.status} ${use.detail}`);
  wait(2500);
  const scene = (arg, value) => on(1, { verb: "dialogue", arg, value }, 10000).detail;
  const greeting = scene();
  expect(/open node=greet/.test(greeting), `the farmer's greeting opens (got: ${greeting})`);
  on(1, { verb: "capture", mode: "ui", name: "roles_dialogue_greeting" }, 25000);
  wait(5000);
  const replies = scene();
  expect(/phase=choosing replies=4/.test(replies), `four replies are offered (got: ${replies})`);
  on(1, { verb: "capture", mode: "ui", name: "roles_dialogue_replies" }, 25000);
  scene("pick", "0");
  wait(2500);
  const answer = scene();
  expect(/open node=what/.test(answer), `the first reply is answered (got: ${answer})`);
  on(1, { verb: "capture", mode: "ui", name: "roles_dialogue_answer" }, 25000);
  wait(5000);
  scene("pick", "1");
  wait(6000);
  expect(scene().startsWith("closed"), "Goodbye ends the scene");

  if (fails.length) {
    console.log("roles-proof: FAIL");
    for (const f of fails) console.log(`  ${f}`);
    return 1;
  }
  console.log("roles-proof: OK");
  return 0;
}

export default { name: "roles-proof", usage: "roles-proof", run: () => runRolesProof() };

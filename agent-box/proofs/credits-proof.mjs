import { arrive, clientBox, sessionDir } from "../session.mjs";

// The credits notice (#481) on a running session, the way a player reads it.
//   node agent-box/session.mjs up --profile full     (one client)
//   node agent-box/session.mjs credits-proof
//   node agent-box/session.mjs down
// Client 1 walks to the notice beside the post office, the F key offers Read: Credits, the card
// opens with the title, a section label and the project Life Framework continues, and Close shuts
// it. Two shots: the notice on the wall and the open card.
const SITE = "Sign_Credits";
const ACTION = "LF_SignReadAction";

async function runCreditsProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail.slice(0, 300)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const must = (r, what) => {
    if (r.status !== "ok") fails.push(`${what} returned ${r.status} ${r.detail}`);
    return r;
  };

  arrive(on, 1, wait);
  must(on(1, { verb: "stand", entityName: SITE }), "stand at the notice");
  wait(2500);
  const look = must(on(1, { verb: "look" }), "look");
  if (!look.detail.includes(ACTION)) fails.push(`the F key does not offer ${ACTION} at ${SITE}: ${look.detail.slice(0, 200)}`);
  on(1, { verb: "capture", mode: "ui", name: "credits_notice" }, 25000);
  must(on(1, { verb: "use", className: ACTION, entityName: SITE }, 10000), "read the notice");
  wait(1500);
  const card = must(on(1, { verb: "ui", mode: "text" }, 10000), "read the card").detail;
  for (const want of ["Credits", "PROJECTS WE BUILD ON", "Everon Life RPG Framework", "life-framework.org/credits"]) {
    if (!card.toLowerCase().includes(want.toLowerCase())) fails.push(`the card does not show "${want}": ${card.slice(0, 300)}`);
  }
  on(1, { verb: "capture", mode: "ui", name: "credits_card" }, 25000);
  must(on(1, { verb: "click", widget: "Close" }, 10000), "close the card");
  wait(800);
  const after = on(1, { verb: "ui", mode: "text" }, 10000).detail;
  if (after.includes("Everon Life RPG Framework")) fails.push("Close left the card open");

  if (fails.length) {
    console.log("credits-proof: FAIL");
    for (const f of fails) console.log(`  ${f}`);
    return 1;
  }
  console.log("credits-proof: OK");
  return 0;
}

export default { name: "credits-proof", usage: "credits-proof", run: () => runCreditsProof() };

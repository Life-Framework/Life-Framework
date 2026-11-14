import { join } from "node:path";
import { arrive, clientBox, parseState, sessionDir } from "../session.mjs";

// #936 on a running `up --profile full --clients 2 --sound` session. Client 1 talks to the fuel
// attendant unarmed (no robbery reply), draws a pistol, and robs the till through the dialogue:
// small talk, "This is a robbery", the attendant panics, the demand starts the hold on the server,
// the panel counts down ("Keep the clerk at gunpoint: 0:30") while the attendant begs, and the till
// pays. Client 2 is a bystander who hears the attendant. A second robbery is walked away from and
// fails with "you left the counter". The world action "Rob the till" is gone from the fuel till.
// `--film` records the first robbery on client 1 with its game audio (tools/film/audio.mjs).
// An agent cannot listen: the proof reads the dialogue log lines; the maintainer's ears do the rest.
export const CLERK = "Roadmap_Fuel_Clerk";
export const TILL = "Roadmap_Fuel_Till";
// Roadmap_Fuel_Till's m_fHoldSeconds, plus the server's quarter-second tick and the network.
export const HOLD_MS = 30000;
// Line lengths in seconds (tools/assets/dialogue/fuel_clerk/elevenlabs.json), rounded up, plus a beat.
const LINE = { greet: 5, about_where: 12.5, chat_quiet: 11, rob_panic: 9, rob_open: 11.5, paid: 6.5 };

// A pistol in the sidearm slot, drawn.
export function armAndDraw(on, n, wait) {
  const give = on(n, { verb: "pistol" }, 10000);
  wait(1500);
  on(n, { verb: "hold", item: "weapon" }, 10000);
  wait(2500);
  return give;
}

// Client n, armed and at the counter, demands the till through the attendant's dialogue. `pace`
// lets each line play for that long instead of skipping it (the film). Returns the scene states.
export function demandTill(on, n, wait, { pace = false, smallTalk = false } = {}) {
  const scene = (arg, value) => on(n, { verb: "dialogue", arg, value }, 10000).detail;
  const line = (id) => {
    if (pace) wait(LINE[id] * 1000);
    else {
      wait(1200);
      scene("skip");
      wait(800);
    }
  };
  const seen = {};
  on(n, { verb: "use", className: "LF_DialogueTalkAction", entityName: CLERK }, 10000);
  line("greet");
  seen.greet = scene();
  if (smallTalk) {
    scene("pick", "1"); // "Quiet day?"
    wait(600);
    line("chat_quiet");
    seen.chat = scene();
  }
  scene("pick", "2"); // "This is a robbery. Hands where I can see them!"
  wait(600);
  line("rob_panic");
  seen.panic = scene();
  scene("pick", "0"); // "Open the till. Now, or you die behind that counter."
  wait(1500);
  seen.demanded = scene();
  return seen;
}

async function runFuelRobberyProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const at = process.argv.indexOf("--name");
  const prefix = at >= 0 && process.argv[at + 1] ? process.argv[at + 1] : "fuelrob";
  const shot = (n, name) => on(n, { verb: "capture", mode: "ui", name: `${prefix}_${name}` }, 25000);
  const scene = (n, arg, value) => on(n, { verb: "dialogue", arg, value }, 10000).detail;
  const hold = (n) => on(n, { verb: "robhold" }, 10000).detail;
  const fails = [];
  const expect = (cond, what) => { if (!cond) fails.push(what); };

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  on(1, { verb: "resetcrime" }, 10000);
  on(1, { verb: "stand", entityName: TILL, side: "s" });
  on(2, { verb: "stand", entityName: TILL, side: "w" });
  wait(2500);
  on(1, { verb: "look" });
  shot(1, "0_counter");

  // The till's own action is retired: robbing it is a conversation now.
  const world = on(1, { verb: "use", className: "LF_RobAction", entityName: TILL }, 10000);
  expect(world.status !== "ok" && /cannot_show|action_not_found/.test(world.detail), `the till's world action is gone (got ${world.status} ${world.detail})`);

  // Unarmed: the attendant answers small talk and offers no robbery.
  on(1, { verb: "use", className: "LF_DialogueTalkAction", entityName: CLERK }, 10000);
  wait(LINE.greet * 1000);
  const unarmed = scene(1);
  expect(/node=greet phase=choosing replies=3/.test(unarmed), `unarmed, the greeting offers three replies and no robbery (got: ${unarmed})`);
  shot(1, "1_unarmed_replies");
  scene(1, "pick", "0"); // "Hey, where are we?"
  wait(1500);
  shot(1, "2_where_are_we");
  wait((LINE.about_where - 1.5) * 1000);
  on(1, { verb: "input", action: "MenuBack" });
  wait(2000);

  const before = parseState(on(1, { verb: "state" }, 10000).detail);
  const give = armAndDraw(on, 1, wait);
  expect(give.status === "ok" && /pistol added=(1|true)/.test(give.detail), `the harness gave a pistol (got ${give.detail})`);
  on(1, { verb: "capture", mode: "ui", name: `${prefix}_3_drawn` }, 25000);

  const film = process.argv.includes("--film");
  let seen;
  let clipLine = "";
  const robbery = async () => {
    seen = demandTill(on, 1, wait, { pace: film, smallTalk: film });
    const demandedAt = Date.now() - 1500; // demandTill waits 1.5 s after the pick
    const until = (ms) => wait(Math.max(0, demandedAt + ms - Date.now()));
    wait(1500);
    shot(1, "5_open_line_and_panel");
    const running = hold(1);
    expect(/^running left=0:[0-3]\d/.test(running), `the hold panel runs with the time left (got: ${running})`);
    until(LINE.rob_open * 1000 + 1000);
    shot(1, "6_hold_panel");
    shot(2, "7_bystander");
    const mid = hold(1);
    expect(/^running/.test(mid) && /edge=false|edge=0/.test(mid), `mid-hold the robber stands within the radius (got: ${mid})`);
    until(HOLD_MS + 1500);
    const done = hold(1);
    expect(/^result/.test(done) && / end=1$/.test(done), `the hold ends paid (LF_ERobHoldEnd.PAID = 1) (got: ${done})`);
    shot(1, "8_paid");
    wait(LINE.paid * 1000);
  };

  if (film) {
    const audio = await import("../../tools/film/audio.mjs");
    const eng = await import("../../tools/film/engine.mjs");
    const send = (spec, t) => { h.setBoxes([clientBox(dir, 1)]); return h.sendCommand(spec, t); };
    const resolveAlias = (alias) => { h.setBoxes([clientBox(dir, 1)]); return h.resolveCapture(alias); };
    const pid = audio.clientPid(join(dir, "session.json"), 1);
    const r = await audio.shootWithAudio({ send, resolveAlias, name: `fuel_robbery_${Date.now() % 100000000}`, fps: 15, seconds: 120, pid, body: robbery, label: "fuel-robbery" });
    clipLine = r.ok ? `clip with audio: ${r.out} (mean_volume ${r.mean} dB, wav ${r.wav})` : `clip failed: ${r.error}`;
    if (r.clip) console.log(eng.describe(r.clip));
    expect(r.ok, clipLine);
  } else {
    await robbery();
  }

  expect(/phase=choosing replies=4/.test(seen.greet), `armed, the greeting offers four replies with the robbery (got: ${seen.greet})`);
  expect(/node=rob_panic/.test(seen.panic), `the robbery reply reaches the panic (got: ${seen.panic})`);
  expect(/node=rob_open|closed/.test(seen.demanded), `the demand opens the till node (got: ${seen.demanded})`);
  const after = parseState(on(1, { verb: "state" }, 10000).detail);
  expect(Number(after.cash) > Number(before.cash), `the till paid cash (${before.cash} -> ${after.cash})`);
  expect(Number(after.wanted) > Number(before.wanted), `the robbery raised the wanted level (${before.wanted} -> ${after.wanted})`);
  const heard = scene(2);
  expect(/lines=[3-9]|lines=\d\d/.test(heard), `the bystander heard the attendant panic, beg and pay (got: ${heard})`);

  // A second robbery, walked away from: the server ends it and the panel says why.
  on(1, { verb: "resetcrime" }, 10000);
  wait(11000); // the rate limit lets two starts in ten seconds
  on(1, { verb: "stand", entityName: TILL, side: "s" });
  wait(2000);
  demandTill(on, 1, wait);
  wait(4000);
  const started = hold(1);
  expect(/^running/.test(started), `the second hold runs (got: ${started})`);
  on(1, { verb: "stand", entityName: "Fuel_PumpHum" });
  wait(1500);
  const left = hold(1);
  expect(/^result/.test(left) && / end=2$/.test(left), `walking off ends it as LEFT (LF_ERobHoldEnd.LEFT = 2) (got: ${left})`);
  shot(1, "9_left_failed");

  if (clipLine) console.log(clipLine);
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "fuel robbery proof: FAIL" : "fuel robbery proof: PASS (read the shots for the panel; grep the server log for 'held robbery' and the client log for '[LFDebug:Dialogue] line')");
  return fails.length ? 1 : 0;
}

export default {
  name: "fuel-robbery-proof",
  usage: "fuel-robbery-proof [--film] [--name <shot prefix>]   (up --profile full --clients 2 [--sound for --film])",
  run: () => runFuelRobberyProof(),
};

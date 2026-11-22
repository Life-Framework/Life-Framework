import { arrive, clientBox, sessionDir } from "../session.mjs";

// The personal file (F2) on a running session, both tabs, the way a player reads it.
//   node agent-box/session.mjs up --profile full     (one client, a fresh character)
//   node agent-box/session.mjs player-menu-proof
//   node agent-box/session.mjs down
// The client signs form R-89, puts the welcome guide away, opens the file on PERSON and waits (bounded)
// for the server's answer, picks a skill (name, text, XP bar, numbers), then turns to PROGRESS and picks
// the current errand. A UI shot of each. Server lines to read after: `[LFDebug:PlayerMenu] file sent to
// player <n>`; client: `file received: <n> skills, <n> quests, <n> licences`.
const ANSWER_BUDGET_MS = 15000;

async function runPlayerMenuProof() {
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
  const must = (r, what) => {
    if (r.status !== "ok") fails.push(`${what} returned ${r.status} ${r.detail}`);
    return r;
  };
  const state = () => on(1, { verb: "playermenu", words: ["state"] });

  arrive(on, 1, wait);
  wait(6000);
  on(1, { verb: "guide", words: ["close"] });
  wait(500);
  // A file worth reading: XP in a few jobs, cash, savings and a house (dev-gated server grant).
  must(on(1, { verb: "playermenu", words: ["seed"] }), "seed the file");
  wait(2500);

  must(on(1, { verb: "playermenu", words: ["open", "person"] }), "open the file on PERSON");
  let answered = false;
  for (let waited = 0; waited < ANSWER_BUDGET_MS && !answered; waited += 1000) {
    answered = state().detail.includes("data=1");
    if (!answered) wait(1000);
  }
  if (!answered) fails.push(`the server's answer did not arrive within ${ANSWER_BUDGET_MS / 1000} s`);
  if (!state().detail.includes("open=1")) fails.push("the file is not open after the answer");
  wait(800);
  on(1, { verb: "capture", mode: "ui", name: "player_menu_person" }, 25000);

  must(on(1, { verb: "playermenu", words: ["pick", "3"] }), "read the fourth skill");
  wait(800);
  on(1, { verb: "capture", mode: "ui", name: "player_menu_person_skill" }, 25000);

  must(on(1, { verb: "click", widget: "PlayerMenuProgressButton" }), "press the Progress tab");
  wait(1200);
  if (!state().detail.includes("tab=1")) fails.push("the Progress tab did not open");
  on(1, { verb: "capture", mode: "ui", name: "player_menu_progress" }, 25000);

  must(on(1, { verb: "playermenu", words: ["pick", "1"] }), "read the second errand");
  wait(800);
  on(1, { verb: "capture", mode: "ui", name: "player_menu_progress_errand" }, 25000);

  must(on(1, { verb: "click", widget: "PlayerMenuCloseButton" }), "press Close");
  wait(800);
  if (!state().detail.includes("open=0")) fails.push("Close did not put the file away");

  if (fails.length) {
    console.log("player-menu-proof: FAIL");
    for (const f of fails) console.log(`  ${f}`);
    return 1;
  }
  console.log("player-menu-proof: OK (read the shots and the [LFDebug:PlayerMenu] lines)");
  return 0;
}

export default { name: "player-menu-proof", usage: "player-menu-proof", run: () => runPlayerMenuProof() };

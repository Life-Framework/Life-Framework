import { arrive, clientBox, sessionDir } from "../session.mjs";

// The welcome guide on a running session, driven the way a new player meets it.
//   node agent-box/session.mjs up --profile full     (one client, a fresh character)
//   node agent-box/session.mjs welcome-guide-proof
//   node agent-box/session.mjs down
// The client signs form R-89 (a brand-new character, so its first spawn marks the guide), then waits
// (bounded) for the guide to open by itself. It keeps a UI shot of every page, presses Next once, then
// Set waypoint (the toast and the HUD marker are in the next shot), reopens it the way F3 does and
// presses Don't show again. Server log lines to read after: `[LFDebug:WelcomeGuide] guide pending for new
// character`, `waypoint for player <n> set to arrival step arrival_register`, `guide dismissed for good`.
const AUTO_OPEN_BUDGET_MS = 40000;

async function runWelcomeGuideProof() {
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
  const state = () => on(1, { verb: "guide", words: ["state"] });

  arrive(on, 1, wait);

  // The guide opens by itself after the bed wake: bounded wait, one state read every 2 s.
  let opened = false;
  for (let waited = 0; waited < AUTO_OPEN_BUDGET_MS && !opened; waited += 2000) {
    opened = state().detail.includes("open=1");
    if (!opened) wait(2000);
  }
  if (!opened) fails.push(`the guide did not open by itself within ${AUTO_OPEN_BUDGET_MS / 1000} s of the first spawn`);

  for (let page = 1; page <= 5; page++) {
    must(on(1, { verb: "guide", words: ["open", String(page)] }), `open page ${page}`);
    wait(800);
    on(1, { verb: "capture", mode: "ui", name: `welcome_guide_page${page}` }, 25000);
  }

  must(on(1, { verb: "guide", words: ["open", "1"] }), "open page 1");
  wait(500);
  must(on(1, { verb: "click", widget: "WelcomeGuideNextButton" }), "press Next");
  wait(800);
  if (!state().detail.includes("page=2")) fails.push("Next did not turn to page 2");

  must(on(1, { verb: "guide", words: ["open", "1"] }), "open page 1 again");
  wait(500);
  must(on(1, { verb: "click", widget: "WelcomeGuideWaypointButton" }), "press Set waypoint");
  wait(1500);
  if (!state().detail.includes("open=0")) fails.push("Set waypoint did not close the guide");
  on(1, { verb: "capture", mode: "ui", name: "welcome_guide_waypoint" }, 25000);

  must(on(1, { verb: "guide", words: ["open", "5"] }), "open the last page");
  wait(500);
  must(on(1, { verb: "click", widget: "WelcomeGuideDismissButton" }), "press Don't show again");
  wait(1500);
  if (!state().detail.includes("open=0")) fails.push("Don't show again did not close the guide");
  on(1, { verb: "capture", mode: "ui", name: "welcome_guide_dismissed" }, 25000);

  if (fails.length) {
    console.log("welcome-guide-proof: FAIL");
    for (const f of fails) console.log(`  ${f}`);
    return 1;
  }
  console.log("welcome-guide-proof: OK (read the shots and the [LFDebug:WelcomeGuide] server lines)");
  return 0;
}

export default { name: "welcome-guide-proof", usage: "welcome-guide-proof", run: () => runWelcomeGuideProof() };

import { readFileSync } from "node:fs";
import { join } from "node:path";
import { arrive, clientBox, newestLog, sessionDir, sessionPaths } from "../session.mjs";

// Changing character at the town clerk on a running `up --mission grid --profile full` (the Everon grid, under the pack test header)
// session (the pack allows 3 characters per account). Client 1 arrives as its first character, asks the clerk
// for another file ("About my papers" -> "I need another of my files"), and the registry sheet opens in switch
// mode with the first character marked "On file now". It registers a second resident on form R-89 and spawns
// as them; then it goes back to the clerk, the sheet now marks the second character, and it picks the first
// one again, who comes back where they stood. Shots: the papers replies, both switch sheets, the form, and the
// world after each switch. The server log must show both switches and no refusal.
const SECOND = ["Bea", "Bandit"];
// The Everon grid's town-hall cell (a grid fixture; the town layers are the maintainer's).
const CLERK = "TownHall_Clerk";

async function runCharacterSwitchProof() {
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
  const prefix = at >= 0 && process.argv[at + 1] ? process.argv[at + 1] : "charswitch";
  const shot = (name) => on(1, { verb: "capture", mode: "ui", name: `${prefix}_${name}` }, 25000);
  const scene = (arg, value) => on(1, { verb: "dialogue", arg, value }, 10000).detail;
  const ui = () => on(1, { verb: "ui" }).detail || "";
  // What the sheet says, every widget (the marked line is text, not a button).
  const uiText = () => on(1, { verb: "ui", mode: "text" }).detail || "";
  const fails = [];
  const expect = (cond, what) => { if (!cond) fails.push(what); };
  const serverLog = () => {
    const f = newestLog(join(sessionPaths(dir).server, "logs"));
    return f ? readFileSync(f, "utf8") : "";
  };

  // Talk to the clerk and take the papers reply, then "another file". The rows are the shown replies.
  const askClerk = (tag) => {
    on(1, { verb: "stand", entityName: CLERK, side: "w" });
    wait(2000);
    on(1, { verb: "look" });
    const use = on(1, { verb: "use", className: "LF_QuestAction", entityName: CLERK }, 10000);
    expect(use.status === "ok", `${tag}: talking to the clerk returned ${use.status} ${use.detail}`);
    wait(4500);
    scene("skip");
    wait(800);
    const greet = scene();
    expect(/node=greet phase=choosing/.test(greet), `${tag}: the greeting offers replies (got: ${greet})`);
    // Greeting rows: [new or next], papers, bye.
    scene("pick", "1");
    wait(2500);
    const papers = scene();
    expect(/node=papers phase=choosing/.test(papers), `${tag}: "About my papers" opens the papers replies (got: ${papers})`);
    shot(`${tag}_papers`);
    // Papers rows: [lost card while missing], another file, back. A carried card hides the first.
    const rows = Number((/replies=(\d+)/.exec(papers) || [])[1] || 0);
    scene("pick", rows >= 3 ? "1" : "0");
    wait(5000);
  };

  arrive(on, 1, wait);
  wait(10000);

  askClerk("1");
  const first = ui();
  const firstText = uiText();
  expect(!first.includes("CharSlot0") && first.includes("CharNew") && first.includes("CharStay"), `the switch sheet offers new and stay, and the played character is no button (got: ${first})`);
  expect(/On file now/.test(firstText), `the played character is marked On file now (got: ${firstText})`);
  shot("2_switch_sheet_first");

  on(1, { verb: "click", widget: "CharNew" });
  wait(4000);
  const form = ui();
  expect(form.includes("SignButton"), `"Register a new resident" opens form R-89 (got: ${form})`);
  shot("3_form_second");
  on(1, { verb: "type", widget: "RegFirstName", text: SECOND[0] });
  on(1, { verb: "type", widget: "RegLastName", text: SECOND[1] });
  on(1, { verb: "type", widget: "RegAge", text: "29" });
  wait(400);
  on(1, { verb: "click", widget: "SignButton" });
  wait(10000);
  shot("4_world_as_second");
  let log = serverLog();
  expect(/\[LFDebug:CharacterSwitch\] player \d+ put away character/.test(log), "the server put the first character away");
  expect(/character registered for player \d+, age 29, slot \d+/.test(log), "the second character registered in its own slot");

  askClerk("5");
  const second = ui();
  const secondText = uiText();
  expect(second.includes("CharSlot0"), `the sheet now offers the first character (got: ${second})`);
  expect(/On file now/.test(secondText) && secondText.includes(SECOND[0]), `the second character is marked On file now (got: ${secondText})`);
  shot("6_switch_sheet_second");

  on(1, { verb: "click", widget: "CharSlot0" });
  wait(10000);
  shot("7_world_as_first_again");
  log = serverLog();
  expect(/\[LFDebug:CharacterSwitch\] player \d+ switched from .* to slot 1/.test(log), "the server switched back to slot 1");
  expect(!/\[LFDebug:CharacterSwitch\] switch refused/.test(log), "no switch was refused");

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "character switch proof: FAIL" : "character switch proof: PASS (read the shots: both sheets, the form, the world after each switch)");
  return fails.length ? 1 : 0;
}

export default {
  name: "character-switch-proof",
  usage: "character-switch-proof [--name <shot prefix>]   (up --mission grid --profile full)",
  run: () => runCharacterSwitchProof(),
};

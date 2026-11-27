import { arrive, clientBox, sessionDir } from "../session.mjs";

// UI click smoke (2026-11-27, follows #5099): opens each main screen on one island client and presses its first
// actionable row or button through the real widget, the way a mouse does. A button that was never built (the
// "What to do next" rows had none for five days), a hidden column or a focus that swallows the press answers
// `widget_not_found <name>`, and that fails the proof. Run on one lowspec island session:
//   node agent-box/session.mjs up --mission island --profile lowspec --hold 20
//   node agent-box/session.mjs ui-click-smoke [fresh]
//   node agent-box/session.mjs down
// The three F2 lists run on the new character; the default then checkpoints it to hour 10 (cash and bank enough to
// pay a row, as `ui-sweep rich`) before the sites, and `fresh` keeps the new character. Screens, in order:
//   activities       F2 Next tab: every row must carry its Read button; presses the last row, the first, then Start
//   errands          F2 Progress tab: every errand row carries its button; presses the first (n/a when none exist yet)
//   skills           F2 Person tab: presses the first skill row (n/a when none)
//   shop list        a till's price list: the first Order button, in list order
//   licence desk     the gun counter's list (the firearm permit heads it): its first Order button
//   car lot          the lot sheet: its first Buy button (n/a on a character who can pay no row)
//   ATM              no sheet: the in-world action rows, so row 0 of `LF_ATMQuickAction` is performed
//   charge sheet     the fixture sheet: its first row action (when a row has one) and the Sign button
// Direct handler calls are not used here (`activities pick <n> direct` is the opt-out for other proofs).
// A result line per screen: `PASS|FAIL|N/A <screen>: <what was pressed or why not>`; the verdict is
// `ui-click-smoke proof: PASS|FAIL (<n> of <m>)`.
const ANSWER_BUDGET_MS = 15000;
const CLOSE_RE = /close|back|exit|cancel|keys|dismiss/i;
const sleep = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);

const FIT = "Fit_saint_philippe_";
// [id, entity, action classes in the order a player picks]
export const SITES = [
  ["shop list", `${FIT}supermarket_supermarket_till`, ["LF_ShopAction", "LF_StoreTillAction"]],
  // The licences are rows of the gun counter's price list (the firearm permit heads it). The town hall's driving
  // licence row needs an ID card (a quest scene), and `Place_LicenceDesk` is a marker no action stands at (ui-sweep).
  ["licence desk", `${FIT}gun_counter_gun_counter_clerk`, ["LF_ShopAction", "LF_StoreTillAction"]],
  ["car lot", `${FIT}carlot_vehicle_lot`, ["LF_VehicleBuyAction"]],
];
const CLERK = `${FIT}arrival_town_hall_clerk`;
const ATM = `${FIT}atms_drop_b79`;
const ATM_ROW = "LF_ATMQuickAction";

// Pure helpers (selftest: tools/validation/ui-click-smoke-selftest.mjs).

// The clickable names a `ui` answer lists ("none" is an empty screen).
export function clickables(detail) {
  const text = String(detail || "").trim();
  if (!text || text === "none") return [];
  return text.split(",").map((n) => n.trim()).filter(Boolean);
}

// The first button a player would press: a list row's own button (`<row>_Action`) first, else any button that is not
// a way out (Close, Back, Exit, Cancel, the key hints, a guide's Dismiss). "" when the screen has none.
export function firstActionable(names) {
  return names.find((n) => /_Action$/.test(n)) || names.find((n) => !CLOSE_RE.test(n)) || "";
}

// The row indexes 0..count-1 whose `<prefix><i>_Action` is not among the clickable names: a row a player cannot press.
export function rowsWithoutButton(count, names, prefix) {
  const have = new Set(names);
  const out = [];
  for (let i = 0; i < count; i++) if (!have.has(`${prefix}${i}_Action`)) out.push(i);
  return out;
}

// What a click answer means: "ok", "dead" (the widget is not there: the bug this proof exists for) or "other".
export function clickVerdict(status, detail) {
  if (status === "ok") return "ok";
  return /widget_not_found/.test(String(detail)) ? "dead" : "other";
}

// `playermenu state` / `activities state` read as numbers: `rows=3 picked=0 ids=...` -> { rows: 3, picked: 0, ids: "..." }.
export function readState(detail) {
  const out = {};
  for (const m of String(detail || "").matchAll(/(\w+)=(\S*)/g)) out[m[1]] = /^-?\d+$/.test(m[2]) ? Number(m[2]) : m[2];
  return out;
}

async function runUiClickSmoke(args = []) {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const fresh = args.includes("fresh");
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${String(r.detail).slice(0, 240)}`);
    return r;
  };
  const c = (spec, timeout = 15000) => on(1, spec, timeout);
  const results = [];
  const note = (verdict, screen, what) => {
    results.push({ verdict, screen });
    console.log(`${verdict}  ${screen}: ${what}`);
  };
  const uiNames = () => clickables(c({ verb: "ui" }, 12000).detail);
  const back = () => {
    c({ verb: "input", action: "MenuBack" });
    sleep(600);
  };
  // Presses one named widget and records it. A dead widget is named in the failure line.
  const press = (screen, widget) => {
    const r = c({ verb: "click", widget });
    const v = clickVerdict(r.status, r.detail);
    if (v === "ok") note("PASS", screen, `pressed ${widget}`);
    else if (v === "dead") note("FAIL", screen, `widget_not_found: ${widget} is not on the screen (a dead button)`);
    else note("FAIL", screen, `${widget} answered ${r.status} ${r.detail}`);
    return v === "ok";
  };
  // A verb that presses a row through the UI path; its own answer carries `widget_not_found <name>`.
  const pressVerb = (screen, spec) => {
    const r = c(spec);
    const v = clickVerdict(r.status, r.detail);
    if (v === "ok") note("PASS", screen, `${spec.verb} ${spec.words.join(" ")} -> ${r.detail}`);
    else if (v === "dead") note("FAIL", screen, `widget_not_found: ${r.detail} (the row has no button a player could press)`);
    else note("FAIL", screen, `${spec.verb} ${spec.words.join(" ")} answered ${r.status} ${r.detail}`);
    return v === "ok";
  };
  // Waits (bounded) until a state answer reads `<key>` above zero.
  const waitFor = (verb, key) => {
    let state = {};
    for (let waited = 0; waited < ANSWER_BUDGET_MS; waited += 1000) {
      state = readState(c({ verb, words: ["state"] }).detail);
      if (state[key] > 0) return state;
      sleep(1000);
    }
    return state;
  };

  arrive(on, 1, sleep);
  sleep(6000);
  c({ verb: "guide", words: ["close"] });
  sleep(500);

  // 0. Papers: the Next list is empty ("First get your papers") until the clerk has registered the character, so a
  // character with none takes the clerk's welcome talk first (the first-two-hours flow's step 0b, by `dialogue pick`).
  c({ verb: "playermenu", words: ["open", "next"] });
  const papers = waitFor("activities", "rows");
  c({ verb: "playermenu", words: ["close"] });
  sleep(600);
  if (!(papers.rows > 0)) {
    c({ verb: "stand", entityName: CLERK, side: "e2" }, 15000);
    sleep(1500);
    c({ verb: "use", className: "LF_QuestAction", entityName: CLERK }, 10000);
    const choosing = () => {
      for (let waited = 0; waited < 8000; waited += 500) {
        if (/choosing/.test(String(c({ verb: "dialogue" }, 8000).detail))) return true;
        sleep(500);
      }
      return false;
    };
    for (const step of ["greet", "welcome_papers", "welcome_room", "welcome_town"]) {
      if (!choosing()) {
        console.log(`the clerk talk never reached the replies of ${step}`);
        break;
      }
      c({ verb: "dialogue", arg: "pick", value: "0" }, 10000); // reply rows count from 0; the digit 0 is Goodbye
      sleep(1200);
    }
    back();
    sleep(3000);
  }

  // 1. F2 Next tab: the list the player picks a run from.
  c({ verb: "playermenu", words: ["open", "next"] });
  const next = waitFor("activities", "rows");
  if (!(next.rows > 0)) note("FAIL", "activities", `no activity rows after ${ANSWER_BUDGET_MS / 1000} s (${JSON.stringify(next)})`);
  else {
    sleep(800);
    const names = uiNames();
    const dead = rowsWithoutButton(next.rows, names, "PlayerMenuNext_");
    if (dead.length) note("FAIL", "activities", `widget_not_found: row(s) ${dead.join(",")} of ${next.rows} have no PlayerMenuNext_<n>_Action button (#5099)`);
    else note("PASS", "activities", `all ${next.rows} rows carry a button`);
    pressVerb("activities", { verb: "activities", words: ["pick", String(next.rows - 1)] });
    sleep(600);
    pressVerb("activities", { verb: "activities", words: ["pick", "0"] });
    sleep(600);
    pressVerb("activities", { verb: "activities", words: ["go"] });
    sleep(2500);
  }
  c({ verb: "playermenu", words: ["close"] });
  sleep(800);

  // 2. F2 Progress tab: the errands (an activity Start turns its first step into one).
  c({ verb: "playermenu", words: ["open", "progress"] });
  const progress = waitFor("playermenu", "quests");
  if (!(progress.quests > 0)) note("N/A", "errands", "the character has no errand rows to press");
  else {
    sleep(800);
    const dead = rowsWithoutButton(progress.quests, uiNames(), "PlayerMenuQuest_");
    if (dead.length) note("FAIL", "errands", `widget_not_found: row(s) ${dead.join(",")} of ${progress.quests} have no PlayerMenuQuest_<n>_Action button`);
    else note("PASS", "errands", `all ${progress.quests} rows carry a button`);
    pressVerb("errands", { verb: "playermenu", words: ["pick", "0"] });
  }
  c({ verb: "playermenu", words: ["close"] });
  sleep(800);

  // 3. F2 Person tab: the skills.
  c({ verb: "playermenu", words: ["open", "person"] });
  const person = waitFor("playermenu", "skills");
  if (!(person.skills > 0)) note("N/A", "skills", "the character has no skill rows to press");
  else {
    sleep(800);
    const dead = rowsWithoutButton(person.skills, uiNames(), "PlayerMenuSkill_");
    if (dead.length) note("FAIL", "skills", `widget_not_found: row(s) ${dead.join(",")} of ${person.skills} have no PlayerMenuSkill_<n>_Action button`);
    else note("PASS", "skills", `all ${person.skills} rows carry a button`);
    pressVerb("skills", { verb: "playermenu", words: ["pick", "0"] });
  }
  c({ verb: "playermenu", words: ["close"] });
  sleep(800);
  back();

  // The F2 lists come first, on the character as it is: a checkpoint-10 character has no run left to start (its list is
  // empty), a new one is offered mining. The sites then want money in the pocket and the bank.
  if (!fresh) {
    c({ verb: "checkpoint", words: ["10"] }, 20000);
    sleep(6000);
    c({ verb: "guide", words: ["close"] });
  }

  // 4. The sites: stand where a player does, take the action the F key offers, press the sheet's first button.
  for (const [screen, entity, classes] of SITES) {
    back();
    let cls = "";
    let host = entity;
    let seen = "";
    for (const side of ["e", "w", "n", "s", "n2", "e2"]) {
      const st = c({ verb: "stand", entityName: entity, side }, 15000);
      if (st.status !== "ok") {
        seen = `stand ${entity}: ${st.status} ${st.detail}`;
        break;
      }
      sleep(1800);
      seen = String(c({ verb: "look" }, 12000).detail);
      cls = classes.find((k) => seen.includes(k)) || "";
      if (cls) {
        const row = seen.split(",").find((x) => x.trim().startsWith(`${cls}@`));
        if (row) host = row.split("@")[1].trim();
        break;
      }
    }
    if (!cls) {
      note("FAIL", screen, `none of ${classes.join(", ")} is offered at ${entity} (${seen.slice(0, 160)})`);
      continue;
    }
    const used = c({ verb: "use", className: cls, entityName: host }, 15000);
    sleep(2500);
    if (used.status !== "ok") {
      note("FAIL", screen, `${cls} answered ${used.status} ${used.detail}`);
      continue;
    }
    const names = uiNames();
    const target = firstActionable(names);
    if (!target) {
      if (screen === "car lot" && !fresh) note("FAIL", screen, `widget_not_found: the lot sheet has no Buy button for a character with the checkpoint's money (${names.join(",") || "no buttons"})`);
      else if (screen === "car lot") note("N/A", screen, `no row this character can pay (${names.join(",") || "no buttons"})`);
      else note("FAIL", screen, `widget_not_found: no actionable widget on the screen (${names.join(",") || "no buttons"})`);
    } else press(screen, target);
    sleep(1000);
    back();
    back();
  }

  // 5. The ATM has no sheet: its rows are the F-key action list, and row 0 is performed by index.
  back();
  let faced = "";
  for (const side of ["e", "w", "n", "s"]) {
    c({ verb: "stand", entityName: ATM, side });
    sleep(2200);
    if (String(c({ verb: "look" }, 12000).detail).includes(ATM_ROW)) {
      faced = side;
      break;
    }
  }
  if (!faced) note("FAIL", "ATM", `no side of ${ATM} offers ${ATM_ROW}`);
  else {
    const r = c({ verb: "use", className: `${ATM_ROW}__0` }, 10000);
    if (r.status === "ok") note("PASS", "ATM", `performed ${ATM_ROW} row 0`);
    else note("FAIL", "ATM", `${ATM_ROW}__0 answered ${r.status} ${r.detail}`);
  }
  sleep(800);

  // 6. The charge sheet (the booking desk's sheet, opened with the harness fixture rows).
  back();
  const open = c({ verb: "chargesheet", words: ["fixture"] });
  sleep(1500);
  if (open.status !== "ok") note("FAIL", "charge sheet", `the fixture sheet did not open: ${open.status} ${open.detail}`);
  else {
    const names = uiNames();
    const row = names.find((n) => /_Action$/.test(n));
    if (row) press("charge sheet", row);
    else note("N/A", "charge sheet", `no row action on the fixture rows (${names.join(",") || "no buttons"})`);
    sleep(600);
    press("charge sheet", "ChargeSheetSign");
  }
  sleep(800);
  c({ verb: "chargesheet", words: ["close"] });

  const failed = results.filter((r) => r.verdict === "FAIL");
  const passed = results.filter((r) => r.verdict === "PASS");
  console.log(`ui-click-smoke proof: ${failed.length ? "FAIL" : "PASS"} (${failed.length} failed of ${results.length} checks, ${passed.length} pressed, ${results.length - passed.length - failed.length} n/a)`);
  for (const f of failed) console.log(`  ${f.screen}`);
  return failed.length ? 1 : 0;
}

export default { name: "ui-click-smoke", usage: "ui-click-smoke [fresh]", run: (ctx) => runUiClickSmoke(ctx?.args || []) };

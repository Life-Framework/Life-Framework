import { mkdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { arrive, clientBox, sessionDir } from "../session.mjs";
import { lintParts, parseUiText } from "./ui-sweep.mjs";

// UI sweep part 2 (2026-11-27, docs/playtest/ui-sweep-2026-11-27.md "Part 2"): the screens built with the older
// `AddListRow` status row (charge sheet, handbook, org ledger, map search, F2 My file) and the six the first sweep
// could not reach (F2 My file, the town hall licence row, the labour counter, the fence, the car lot's per-car lines).
// One island client on `up --mission island --profile full --hold 20`:
//   node agent-box/session.mjs ui-sweep-p2 [phase-name] [only-substring]
// Each screen is opened the way a player does (a key, a button, the F-key menu), its text read with `ui text`, linted
// with the part-1 rules and photographed (`sweep2_<phase>_<id>`). A desk that offers its rows as F-key lines is read with
// `aim` (the menu text). Data lands in `tmp/ui-sweep/p2-<phase>.json`.

const sleep = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);

async function runP2(args) {
  const kv = (k, d) => ((args.find((a) => a.startsWith(k + "=")) || "").slice(k.length + 1)) || d;
  const phase = args.find((a) => !a.includes("=")) || "run";
  const only = kv("only", "");
  const mode = kv("mode", "main");
  const client = Number(kv("client", "1"));
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${String(r.detail).slice(0, 260)}`);
    return r;
  };
  const c = (spec, timeout = 15000) => on(client, spec, timeout);
  const wait = sleep;
  const outDir = join(process.cwd(), "tmp", "ui-sweep");
  mkdirSync(outDir, { recursive: true });
  const record = [];
  const save = () => writeFileSync(join(outDir, `p2-${phase}.json`), JSON.stringify(record, null, 1));
  const shot = (name) => c({ verb: "capture", mode: "ui", name: `sweep2_${phase}_${name}` }, 25000);
  const read = () => String(c({ verb: "ui", mode: "text" }, 12000).detail);
  const back = () => { c({ verb: "input", action: "MenuBack" }); wait(600); };
  // Put every sheet and the pause menu away: the F-key menu pictures must not sit under the pause menu.
  const clear = () => {
    for (let i = 0; i < 4; i++) {
      const aim = String(c({ verb: "aim", words: [] }, 10000).detail);
      if (!/menu=(PauseMenuUI|[A-Za-z]*Menu[A-Za-z]*UI)/.test(aim) && !/PauseMenu/.test(aim)) return;
      c({ verb: "pausemenu", words: ["close"] });
      wait(400);
      back();
    }
  };

  // Read the open screen: text, lint, picture, record.
  const grab = (id, extra = {}) => {
    const text = read();
    const parts = parseUiText(text);
    const problems = lintParts(parts);
    shot(id);
    record.push({ id, text, problems, ...extra });
    console.log(`SCREEN ${id}: ${parts.length} parts, ${problems.length} problem(s)`);
    for (const p of problems) console.log(`   LINT ${p.problem}: ${p.part} "${p.text}"`);
    save();
    return { text, parts, problems };
  };
  const step = (id, fn) => {
    if (only && !id.includes(only)) return;
    try { fn(); } catch (e) { console.log(`STEP ${id} threw: ${e.message}`); record.push({ id, error: String(e.message) }); save(); }
  };

  // The F-key menu at a prop: stand on each side, look down, read `aim`'s menu text, photograph it.
  const menuAt = (id, entity, { sides = ["e1", "w1", "n1", "s1", "n2", "e2"], pitch = -45, want = /LF_/ } = {}) => {
    let aim = "";
    let side = "";
    for (side of sides) {
      const st = c({ verb: "stand", entityName: entity, side }, 15000);
      if (st.status !== "ok") { aim = `stand:${st.detail}`; continue; }
      wait(1500);
      c({ verb: "input", action: "CharacterPitch", value: pitch }, 8000);
      wait(700);
      aim = String(c({ verb: "aim", words: [] }, 10000).detail);
      const look = String(c({ verb: "look" }, 10000).detail);
      if (want.test(look)) { aim += ` || look=${look}`; break; }
    }
    shot(`${id}_menu`);
    c({ verb: "input", action: "CharacterPitch", value: 0 }, 8000);
    const menu = (/ menu=(.*?)( \|\| look=|$)/.exec(aim) || [0, ""])[1];
    const problems = lintParts(parseUiText(`line:menu=${menu}`));
    record.push({ id: `${id}_menu`, entity, side, aim: aim.slice(0, 600), problems });
    console.log(`MENU ${id} (${side}): ${aim.slice(0, 400)}`);
    save();
    return aim;
  };

  // mode=fresh: a new character on this client (no checkpoint, so no driver's license yet): the clerk files the papers
  // through the dialogue, then the town hall's driving-license row is read. mode=file: F2 My file only (after a booking).
  if (mode === "fresh") {
    arrive(on, client, wait);
    wait(12000);
    const clerk = "Fit_saint_philippe_arrival_town_hall_clerk";
    c({ verb: "stand", entityName: clerk, side: "w" });
    wait(2500);
    c({ verb: "use", className: "LF_QuestAction", entityName: clerk }, 10000);
    wait(7000);
    for (let i = 0; i < 14; i++) {
      c({ verb: "dialogue", arg: "pick", value: "0" }, 10000);
      wait(4500);
      if (!/idcard=none/.test(String(c({ verb: "state" }, 10000).detail))) break;
    }
    back();
    wait(1500);
    console.log("state: " + c({ verb: "state" }, 10000).detail);
    step("licence_row", () => {
      let found = "";
      for (const side of ["e2", "e3", "e4", "e1", "e5"]) {
        c({ verb: "stand", entityName: clerk, side }, 15000);
        wait(1800);
        const look = String(c({ verb: "look" }, 10000).detail);
        if (/LF_LicenceDesk/.test(look)) { found = look; break; }
      }
      record.push({ id: "licence_row_look", look: found || "not offered" });
      c({ verb: "input", action: "CharacterPitch", value: -35 }, 8000);
      wait(700);
      shot("licence_row_menu");
      const used = c({ verb: "use", className: "LF_LicenceDeskDrivingAction" }, 15000);
      wait(1500);
      shot("licence_row_toast");
      record.push({ id: "licence_row_use", status: used.status, detail: String(used.detail).slice(0, 300) });
      c({ verb: "input", action: "CharacterPitch", value: 0 }, 8000);
      save();
    });
    return 0;
  }
  if (mode === "file") {
    c({ verb: "playermenu", words: ["open", "person"] });
    wait(3500);
    c({ verb: "click", widget: "PlayerMenuFileButton" });
    wait(2500);
    grab("f2_file_booked");
    c({ verb: "playermenu", words: ["close"] });
    return 0;
  }

  arrive(on, 1, wait);
  wait(6000);
  c({ verb: "guide", words: ["close"] });
  wait(500);
  c({ verb: "checkpoint", words: ["10"] }, 20000);
  wait(6000);

  // F2 tabs: My file (booking records, seized licenses, unpaid tickets) and the gang tab.
  step("f2_file", () => {
    c({ verb: "playermenu", words: ["open", "person"] });
    wait(3500);
    c({ verb: "click", widget: "PlayerMenuFileButton" });
    wait(2500);
    grab("f2_file");
    c({ verb: "click", widget: "PlayerMenuGangButton" });
    wait(2500);
    grab("f2_gang");
    c({ verb: "playermenu", words: ["close"] });
    wait(600);
    back();
  });

  // Handbook: pause menu, Handbook button, list, a page.
  step("handbook", () => {
    c({ verb: "pausemenu", words: ["open"] });
    wait(1500);
    c({ verb: "click", widget: "HandbookButton" });
    wait(2000);
    grab("handbook_list");
    c({ verb: "click", widget: "HandbookPage1_Action" });
    wait(1500);
    grab("handbook_page1");
    back();
    back();
    c({ verb: "pausemenu", words: ["close"] });
    wait(600);
    clear();
  });

  // Charge sheet (the harness fixture, and the over-cap variant that prints the "above your rank's limit" row).
  step("chargesheet", () => {
    c({ verb: "chargesheet", words: [] });
    wait(1500);
    grab("chargesheet");
    c({ verb: "chargesheet", words: ["close"] });
    wait(500);
    c({ verb: "chargesheet", words: ["fixture-cap"] });
    wait(1500);
    grab("chargesheet_cap");
    c({ verb: "chargesheet", words: ["close"] });
    wait(500);
  });

  // The labour counter: rows are F-key lines (a perk pick at Mining 10).
  step("labour_counter", () => {
    c({ verb: "levelset", words: ["MINER", "9"] }, 10000);
    wait(2000);
    c({ verb: "levelset", words: ["MINER", "10"] }, 10000);
    wait(3500);
    c({ verb: "guide", words: ["close"] });
    clear();
    menuAt("labour_counter", "Fit_saint_philippe_labour_labour_counter", { sides: ["n2", "s2", "e2", "w2", "n1", "s1"], want: /LF_PerkPickAction/ });
  });

  // The car lot's per-car lines: the F-key menu lists Buy rows; cycle through them.
  step("carlot_lines", () => {
    const aim = menuAt("carlot_lines", "Fit_saint_philippe_carlot_vehicle_lot", { want: /LF_Vehicle/ });
    for (let i = 0; i < 3; i++) {
      c({ verb: "interact", step: "next" }, 8000);
      wait(600);
      shot(`carlot_lines_next${i + 1}`);
    }
    void aim;
  });

  // The fence: Talk and Browse goods, then the price list.
  step("fence", () => {
    clear();
    menuAt("fence", "Fit_saint_philippe_fence_fence_yard_fence", { sides: ["n2", "s2", "e2", "w2"], want: /LF_ShopAction|LF_RolesAction/ });
    const used = c({ verb: "use", className: "LF_ShopAction", entityName: "Fit_saint_philippe_fence_fence_yard_fence" }, 15000);
    wait(2500);
    if (used.status === "ok") { grab("fence_shop"); back(); }
    const stall = menuAt("fence_stall", "Fit_saint_philippe_fence_fence_yard_stall", { sides: ["n1", "s1", "e1", "w1"], want: /LF_/ });
    void stall;
  });

  // Map search (the map legend sheet).
  step("map_search", () => {
    c({ verb: "map", words: ["open"] });
    wait(3000);
    c({ verb: "maplegend", words: ["open"] });
    wait(1500);
    grab("map_legend");
    c({ verb: "maplegend", words: ["search", "bank"] });
    wait(1500);
    grab("map_search_bank");
    c({ verb: "maplegend", words: ["search", "zzzz"] });
    wait(1500);
    grab("map_search_none");
    c({ verb: "maplegend", words: ["close"] });
    c({ verb: "map", words: ["close"] });
    wait(600);
  });

  // The organization registry: found one, read its ledger (members, held sites, the hideout lease row).
  step("org_ledger", () => {
    const desk = "Fit_saint_philippe_org_registry_desk";
    c({ verb: "stand", entityName: desk, side: "n1" });
    wait(2500);
    const used = c({ verb: "use", className: "LF_OrganizationDeskAction", entityName: desk }, 15000);
    wait(2000);
    if (used.status !== "ok") { record.push({ id: "org_ledger", error: `use: ${used.detail}` }); return; }
    grab("org_registry_form");
    c({ verb: "type", widget: "OrgName", text: "SweepCrew" });
    c({ verb: "click", widget: "FoundButton" });
    wait(3500);
    grab("org_ledger");
    c({ verb: "click", widget: "OrgHideoutRow_Action" });
    wait(2500);
    grab("org_ledger_after_row");
    for (let i = 0; i < 10; i++) { c({ verb: "input", action: "MenuDown" }); wait(200); }
    wait(500);
    grab("org_ledger_scrolled");
    c({ verb: "click", widget: "CloseButton" });
    wait(600);
    c({ verb: "playermenu", words: ["open", "person"] });
    wait(3000);
    c({ verb: "click", widget: "PlayerMenuGangButton" });
    wait(2500);
    grab("f2_gang_with_org");
    c({ verb: "playermenu", words: ["close"] });
  });

  // The war board lists organizations (this one now among them); the payphone's dial sheet.
  const deskSheet = (id, entity, cls, side = "n1") => {
    clear();
    c({ verb: "stand", entityName: entity, side }, 15000);
    wait(2000);
    const used = c({ verb: "use", className: cls, entityName: entity }, 15000);
    wait(2200);
    if (used.status === "ok") grab(id);
    else record.push({ id, error: `use ${cls}: ${used.detail}` });
    back();
  };
  step("org_war_board", () => deskSheet("org_war_board", "Fit_saint_philippe_org_war_board", "LF_OrgWarBoardReadAction"));
  step("payphone", () => deskSheet("payphone", "Fit_saint_philippe_payphones_drop_b06", "LF_PhoneDialAction"));

  save();
  const bad = record.reduce((n, r) => n + (r.problems?.length || 0), 0);
  console.log(`ui-sweep-p2 ${phase}: ${record.length} screens, ${bad} lint problem(s); data tmp/ui-sweep/p2-${phase}.json`);
  return 0;
}

export default {
  name: "ui-sweep-p2",
  usage: "ui-sweep-p2 [phase-name] [only=<id substring>]",
  run: (ctx) => runP2(ctx?.args || []),
};

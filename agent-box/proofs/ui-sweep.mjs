import { mkdirSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { arrive, clientBox, sessionDir } from "../session.mjs";

// The UI sweep (2026-11-27, docs/playtest/ui-sweep-2026-11-27.md): opens every player-facing list, desk and sheet
// on one island client, reads the screen text with `ui text`, lints it for what the morning playtest found (a cut-off
// row, a raw prefab name, a `#LF-` key, a `%1`, a bare dollar-less amount, UK spelling) and takes one UI picture of
// each screen. Run on `up --mission island --profile full --hold 20` (one client):
//   node agent-box/session.mjs ui-sweep rich      checkpoint 10 first: the licence, shop and job screens of a settled player
//   node agent-box/session.mjs ui-sweep starter   a fresh character: the F2 sheet, the first guide pages
//   node agent-box/session.mjs ui-sweep rich shops   only the sites whose id contains "shops"
// Pictures keep the names `sweep_<phase>_<site>`; the lint lines and every `ui text` land in
// `tmp/ui-sweep/<phase>.json` for the report.

const SITES = [
  // shops (the price list a till opens)
  ["shop_supermarket", "Fit_saint_philippe_supermarket_supermarket_till"],
  ["shop_general", "Fit_saint_philippe_general_store_general_store_till"],
  ["shop_general_miners", "Fit_saint_philippe_general_store_general_store_miners_till"],
  ["shop_hardware", "Fit_saint_philippe_hardware_hardware_shop_till"],
  ["shop_hardware_miners", "Fit_saint_philippe_hardware_hardware_miners_till"],
  ["shop_gun", "Fit_saint_philippe_gun_counter_gun_counter_clerk"],
  ["shop_clothing", "Fit_saint_philippe_clothing_cashier_shop_01"],
  ["shop_pharmacy", "Fit_saint_philippe_pharmacy_pharmacy_till"],
  ["shop_butcher", "Fit_saint_philippe_butcher_butcher_till"],
  ["shop_bakery", "Fit_saint_philippe_bakery_bakery_till"],
  ["shop_seedfeed", "Fit_saint_philippe_seed_feed_seed_feed_till"],
  ["shop_fuel", "Fit_saint_philippe_fuel_till"],
  ["shop_newsagent", "Fit_saint_philippe_newsagent_newsagent_till"],
  ["shop_farmsupply", "Fit_saint_philippe_farm_supply_farm_supply_till"],
  ["shop_garage", "Fit_saint_philippe_districts_garage_shop_till"],
  ["shop_cafe", "Fit_saint_philippe_cafe_cafe_till"],
  ["shop_mine_supply", "Fit_coalmans_brow_mine_mine_supply_till"],
  // desks and boards
  ["atm", "Fit_saint_philippe_atms_drop_b79"],
  ["bank_teller", "Fit_saint_philippe_bank_bank_teller"],
  ["licence_desk", "Fit_saint_philippe_arrival_Place_LicenceDesk"],
  ["town_clerk", "Fit_saint_philippe_arrival_town_hall_clerk"],
  ["labour_counter", "Fit_saint_philippe_labour_labour_counter"],
  ["export_board", "Fit_saint_philippe_dock_warehouse_export_board"],
  ["wholesale_clerk", "Fit_saint_philippe_dock_wholesale_clerk"],
  ["price_board_market", "Fit_saint_philippe_market_market_price_board"],
  ["price_board_fish", "Fit_saint_philippe_market_fish_price_board"],
  ["price_board_wholesale", "Fit_saint_philippe_dock_wholesale_price_board"],
  ["town_board", "Fit_saint_philippe_board_Place"],
  ["org_registry", "Fit_saint_philippe_org_registry_desk"],
  ["org_war_board", "Fit_saint_philippe_org_war_board"],
  ["payphone", "Fit_saint_philippe_payphones_drop_b06"],
  ["bail_phone", "Fit_saint_philippe_jail_jail_bail_phone"],
  ["police_duty_desk", "Fit_saint_philippe_police_police_kit_duty_desk"],
  ["police_wanted_board", "Fit_saint_philippe_police_police_kit_wanted_board"],
  ["police_booking_desk", "Fit_saint_philippe_police_police_kit_booking_desk"],
  ["medic_duty_desk", "Fit_saint_philippe_clinic_lf_medic_duty_desk"],
  ["impound_desk", "Fit_saint_philippe_carlot_impound_desk"],
  ["carlot", "Fit_saint_philippe_carlot_vehicle_lot"],
  ["truckyard", "Fit_ne_compound_truckyard_lot"],
  ["furniture_counter", "Fit_saint_philippe_furniture_furniture_counter"],
  ["fence", "FenceNpc"],
];

// What a screen must not print. The `ui text` line is `kind:name=text | ...`; only the text after `=` is a player's.
const LINTS = [
  [/#(LF|EL|AR)-[A-Za-z0-9_]+/, "raw string key"],
  [/%\d/, "stray placeholder"],
  [/\b[A-Za-z]+_[A-Za-z0-9_]+\b/, "id with an underscore"],
  [/\b[A-Za-z]{2,}[a-z][A-Z][a-z]+\b/, "camelCase id"],
  [/\b(?:null|undefined|NaN|\(null\))\b/i, "null or NaN text"],
  [/\b(colour|armour|neighbour|favour|honour|centre|metre|litre|licence plate|grey|travelling|cancelled|organis\w+|realis\w+|analys\w+|programme|defence|offence)\b/i, "UK spelling"],
  [/\S(?:\.\.\.|…)$/, "cut-off with an ellipsis"],
  [/[£€]/, "wrong currency"],
  [/\b\d{1,3}(?:,\d{3})*\s?(?:dollars|USD|bucks)\b/i, "money spelt out"],
];

const sleep = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);

export function parseUiText(detail) {
  // `kind:name=text | kind:name=text`; a text may hold ` | ` rarely, so split on the next `word:name=` marker.
  const parts = [];
  const re = /(?:^|\s\|\s)([a-z]+):([A-Za-z0-9_#.\-]+)=/g;
  const marks = [];
  let m;
  while ((m = re.exec(detail))) marks.push({ at: m.index, end: re.lastIndex, kind: m[1], name: m[2] });
  marks.forEach((mk, i) => {
    const text = detail.slice(mk.end, i + 1 < marks.length ? marks[i + 1].at : detail.length).trim();
    parts.push({ kind: mk.kind, name: mk.name, text });
  });
  return parts;
}

export function lintParts(parts) {
  const out = [];
  for (const p of parts) {
    if (!p.text) {
      if (p.kind === "button" || p.kind === "line") out.push({ part: `${p.kind}:${p.name}`, problem: "empty label", text: "" });
      continue;
    }
    for (const [re, why] of LINTS) {
      // The credits name projects and repositories (LNGCLifeRP_Public, thebonbon/EveronLife) as they are written.
      if (p.name === "CreditsBody" && /id|camelCase/.test(why)) continue;
      const hit = re.exec(p.text);
      if (hit) out.push({ part: `${p.kind}:${p.name}`, problem: why, text: p.text.slice(0, 120), match: hit[0] });
    }
    // Money: a bare number next to a money word, or a row total with no dollar sign.
    if (/\b(price|total|fine|fee|cash|bank|balance|pays?|cost)\b/i.test(p.name + " " + p.text) && /(^|\s)\d{2,}(?:,\d{3})*(\s|$)/.test(p.text) && !/\$/.test(p.text)) {
      out.push({ part: `${p.kind}:${p.name}`, problem: "money with no dollar sign", text: p.text.slice(0, 120) });
    }
  }
  return out;
}

async function runUiSweep(args) {
  const phase = args[0] === "starter" ? "starter" : "rich";
  const only = args.slice(1).find((a) => !a.startsWith("--")) || "";
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const quiet = args.includes("--quiet");
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    if (!quiet) console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${String(r.detail).slice(0, 300)}`);
    return r;
  };
  const c = (spec, timeout = 15000) => on(1, spec, timeout);
  const wait = sleep;
  const record = [];
  const outDir = join(process.cwd(), "tmp", "ui-sweep");
  mkdirSync(outDir, { recursive: true });
  const save = () => writeFileSync(join(outDir, `${phase}.json`), JSON.stringify(record, null, 1));
  const shot = (name) => c({ verb: "capture", mode: "ui", name: `sweep_${phase}_${name}` }, 25000);

  arrive(on, 1, wait);
  wait(6000);
  c({ verb: "guide", words: ["close"] });
  wait(500);
  if (phase === "rich") {
    c({ verb: "checkpoint", words: ["10"] }, 20000);
    wait(6000);
  }

  const readScreen = () => {
    const r = c({ verb: "ui", mode: "text" }, 12000);
    return String(r.detail);
  };
  const close = () => {
    c({ verb: "input", action: "MenuBack" });
    wait(700);
  };

  // Phase screens that are not a world site: F2, F3, level cards, credits, welcome guide pages.
  const screens = [];
  if (!only || only.startsWith("!") || only.includes("f2")) {
    for (const [tab, name] of [["person", "f2_person"], ["progress", "f2_progress"], ["next", "f2_next"], ["file", "f2_file"]]) {
      screens.push({ id: name, open: () => { c({ verb: "playermenu", words: ["open", tab] }); wait(3500); }, close: () => c({ verb: "playermenu", words: ["close"] }) });
    }
  }
  if (!only || only.startsWith("!") || only.includes("guide")) {
    for (let p = 1; p <= 5; p++) screens.push({ id: `guide_page${p}`, open: () => { c({ verb: "guide", words: ["open", String(p)] }); wait(1200); }, close: () => c({ verb: "guide", words: ["close"] }) });
  }
  if (!only || only.startsWith("!") || only.includes("credits")) screens.push({ id: "credits", open: () => { c({ verb: "kit", step: "credits" }, 10000); wait(1800); }, close: () => c({ verb: "kit", step: "hide" }, 10000) });

  for (const s of screens) {
    s.open();
    const text = readScreen();
    const parts = parseUiText(text);
    const problems = lintParts(parts);
    shot(s.id);
    record.push({ id: s.id, kind: "screen", text, problems });
    console.log(`SCREEN ${s.id}: ${parts.length} parts, ${problems.length} problem(s)`);
    for (const p of problems) console.log(`   LINT ${p.problem}: ${p.part} "${p.text}"`);
    s.close();
    wait(600);
    c({ verb: "input", action: "MenuBack" });
    wait(600);
    save();
  }

  if (phase === "rich") {
    for (const [job, lvl] of [["MINER", 2], ["MINER", 3], ["MINER", 6], ["LUMBER", 2], ["DELIVERY", 4]]) {
      if (only && !only.startsWith("!") && !only.includes("level")) break;
      c({ verb: "levelset", words: [job, String(lvl)] });
      wait(700);
      const text = readScreen();
      const problems = lintParts(parseUiText(text));
      shot(`levelcard_${job.toLowerCase()}_${lvl}`);
      record.push({ id: `levelcard_${job}_${lvl}`, kind: "screen", text, problems });
      console.log(`SCREEN levelcard ${job} ${lvl}: ${problems.length} problem(s)`);
      for (const p of problems) console.log(`   LINT ${p.problem}: ${p.part} "${p.text}"`);
      wait(2500);
    }
  }

  for (const [id, entity] of SITES) {
    if (phase === "starter" && !/licence|clerk|atm|shop_general|shop_supermarket|labour|carlot/.test(id)) continue;
    if (only && only.startsWith("!") ? id.includes(only.slice(1)) : only && !id.includes(only)) continue;
    c({ verb: "input", action: "MenuBack" }); wait(500);
    let look = "";
    let offered = "";
    for (const side of ["e", "w", "n", "s", "n2", "e2"]) {
      const st = c({ verb: "stand", entityName: entity, side }, 15000);
      if (st.status !== "ok") { offered = `stand:${st.detail}`; break; }
      wait(1800);
      look = String(c({ verb: "look" }, 12000).detail);
      if (/LF_\w+Action/.test(look)) { offered = look; break; }
    }
    if (!/LF_\w+Action/.test(offered)) {
      record.push({ id, kind: "site", entity, error: `no action offered: ${offered || look}`.slice(0, 300) });
      console.log(`SITE ${id}: no LF action offered (${(offered || look).slice(0, 160)})`);
      save();
      continue;
    }
    // The classes the F key lists here, in the order a player would pick.
    const classes = [...new Set(offered.split(",").map((x) => x.split("@")[0].trim()).filter((x) => /^LF_\w+Action/.test(x)))];
    const prefer = ["LF_VehicleLotAction", "LF_ShopAction", "LF_StoreTillAction", "LF_ATMQuickAction", "LF_LicenceDeskAction", "LF_QuestAction", "LF_TraderSellAction", "LF_PriceBoardReadAction", "LF_OrganizationDeskAction", "LF_OrgWarBoardReadAction", "LF_PhoneDialAction", "LF_JailBailAction", "LF_DutyAction", "LF_ExportBoardTakeAction", "LF_RankBoardReadAction", "LF_WantedBoardReadAction", "LF_SignReadAction"];
    const pick = prefer.find((p) => classes.includes(p)) || classes[0];
    const hostRow = offered.split(",").find((x) => x.startsWith(`${pick}@`));
    const host = hostRow ? hostRow.split("@")[1].trim() : entity;
    const used = c({ verb: "use", className: pick, entityName: host }, 15000);
    wait(2500);
    const text = readScreen();
    const parts = parseUiText(text);
    const problems = lintParts(parts);
    const rows = parts.filter((p) => /Row/.test(p.name)).length;
    shot(id);
    if (rows > 8) {
      for (let i = 0; i < 8; i++) { c({ verb: "input", action: "MenuDown" }); wait(250); }
      wait(600);
      shot(`${id}_scrolled`);
    }
    record.push({ id, kind: "site", entity, classes, used: pick, useStatus: used.status, rows, text, problems });
    console.log(`SITE ${id}: ${pick} -> ${used.status}, ${parts.length} parts, ${rows} rows, ${problems.length} problem(s)`);
    for (const p of problems) console.log(`   LINT ${p.problem}: ${p.part} "${p.text}"`);
    close();
    save();
  }
  save();
  console.log(`ui-sweep ${phase}: ${record.length} screens, ${record.reduce((n, r) => n + (r.problems?.length || 0), 0)} lint problem(s); data tmp/ui-sweep/${phase}.json`);
  return 0;
}

export default {
  name: "ui-sweep",
  usage: "ui-sweep [rich|starter] [site-id substring] [--quiet]",
  run: (ctx) => runUiSweep(ctx?.args || []),
};

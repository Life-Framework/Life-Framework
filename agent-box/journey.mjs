#!/usr/bin/env node
// `journey`: the M1 player journey, end to end, on a running multi-client
// session (agent-box/session.mjs up --clients 2). Every step is driven from a
// game client the way a player does it (stand, look, use, the screen's own
// buttons and fields), records `state` before and after on the clients it
// touches, judges the change, and photographs what that client sees. The run
// writes agent-box/reports/journey.html.
//
//   node agent-box/journey.mjs                 every step that fits the session
//   node agent-box/journey.mjs atm shop        only those step ids (arrive runs when a client has no character)
//   node agent-box/journey.mjs --list          the steps
//   node agent-box/journey.mjs selftest        judges and the report, no game
//
// This is the harness M1-23 (#108) proves the journey with; it is not that
// proof. Steps marked `debug` use the debug menu as a fixture (cash, police) and
// run only on the bare DebugWorld session. `restart` runs only on
// `up --mission playtest`, where the debug menu is closed and the harness
// passes the gate through the LF_HARNESS build.

import { spawnSync } from "node:child_process";
import { copyFileSync, mkdirSync, mkdtempSync, readFileSync, writeFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { dirname, join, relative, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..");

export const NAMES = { 1: ["Ada", "Tester"], 2: ["Bea", "Tester"] };
export const DEPOSIT = 1200;
// Without the debug-menu fixture (playtest mission) a courier carries a few hundred.
export const SMALL_DEPOSIT = 100;
export const GIVE = 50;
export const ORG_NAME = "Journeymen";

// "faction=CIVILIAN cash=500 ..." -> numbers where they are numbers.
export function parseState(detail) {
  const out = {};
  for (const part of String(detail ?? "").trim().split(/\s+/)) {
    const eq = part.indexOf("=");
    if (eq > 0) out[part.slice(0, eq)] = part.slice(eq + 1);
  }
  return out;
}

const num = (s, key) => Number(s?.[key]);

// Each judge gets { before: {1: state, 2: state}, after: {...}, ctx } and
// returns a list of failures (empty is a pass).
export const STEPS = [
  {
    id: "arrive",
    title: "Arrive: faction, town, registration form",
    clients: [1, 2],
    judge: ({ after }) => {
      const fails = [];
      for (const n of [1, 2]) {
        if (!after[n] || after[n].faction === undefined) fails.push(`client ${n} has no character state`);
        else if (after[n].faction !== "CIVILIAN") fails.push(`client ${n} is ${after[n].faction}, expected CIVILIAN`);
      }
      return fails;
    },
  },
  {
    id: "job",
    title: "Take a job at the post counter",
    clients: [1],
    judge: ({ after }) => (after[1].job && after[1].job !== "UNEMPLOYED" ? [] : [`job is ${after[1].job} after signing up`]),
  },
  {
    id: "earn",
    title: "Earn cash: one parcel run",
    clients: [1],
    judge: ({ before, after }) => (num(after[1], "cash") > num(before[1], "cash") ? [] : [`cash ${before[1].cash} -> ${after[1].cash}, expected a payout`]),
  },
  {
    id: "fund",
    title: "Fixture: debug menu adds 1000 cash (a car costs 1000 from the bank)",
    clients: [1],
    debug: true,
    judge: ({ before, after }) => (num(after[1], "cash") === num(before[1], "cash") + 1000 ? [] : [`cash ${before[1].cash} -> ${after[1].cash}, expected +1000`]),
  },
  {
    id: "atm",
    title: `ATM: key in an amount (${DEPOSIT} when carried, else ${SMALL_DEPOSIT}) and deposit`,
    clients: [1],
    judge: ({ before, after, ctx }) => {
      const fails = [];
      const amount = ctx.amount ?? DEPOSIT;
      if (num(after[1], "bank") !== num(before[1], "bank") + amount) fails.push(`bank ${before[1].bank} -> ${after[1].bank}, expected +${amount}`);
      if (num(after[1], "cash") !== num(before[1], "cash") - amount) fails.push(`cash ${before[1].cash} -> ${after[1].cash}, expected -${amount}`);
      return fails;
    },
  },
  {
    id: "shop",
    title: "Shop: order the first line on the price list and pay",
    clients: [1],
    judge: ({ before, after }) => (num(after[1], "cash") < num(before[1], "cash") ? [] : [`cash ${before[1].cash} -> ${after[1].cash}, expected a payment`]),
  },
  {
    id: "vehicle",
    title: "Buy the first car at the lot (paid from the bank)",
    clients: [1],
    judge: ({ before, after }) => {
      if (num(after[1], "bank") === num(before[1], "bank") - 1000) return [];
      if (num(before[1], "bank") < 1000) return [`blocked: bank ${before[1].bank} is below the S105's 1000 (cars are paid from the bank)`];
      return [`bank ${before[1].bank} -> ${after[1].bank}, expected -1000 for the S105`];
    },
  },
  {
    id: "give",
    title: `Hand ${GIVE} cash to the other player, face to face`,
    clients: [1, 2],
    judge: ({ before, after }) => {
      const fails = [];
      if (num(after[1], "cash") !== num(before[1], "cash") - GIVE) fails.push(`client 1 cash ${before[1].cash} -> ${after[1].cash}, expected -${GIVE}`);
      if (num(after[2], "cash") !== num(before[2], "cash") + GIVE) fails.push(`client 2 cash ${before[2].cash} -> ${after[2].cash}, expected +${GIVE}`);
      return fails;
    },
  },
  {
    id: "police",
    title: "Fixture: client 2 joins the police and goes on duty (debug menu)",
    clients: [2],
    debug: true,
    judge: ({ after }) => (after[2].faction === "POLICE" && after[2].duty === "1" ? [] : [`client 2 is ${after[2].faction} duty=${after[2].duty}`]),
  },
  {
    id: "rob",
    title: "Rob the fuel till; the officer gets the alert",
    clients: [1, 2],
    judge: ({ before, after }) => {
      const fails = [];
      if (!(num(after[1], "wanted") > num(before[1], "wanted"))) fails.push(`client 1 wanted ${before[1].wanted} -> ${after[1].wanted}`);
      if (!(num(after[1], "cash") > num(before[1], "cash"))) fails.push(`client 1 cash ${before[1].cash} -> ${after[1].cash}, expected the till`);
      if (after[2].faction === "POLICE" && after[2].toast !== "#EL-Police_Alert_Robbery") fails.push(`officer's last toast is ${after[2].toast}`);
      return fails;
    },
  },
  {
    id: "org",
    title: `Found ${ORG_NAME} at the registry desk`,
    clients: [1],
    judge: ({ after }) => (after[1].org === ORG_NAME ? [] : [`org is ${after[1].org}, expected ${ORG_NAME}`]),
  },
  {
    id: "garage",
    title: "Garage: the counter offers store and retrieve",
    clients: [1],
    judge: ({ ctx }) => (/EL_Vehicle(Store|Retrieve)Action/.test(ctx.look || "") ? [] : [`look offers ${ctx.look || "nothing"}`]),
  },
  {
    id: "restart",
    title: "Restart: save, stop, load, rejoin; carried and banked money stay",
    clients: [1, 2],
    playtest: true,
    judge: ({ before, after }) => {
      const fails = [];
      for (const n of [1, 2]) {
        for (const key of ["cash", "bank", "org", "job"]) {
          if (String(after[n]?.[key]) !== String(before[n]?.[key])) fails.push(`client ${n} ${key} ${before[n]?.[key]} -> ${after[n]?.[key]}`);
        }
      }
      return fails;
    },
  },
];

export function selectSteps(ids, mission) {
  const unknown = ids.filter((id) => !STEPS.some((s) => s.id === id));
  if (unknown.length) throw new Error(`unknown step ${unknown.join(", ")}. Known: ${STEPS.map((s) => s.id).join(", ")}`);
  return STEPS.filter((s) => {
    if (ids.length && !ids.includes(s.id) && s.id !== "arrive") return false;
    if (s.debug && mission === "playtest") return false;
    if (s.playtest && mission !== "playtest") return false;
    return true;
  });
}

// The actions of one step. `h` is the driver: on(n, spec) sends one harness
// command to client n, text(n, want) waits for a screen to show want, shot(n,
// name) photographs client n's view, pause(ms), state(n), restart().
export const ACTIONS = {
  arrive(h) {
    for (const n of [1, 2]) {
      const ui = h.on(n, { verb: "ui" });
      if (ui.status === "ok" && ui.detail.includes("CivilianButton")) {
        h.on(n, { verb: "click", widget: "CivilianButton" });
        h.pause(1500);
        h.on(n, { verb: "click", widget: "TownButton" });
      }
    }
    for (const n of [1, 2]) {
      if (!h.text(n, "SignButton", 15000).found) continue;
      h.on(n, { verb: "type", widget: "RegFirstName", text: NAMES[n][0] });
      h.on(n, { verb: "type", widget: "RegLastName", text: NAMES[n][1] });
      h.on(n, { verb: "type", widget: "RegAge", text: "30" });
      h.shot(n, `journey-arrive-form-${n}`);
      h.on(n, { verb: "click", widget: "SignButton" });
    }
    h.pause(10000);
  },
  job(h) {
    h.on(1, { verb: "stand", entityName: "Roadmap_Job_Depot" });
    h.pause(1500);
    h.on(1, { verb: "use", className: "LF_JobSignUpAction", entityName: "Roadmap_Job_Depot" }, 10000);
    h.pause(1500);
  },
  earn(h) {
    h.on(1, { verb: "use", className: "LF_JobStartRunAction", entityName: "Roadmap_Job_Depot" }, 10000);
    h.pause(1500);
    h.on(1, { verb: "stand", entityName: "Roadmap_Job_Drop" });
    h.pause(1500);
    h.on(1, { verb: "use", className: "LF_JobDropoffAction", entityName: "Roadmap_Job_Drop" }, 10000);
    h.pause(2000);
  },
  fund(h) {
    h.on(1, { verb: "open", menu: "DebugMenu", mode: "show" });
    h.pause(800);
    h.on(1, { verb: "click", widget: "GiveCashButton" });
    h.on(1, { verb: "open", menu: "DebugMenu", mode: "hide" });
    h.pause(1500);
  },
  atm(h) {
    h.on(1, { verb: "stand", entityName: "ATM_Debug" });
    h.pause(1500);
    h.on(1, { verb: "use", className: "EL_ATMAction", entityName: "ATM_Debug" }, 10000);
    if (!h.text(1, "DepositButton", 8000).found) return;
    h.ctx.amount = num(h.states[1], "cash") >= DEPOSIT ? DEPOSIT : SMALL_DEPOSIT;
    for (const digit of String(h.ctx.amount)) h.on(1, { verb: "click", widget: `Key${digit}Button` });
    h.on(1, { verb: "click", widget: "DepositButton" });
    h.text(1, "DEPOSITED", 8000);
    h.shot(1, "journey-atm");
    h.on(1, { verb: "click", widget: "CloseButton" });
  },
  shop(h) {
    h.on(1, { verb: "stand", entityName: "Shop_GeneralStore_Debug" });
    h.pause(1500);
    h.on(1, { verb: "use", className: "EL_ShopAction", entityName: "Shop_GeneralStore_Debug" }, 10000);
    if (!h.text(1, "Item0Row_Action", 8000).found) return;
    h.on(1, { verb: "click", widget: "Item0Row_Action" });
    h.pause(500);
    h.on(1, { verb: "click", widget: "PayButton" });
    h.pause(2500);
    h.shot(1, "journey-shop");
    h.on(1, { verb: "click", widget: "CloseButton" });
  },
  vehicle(h) {
    h.on(1, { verb: "stand", entityName: "Roadmap_Vehicle_Lot" });
    h.pause(1500);
    h.on(1, { verb: "use", className: "EL_VehicleBuyAction", entityName: "Roadmap_Vehicle_Lot" }, 10000);
    h.pause(4000);
  },
  give(h) {
    const other = `Player${h.states[2]?.player ?? 2}`;
    h.on(1, { verb: "stand", entityName: other });
    h.pause(2000);
    h.on(1, { verb: "use", className: "LF_GiveCashAction", entityName: other }, 10000);
    if (!h.text(1, "GiveCashAmount", 8000).found) return;
    h.on(1, { verb: "type", widget: "GiveCashAmount", text: String(GIVE) });
    h.shot(1, "journey-give");
    h.on(1, { verb: "click", widget: "GiveCashButton" });
    h.pause(3000);
  },
  police(h) {
    h.on(2, { verb: "open", menu: "DebugMenu", mode: "show" });
    h.pause(800);
    h.on(2, { verb: "click", widget: "FactionPoliceButton" });
    h.on(2, { verb: "open", menu: "DebugMenu", mode: "hide" });
    h.pause(1500);
  },
  rob(h) {
    h.on(1, { verb: "stand", entityName: "Roadmap_Fuel_Till" });
    h.pause(2000);
    h.on(1, { verb: "use", className: "EL_RobAction", entityName: "Roadmap_Fuel_Till" }, 10000);
    h.pause(2500);
  },
  org(h) {
    h.on(1, { verb: "stand", entityName: "Roadmap_Org_Desk" });
    h.pause(1500);
    h.on(1, { verb: "use", className: "EL_OrganizationDeskAction", entityName: "Roadmap_Org_Desk" }, 10000);
    h.pause(1500);
    const sheet = h.on(1, { verb: "ui", mode: "text" });
    h.ctx.screen = sheet.detail;
    const field = /field:(\w+)=/.exec(sheet.detail || "");
    if (field) {
      h.on(1, { verb: "type", widget: field[1], text: ORG_NAME });
      const sign = /button:(\w*Sign\w*)=/.exec(sheet.detail || "");
      if (sign) h.on(1, { verb: "click", widget: sign[1] });
      h.pause(2500);
    }
    h.shot(1, "journey-org");
    h.on(1, { verb: "input", action: "MenuBack" });
  },
  garage(h) {
    h.on(1, { verb: "stand", entityName: "Roadmap_Garage" });
    h.pause(1500);
    h.ctx.look = h.on(1, { verb: "look" }).detail;
  },
  restart(h) {
    h.restart();
  },
};

// Runs the steps against a driver and returns the rows of the report.
export function runJourney(steps, driver) {
  const rows = [];
  for (const step of steps) {
    const ctx = {};
    const shots = [];
    const log = [];
    const h = {
      ctx,
      states: {},
      pause: driver.pause,
      on: (n, spec, timeout) => {
        const r = driver.send(n, spec, timeout);
        log.push(`client ${n}: ${r.line ? r.line.replace(/^\d+ /, "") : spec.verb} -> ${r.status} ${r.detail}`);
        return r;
      },
      text: (n, want, timeout) => {
        const r = driver.waitText(n, want, timeout);
        log.push(`client ${n}: wait-text ${want} -> ${r.found ? "found" : r.detail}`);
        return r;
      },
      shot: (n, name) => {
        const r = driver.send(n, { verb: "capture", mode: "ui", name: name.replaceAll("-", "_") }, 25000);
        // The engine can answer warn:file_not_found while the BMP is still landing; resolveCapture
        // finds it and converts it.
        const file = driver.resolve(r);
        log.push(`client ${n}: shot ${name} -> ${r.status} ${file || r.detail}`);
        if (file) shots.push({ name, file });
      },
      restart: () => {
        const r = driver.restart();
        log.push(`session restart -> ${r.code === 0 ? "ok" : "FAIL"}`);
        if (r.code !== 0) ctx.restartFailed = true;
      },
    };
    const before = {};
    for (const n of step.clients) before[n] = parseState(driver.state(n));
    h.states = before;
    driver.log(`step ${step.id}: ${step.title}`);
    try {
      ACTIONS[step.id](h);
    } catch (err) {
      log.push(`threw: ${err.message}`);
      ctx.threw = err.message;
    }
    // Every step ends with a picture from each client it touched, so the report shows the whole
    // playthrough, refusals and toasts included, not only the screens a step opens.
    for (const n of step.clients) h.shot(n, `journey-${step.id}-end-${n}`);
    const after = {};
    for (const n of step.clients) after[n] = parseState(driver.state(n));
    const failures = step.judge({ before, after, ctx });
    if (ctx.threw) failures.unshift(`driver threw: ${ctx.threw}`);
    if (ctx.restartFailed) failures.unshift("session restart failed");
    for (const line of log) driver.log(`  ${line}`);
    driver.log(`${failures.length ? "FAIL" : "pass"} ${step.id}${failures.length ? `: ${failures.join("; ")}` : ""}`);
    rows.push({ id: step.id, title: step.title, clients: step.clients, before, after, failures, shots, log, screen: ctx.screen || "" });
  }
  return rows;
}

const escapeHtml = (value) =>
  String(value ?? "")
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;");

const STATE_KEYS = ["faction", "duty", "wanted", "rank", "cash", "bank", "job", "org", "toast"];

function stateTable(row) {
  const lines = row.clients.map((n) => {
    const b = row.before[n] || {};
    const a = row.after[n] || {};
    const cells = STATE_KEYS.map((k) => {
      const changed = String(b[k]) !== String(a[k]);
      const text = changed ? `${b[k] ?? "-"} → ${a[k] ?? "-"}` : (a[k] ?? "-");
      return `<td${changed ? ' class="changed"' : ""}>${escapeHtml(text)}</td>`;
    });
    return `<tr><th>client ${n}</th>${cells.join("")}</tr>`;
  });
  return `<table><tr><th></th>${STATE_KEYS.map((k) => `<th>${k}</th>`).join("")}</tr>${lines.join("")}</table>`;
}

export function renderJourney(rows, { mission, when, clip = null }) {
  const failed = rows.filter((r) => r.failures.length).length;
  const body = rows
    .map((row) => {
      const verdict = row.failures.length
        ? `<ul class="fail">${row.failures.map((f) => `<li>${escapeHtml(f)}</li>`).join("")}</ul>`
        : '<p class="pass">Pass.</p>';
      const pictures = row.shots.length
        ? row.shots.map((s) => `<figure><img src="${escapeHtml(s.src)}" alt="${escapeHtml(s.name)}"><figcaption>${escapeHtml(s.name)}</figcaption></figure>`).join("")
        : '<p class="missing">No picture.</p>';
      return `<article class="${row.failures.length ? "fail-row" : "pass-row"}">
  <div class="pictures">${pictures}</div>
  <div class="text">
    <h2>${escapeHtml(row.id)}: ${escapeHtml(row.title)}</h2>
    ${verdict}
    ${stateTable(row)}
    <details><summary>Commands</summary><pre>${escapeHtml(row.log.join("\n"))}</pre></details>
  </div>
</article>`;
    })
    .join("\n");
  return `<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<title>Journey</title>
<style>
  body { margin: 24px; font: 16px/1.45 Georgia, serif; background: #f4f1ea; color: #1c1915; }
  article { display: grid; grid-template-columns: minmax(240px, 420px) 1fr; gap: 20px; align-items: start; margin: 24px 0; padding: 16px; border-top: 1px solid #d5cfc3; }
  img { width: 100%; height: auto; background: #ddd; }
  figure { margin: 0 0 10px; }
  table { border-collapse: collapse; font-size: 0.85rem; }
  td, th { border: 1px solid #d5cfc3; padding: 2px 6px; text-align: left; }
  .changed { background: #fff3c4; }
  .fail { color: #8c1d1d; }
  .pass { color: #1d5c32; }
  .fail-row { background: #f8e8e4; }
  .missing { color: #8c1d1d; }
  pre { white-space: pre-wrap; font-size: 0.8rem; }
  @media (max-width: 700px) { article { grid-template-columns: 1fr; } }
</style>
</head>
<body>
<h1>Journey</h1>
<p>${rows.length - failed} passed, ${failed} failed, ${rows.length} steps. Mission: ${escapeHtml(mission)}. ${escapeHtml(when)}.</p>
<p>Each step is driven from a game client on a dedicated server (agent-box/journey.mjs). State is the server's view of that player before and after the step. Every step ends with a picture from each client it touched.</p>
${clip ? `<p>Video of client 1: <a href="${escapeHtml(clip.href)}">${escapeHtml(clip.href)}</a></p>${clip.previewHref ? `<p><img src="${escapeHtml(clip.previewHref)}" alt="journey preview" style="max-width:640px"></p>` : ""}` : ""}
${body}
</body>
</html>
`;
}

export function writeJourney(rows, root, meta) {
  const outDir = join(root, "agent-box", "reports");
  const shotDir = join(outDir, "shots");
  mkdirSync(shotDir, { recursive: true });
  for (const row of rows) {
    for (const s of row.shots) {
      const name = `${s.name}.png`;
      copyFileSync(s.file, join(shotDir, name));
      s.src = `shots/${name}`;
    }
  }
  const path = join(outDir, "journey.html");
  // The clip lives in reports/film/; link it relative to the report.
  const clip = meta.clip ? { href: relative(outDir, meta.clip.out).replaceAll("\\", "/"), previewHref: meta.clip.preview ? relative(outDir, meta.clip.preview).replaceAll("\\", "/") : "" } : null;
  writeFileSync(path, renderJourney(rows, { ...meta, clip }), "utf8");
  return path;
}

export function selftestJourney() {
  const failures = [];
  const civ = "faction=CIVILIAN duty=0 wanted=0 rank=0 cash=500 bank=0 job=UNEMPLOYED org=none player=1 toast=none";
  const step = (id) => STEPS.find((s) => s.id === id);
  const judge = (id, b1, a1, b2, a2, ctx = {}) => step(id).judge({ before: { 1: parseState(b1), 2: parseState(b2) }, after: { 1: parseState(a1), 2: parseState(a2) }, ctx });
  if (judge("atm", civ.replace("cash=500", "cash=1700"), civ.replace("bank=0", "bank=1200"), civ, civ).length) failures.push("atm judge failed a good deposit");
  if (!judge("atm", civ, civ, civ, civ).length) failures.push("atm judge passed a deposit that moved nothing");
  if (!judge("earn", civ, civ, civ, civ).length) failures.push("earn judge passed no payout");
  if (judge("give", civ, civ.replace("cash=500", "cash=450"), civ, civ.replace("cash=500", "cash=550")).length) failures.push("give judge failed a good hand-over");
  if (!judge("give", civ, civ.replace("cash=500", "cash=450"), civ, civ).length) failures.push("give judge passed cash that vanished");
  if (!judge("vehicle", civ.replace("bank=0", "bank=1200"), civ.replace("bank=0", "bank=1200"), civ, civ).length) failures.push("vehicle judge passed no payment");
  if (!judge("garage", civ, civ, civ, civ, { look: "none" }).length) failures.push("garage judge passed an empty look");
  if (judge("garage", civ, civ, civ, civ, { look: "EL_VehicleStoreAction@Roadmap_Garage" }).length) failures.push("garage judge failed a store offer");
  if (!judge("restart", civ, civ.replace("cash=500", "cash=0"), civ, civ).length) failures.push("restart judge passed lost cash");
  if (judge("restart", civ, civ, civ, civ).length) failures.push("restart judge failed kept state");
  if (selectSteps([], "playtest").some((s) => s.debug)) failures.push("a debug-menu fixture runs on the playtest mission");
  if (selectSteps([], "debug").some((s) => s.id === "restart")) failures.push("restart runs on the bare DebugWorld (no stop file)");
  if (selectSteps(["atm"], "debug").map((s) => s.id).join() !== "arrive,atm") failures.push("a filtered run must still arrive first");
  try {
    selectSteps(["nope"], "debug");
    failures.push("an unknown step id was accepted");
  } catch {}
  for (const s of STEPS) if (typeof ACTIONS[s.id] !== "function") failures.push(`step ${s.id} has no actions`);

  // A scripted fake session: the ATM step sees the screen, keys the amount,
  // and the state moves. The report lists the commands and the picture.
  const scratch = mkdtempSync(join(tmpdir(), "journey-"));
  const png = join(scratch, "x.png");
  writeFileSync(png, "png");
  let bank = 0;
  let cash = 1700;
  const sent = [];
  const driver = {
    send: (n, spec) => {
      sent.push(spec.widget || spec.verb);
      if (spec.widget === "DepositButton") {
        bank += DEPOSIT;
        cash -= DEPOSIT;
      }
      return { status: "ok", detail: spec.verb === "capture" ? "$logs:x.png" : "ok", line: `1 ${spec.verb} ${spec.widget || ""}`.trim() };
    },
    waitText: () => ({ found: true }),
    state: () => `faction=CIVILIAN cash=${cash} bank=${bank} job=PARCEL org=none`,
    resolve: () => png,
    pause: () => {},
    restart: () => ({ code: 0 }),
    log: () => {},
  };
  const rows = runJourney([step("atm")], driver);
  if (rows[0].failures.length) failures.push(`fake ATM run failed: ${rows[0].failures.join("; ")}`);
  if (sent.join(",") !== "stand,use,Key1Button,Key2Button,Key0Button,Key0Button,DepositButton,capture,CloseButton,capture") failures.push(`ATM keyed ${sent.join(",")}`);
  const page = readFileSync(writeJourney(rows, scratch, { mission: "debug", when: "now" }), "utf8");
  if (!rows[0].shots.some((x) => x.name === "journey-atm-end-1")) failures.push("a step must end with a picture from each client it touched");
  if (!page.includes("shots/journey-atm.png") || !page.includes("1700 → 500") || !page.includes("Key1Button")) failures.push("journey.html lacks the picture, the state change, or the commands");
  return failures;
}

async function main(argv) {
  if (argv[0] === "selftest") {
    const f = selftestJourney();
    for (const x of f) console.log(`  ${x}`);
    console.log(f.length ? "journey selftest: FAIL" : "journey selftest: OK");
    return f.length ? 1 : 0;
  }
  if (argv.includes("--list")) {
    for (const s of STEPS) console.log(`${s.id.padEnd(8)} clients ${s.clients.join(",")}${s.debug ? "  (debug fixture)" : ""}${s.playtest ? "  (playtest only)" : ""}  ${s.title}`);
    return 0;
  }
  const h = await import("./harness.mjs");
  const ses = await import("./session.mjs");
  const dir = ses.sessionDir();
  const current = ses.readSession(dir);
  if (!current || current.stoppedAt) {
    console.log("journey: no session; start one with node agent-box/session.mjs up --clients 2");
    return 1;
  }
  const mission = current.mission || "debug";
  const steps = selectSteps(argv.filter((a) => !a.startsWith("--")), mission);
  const at = (n) => h.setBoxes([ses.clientBox(dir, n)]);
  const driver = {
    send: (n, spec, timeout) => {
      at(n);
      return h.sendCommand(spec, timeout);
    },
    waitText: (n, want, timeout) => {
      at(n);
      return h.waitForText(want, timeout);
    },
    state: (n) => {
      at(n);
      const r = h.sendCommand({ verb: "state" }, 10000);
      return r.status === "ok" ? r.detail : "";
    },
    resolve: (r) => h.resolveCapture(r.detail),
    pause: (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms),
    restart: () => {
      const r = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "session", "restart"], { windowsHide: true, stdio: "inherit" });
      return { code: r.status ?? 1 };
    },
    log: (line) => console.log(line),
  };
  // --film records client 1's window with tools/film for the whole run. Screen
  // capture cannot see the hidden LF-Agents desktop (desktop duplication and
  // GDI only read the input desktop), so this needs `session.mjs up --visible`.
  let film = null;
  if (argv.includes("--film")) {
    if (!current.visible) {
      console.log("journey: --film needs a session started with --visible (screen capture cannot read the hidden LF-Agents desktop)");
      return 1;
    }
    const { startRecording } = await import("../tools/film/film.mjs");
    film = startRecording({ window: "Arma Reforger", pid: current.clients?.[0]?.pid, label: "journey", fps: 30 });
    console.log(`journey: filming client 1 (${film.plan.method}, ${film.plan.encoder}) -> ${film.out}`);
  }
  const rows = runJourney(steps, driver);
  let clip = null;
  if (film) {
    clip = await film.stop();
    console.log(clip.ok ? `journey: clip ${clip.out}` : `journey: film failed (ffmpeg ${clip.code}); see ${clip.out.replace(/\.mp4$/i, ".ffmpeg.log")}`);
  }
  const path = writeJourney(rows, ROOT, { mission, when: new Date().toISOString(), clip: clip?.ok ? clip : null });
  const failed = rows.filter((r) => r.failures.length).length;
  console.log(`journey: ${rows.length - failed}/${rows.length} steps passed; report ${path}`);
  return failed ? 1 : 0;
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  main(process.argv.slice(2)).then(
    (c) => process.exit(c),
    (e) => {
      console.error(e.message);
      process.exit(1);
    },
  );
}

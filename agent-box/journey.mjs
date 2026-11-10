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
//   node agent-box/journey.mjs atm shop --film [--film-client 1] [--film-fps 12]
//                                              one in-engine clip per step, joined into one mp4
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
import { bmpFileToPng } from "./bmp.mjs";

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), "..");

export const NAMES = { 1: ["Ada", "Tester"], 2: ["Bea", "Tester"], 3: ["Cy", "Tester"] };
// The jail cell's middle (Street_Jail.layer) and how far a prisoner may stand from it.
export const CELL = { x: 233.7, z: 219 };
export const CELL_RADIUS = 14;
export const C_ORG_NAME = "CellarCrew";
export const CONTRABAND_PAY = 200;
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

// "x,y,z" from the state line's pos token.
export function parsePos(text) {
  const [x, y, z] = String(text ?? "").split(",").map(Number);
  return { x, y, z };
}

export function distanceToCell(pos) {
  const p = parsePos(pos);
  return Math.hypot(p.x - CELL.x, p.z - CELL.z);
}

// What the restart judge compares for every client (stash, garage and parked are the world's view
// of that player; rank is the police rank, 0 until a promotion).
// Seconds left on each sentence the server loaded from the save: "[LFDebug:Jail] apply-save <uid> remaining=40 serving=1".
export function parseSentenceRestores(logText) {
  const out = [];
  for (const m of String(logText ?? "").matchAll(/Jail\] apply-save \S+ remaining=(\d+) serving=1/g)) out.push(Number(m[1]));
  return out;
}

export const RESTART_KEYS = ["cash", "bank", "org", "job", "rank", "stash", "garage", "parked"];

// Each judge gets { before: {1: state, 2: state}, after: {...}, ctx } and
// returns a list of failures (empty is a pass).
export const STEPS = [
  {
    id: "arrive",
    title: "Arrive: faction, town, registration form",
    clients: [1, 2],
    judge: ({ after }) => {
      const fails = [];
      for (const n of Object.keys(after).map(Number)) {
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
    id: "clockin",
    title: "Clock in at the police desk (client 2 is on the roster)",
    clients: [2],
    playtest: true,
    judge: ({ after }) => (after[2].faction === "POLICE" && after[2].duty === "1" ? [] : [`client 2 is ${after[2].faction} duty=${after[2].duty} after the desk`]),
  },
  {
    id: "rob",
    title: "Rob the fuel till; the officer gets the alert",
    clients: [1, 2],
    judge: ({ before, after }) => {
      const fails = [];
      if (!(num(after[1], "wanted") > num(before[1], "wanted"))) fails.push(`client 1 wanted ${before[1].wanted} -> ${after[1].wanted}`);
      if (!(num(after[1], "cash") > num(before[1], "cash"))) fails.push(`client 1 cash ${before[1].cash} -> ${after[1].cash}, expected the till`);
      if (after[2].faction === "POLICE" && after[2].toast !== "#LF-Dispatch_Radio") fails.push(`officer's last toast is ${after[2].toast}`);
      return fails;
    },
  },
  {
    id: "board",
    title: "Wanted board: the officer reads the police notices, a civilian reads the public ones",
    clients: [1, 2],
    judge: ({ ctx }) => {
      const fails = [];
      if (!String(ctx.officerBoard || "").includes("WANTED - police notices")) fails.push(`officer's board shows: ${String(ctx.officerBoard || "nothing").slice(0, 120)}`);
      if (String(ctx.officerBoard || "").includes("No one is wanted")) fails.push("officer's board is empty after a robbery");
      if (!String(ctx.publicBoard || "").includes("WANTED")) fails.push(`public board shows: ${String(ctx.publicBoard || "nothing").slice(0, 120)}`);
      return fails;
    },
  },
  {
    id: "dispatch",
    title: "Dispatch: the radio call names the robbery and the place, not the robber; the dispatch board opens",
    clients: [2],
    judge: ({ after, ctx }) => {
      const fails = [];
      if (!(num(after[2], "dispatch") >= 1)) fails.push(`officer heard ${after[2].dispatch} dispatch(es)`);
      if (!String(after[2].lastdispatch ?? "").startsWith("#LF-Offence_Robbery@")) fails.push(`lastdispatch=${after[2].lastdispatch}`);
      if (String(after[2].lastdispatch ?? "").includes(NAMES[1][0])) fails.push("the dispatch names the robber");
      if (!ctx.boardRead) fails.push("the dispatch board read was refused");
      return fails;
    },
  },
  {
    id: "ticket",
    title: "Ticket: the officer writes one face to face, the holder pays it at the desk",
    clients: [1, 2],
    judge: ({ before, after, ctx }) => {
      const fails = [];
      if (ctx.written !== "1") fails.push(`holder carried ${ctx.written} unpaid ticket(s) after the write, expected 1`);
      if (!(ctx.owed > 0)) fails.push("the ticket owes nothing");
      if (num(after[1], "cash") !== num(before[1], "cash") - ctx.owed) fails.push(`holder cash ${before[1].cash} -> ${after[1].cash}, expected -${ctx.owed}`);
      if (after[1].tickets !== "0") fails.push(`holder still carries ${after[1].tickets} unpaid ticket(s)`);
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
    id: "orgc",
    title: `Civilian C founds ${C_ORG_NAME} (3-client session only; with two clients the org-C steps run on client 1's ${ORG_NAME})`,
    clients: [3],
    needsThree: true,
    judge: ({ after }) => (after[3].org === C_ORG_NAME ? [] : [`client 3 org is ${after[3].org}, expected ${C_ORG_NAME}`]),
  },
  {
    id: "hideout",
    title: "Hideout: the organization takes the alley yard lease on the ledger",
    clients: ["c"],
    judge: ({ ctx }) => (ctx.leased ? [] : [`the ledger never read the lease as ours: ${String(ctx.screen || "nothing").slice(0, 160)}`]),
  },
  {
    id: "stash",
    title: "Stash: a member puts an apple in the hideout stash",
    clients: ["c"],
    judge: ({ before, after, ctx }) => {
      const fails = [];
      if (ctx.stashed !== "stash moved=1") fails.push(`stash move answered: ${ctx.stashed}`);
      if (num(after[ctx.c], "stash") !== num(before[ctx.c], "stash") + 1) fails.push(`stash items ${before[ctx.c].stash} -> ${after[ctx.c].stash}, expected +1`);
      return fails;
    },
  },
  {
    id: "contraband",
    title: "Contraband: start the convoy at the barn, deliver the crate at the pub",
    clients: ["c"],
    judge: ({ before, after, ctx }) => {
      const fails = [];
      if (num(after[ctx.c], "cash") !== num(before[ctx.c], "cash") + CONTRABAND_PAY) fails.push(`cash ${before[ctx.c].cash} -> ${after[ctx.c].cash}, expected +${CONTRABAND_PAY} (toast after the start: ${ctx.startToast})`);
      return fails;
    },
  },
  {
    id: "garage",
    title: "Garage: the counter offers store and retrieve; the bought car is stored",
    clients: [1],
    judge: ({ before, after, ctx }) => {
      const fails = [];
      if (!/LF_Vehicle(Store|Retrieve)Action/.test(ctx.look || "")) fails.push(`look offers ${ctx.look || "nothing"}`);
      if (before[1].parked === "none") fails.push("blocked: client 1 has no car in the world to store");
      else if (num(after[1], "garage") !== num(before[1], "garage") + 1) fails.push(`garage ${before[1].garage} -> ${after[1].garage} (parked ${before[1].parked} -> ${after[1].parked}), expected one more stored`);
      return fails;
    },
  },
  {
    id: "arrest",
    title: "Arrest: the officer sends client 1 to jail from the interaction menu",
    clients: [1, 2],
    judge: ({ before, after, ctx }) => {
      const fails = [];
      if (!(num(before[1], "wanted") > 0)) fails.push(`client 1 is not wanted (${before[1].wanted}); nothing to arrest`);
      if (!(num(after[1], "jail") > 0)) fails.push(`client 1 jail=${after[1].jail} after the arrest (use ${ctx.use})`);
      return fails;
    },
  },
  {
    id: "jail",
    title: "Jail: the prisoner is in the cell and the sentence counts down",
    clients: [1],
    judge: ({ before, after }) => {
      const fails = [];
      if (!(num(after[1], "jail") > 0)) fails.push(`sentence over or never served: jail=${after[1].jail}`);
      else if (!(num(after[1], "jail") < num(before[1], "jail"))) fails.push(`sentence did not count down: ${before[1].jail} -> ${after[1].jail}`);
      const d = distanceToCell(after[1].pos);
      if (!(d <= CELL_RADIUS)) fails.push(`prisoner is ${Math.round(d)} m from the cell at ${after[1].pos}`);
      return fails;
    },
  },
  {
    id: "restart",
    title: "Restart: save, stop, load, rejoin; money, org, job, rank, stash, the stored car and a running sentence stay",
    clients: [1, 2],
    all: true,
    playtest: true,
    judge: ({ before, after, ctx }) => {
      const fails = [];
      for (const n of Object.keys(before).map(Number)) {
        for (const key of RESTART_KEYS) {
          if (before[n]?.[key] === undefined) continue; // a token a build does not print
          if (String(after[n]?.[key]) !== String(before[n]?.[key])) fails.push(`client ${n} ${key} ${before[n]?.[key]} -> ${after[n]?.[key]}`);
        }
        // A sentence keeps running through the restart: served and never longer, or loaded from
        // the save (the server log) and already run out by the time the clients are back.
        if (num(before[n], "jail") > 0) {
          const running = num(after[n], "jail") > 0 && num(after[n], "jail") <= num(before[n], "jail");
          const restored = (ctx.restoredSentences ?? []).some((left) => left > 0 && left <= num(before[n], "jail"));
          if (!running && !restored) fails.push(`client ${n} jail ${before[n].jail} -> ${after[n]?.jail}, and the server log restored ${(ctx.restoredSentences ?? []).join(",") || "no sentence"}`);
        }
      }
      return fails;
    },
  },
];

// The steps a session runs. `clients` is the number of game clients in the session: client 3 is
// civilian C (the org steps, "c" in a step's client list); with two clients C is client 1.
// `needsThree` steps run only with three, and `arrive` and `all` steps then cover client 3 too.
export function selectSteps(ids, mission, clients = 2) {
  const unknown = ids.filter((id) => !STEPS.some((s) => s.id === id));
  if (unknown.length) throw new Error(`unknown step ${unknown.join(", ")}. Known: ${STEPS.map((s) => s.id).join(", ")}`);
  const c = clients >= 3 ? 3 : 1;
  const out = [];
  for (const s of STEPS) {
    if (ids.length && !ids.includes(s.id) && s.id !== "arrive") continue;
    if (s.debug && mission === "playtest") continue;
    if (s.playtest && mission !== "playtest") continue;
    if (s.needsThree && clients < 3) continue;
    let list = s.clients.map((n) => (n === "c" ? c : n));
    if ((s.id === "arrive" || s.all) && clients >= 3) list = [1, 2, 3];
    out.push({ ...s, clients: list, c });
  }
  return out;
}

// The actions of one step. `h` is the driver: on(n, spec) sends one harness
// command to client n, text(n, want) waits for a screen to show want, shot(n,
// name) photographs client n's view, pause(ms), state(n), restart().
export const ACTIONS = {
  arrive(h) {
    // The registration form is the first screen (no faction screen, M1-11).
    for (const n of h.clients) {
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
    h.on(1, { verb: "use", className: "LF_ATMAction", entityName: "ATM_Debug" }, 10000);
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
    h.on(1, { verb: "use", className: "LF_ShopAction", entityName: "Shop_GeneralStore_Debug" }, 10000);
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
    // The lot opens its price sheet; the first row (the S105) has Buy when the bank covers it.
    h.on(1, { verb: "use", className: "LF_VehicleBuyAction", entityName: "Roadmap_Vehicle_Lot" }, 10000);
    if (h.text(1, "Car0_Action", 8000).found) {
      h.shot(1, "journey-vehicle-sheet");
      h.on(1, { verb: "click", widget: "Car0_Action" });
    }
    h.pause(4000);
    h.on(1, { verb: "click", widget: "LotClose" });
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
    h.on(1, { verb: "use", className: "LF_RobAction", entityName: "Roadmap_Fuel_Till" }, 10000);
    h.pause(2500);
  },
  org(h) {
    foundOrg(h, 1, ORG_NAME);
  },
  orgc(h) {
    foundOrg(h, 3, C_ORG_NAME);
  },
  clockin(h) {
    h.on(2, { verb: "stand", entityName: "Roadmap_Police_Desk" });
    h.pause(2500);
    h.on(2, { verb: "look" });
    h.on(2, { verb: "use", className: "LF_DutyAction", entityName: "Roadmap_Police_Desk" }, 10000);
    h.pause(2500);
  },
  board(h) {
    const read = (n) => {
      h.on(n, { verb: "stand", entityName: "Roadmap_Police_WantedBoard" });
      h.pause(2500);
      h.on(n, { verb: "use", className: "LF_WantedBoardReadAction", entityName: "Roadmap_Police_WantedBoard" }, 10000);
      h.pause(2500);
      const screen = h.on(n, { verb: "ui", mode: "text" }).detail;
      h.shot(n, `journey-board-${n}`);
      h.on(n, { verb: "click", widget: "Close" });
      h.pause(800);
      return screen;
    };
    h.ctx.officerBoard = read(2);
    h.ctx.publicBoard = read(1);
  },
  dispatch(h) {
    // The radio call already reached the officer at the robbery; the board is the written record.
    h.on(2, { verb: "stand", entityName: "Roadmap_Police_DispatchBoard" });
    h.pause(2500);
    const read = h.on(2, { verb: "use", className: "LF_WantedBoardReadAction", entityName: "Roadmap_Police_DispatchBoard" }, 10000);
    h.ctx.boardRead = read.status === "ok";
    h.pause(2500);
    h.ctx.screen = h.on(2, { verb: "ui", mode: "text" }).detail;
    h.shot(2, "journey-dispatch-board");
    h.on(2, { verb: "click", widget: "Close" });
    h.pause(800);
  },
  ticket(h) {
    const suspect = `Player${h.states[1]?.player ?? 1}`;
    h.on(2, { verb: "stand", entityName: suspect });
    h.pause(2500);
    h.on(2, { verb: "look" });
    h.on(2, { verb: "use", className: "LF_PlayerRoleAction", entityName: suspect }, 10000);
    if (!h.text(2, "POLICE TICKET", 10000).found) return;
    h.shot(2, "journey-ticket-pad");
    h.on(2, { verb: "ui", mode: "text" });
    h.on(2, { verb: "click", widget: "TicketOffence0" });
    h.pause(3000);
    if (!h.text(1, "Issued to", 10000).found) return;
    h.shot(1, "journey-ticket-card");
    h.on(1, { verb: "ui", mode: "text" });
    h.on(1, { verb: "click", widget: "TicketClose" });
    const written = h.state(1);
    h.ctx.written = written.tickets;
    h.ctx.owed = Number(written.owed);
    h.on(1, { verb: "stand", entityName: "Roadmap_Police_Desk" });
    h.pause(2500);
    h.on(1, { verb: "look" });
    h.on(1, { verb: "use", className: "LF_FineTicketPayAction", entityName: "Roadmap_Police_Desk" }, 10000);
    if (!h.text(1, "PAID", 10000).found) return;
    h.shot(1, "journey-ticket-receipt");
    h.on(1, { verb: "ui", mode: "text" });
    h.on(1, { verb: "click", widget: "TicketClose" });
    h.pause(1000);
  },
  hideout(h) {
    const n = h.c;
    h.on(n, { verb: "stand", entityName: "Roadmap_Org_Desk" });
    h.pause(1500);
    h.on(n, { verb: "use", className: "LF_OrganizationDeskAction", entityName: "Roadmap_Org_Desk" }, 10000);
    if (!h.text(n, "OrgHideoutRow", 10000).found) return;
    h.on(n, { verb: "click", widget: "OrgHideoutRow_Action" });
    h.pause(2500);
    h.ctx.leased = h.text(n, "Yours", 8000).found;
    h.ctx.screen = h.on(n, { verb: "ui", mode: "text" }).detail;
    h.shot(n, "journey-hideout-lease");
    h.on(n, { verb: "input", action: "MenuBack" });
    // The yard gate and the stash are where members meet; the stash offers its open action here.
    h.on(n, { verb: "stand", entityName: "Roadmap_Hideout_Stash" });
    h.pause(1500);
    h.ctx.look = h.on(n, { verb: "look" }).detail;
  },
  stash(h) {
    const n = h.c;
    h.on(n, { verb: "stand", entityName: "Roadmap_Hideout_Stash" });
    h.pause(1500);
    h.on(n, { verb: "apples" }, 10000);
    h.ctx.stashed = h.on(n, { verb: "stash" }, 10000).detail;
    h.pause(1500);
  },
  contraband(h) {
    const n = h.c;
    h.on(n, { verb: "stand", entityName: "Roadmap_Convoy_Start" });
    h.pause(2000);
    h.on(n, { verb: "use", className: "LF_ConvoyStartAction", entityName: "Roadmap_Convoy_Start" }, 10000);
    h.pause(2000);
    h.ctx.startToast = h.state(n).toast;
    h.on(n, { verb: "stand", entityName: "Roadmap_Convoy_Drop" });
    h.pause(2000);
    h.on(n, { verb: "use", className: "LF_ConvoyDeliverAction", entityName: "Roadmap_Convoy_Drop" }, 10000);
    h.pause(2500);
  },
  garage(h) {
    // Stands in for driving the bought car over (the harness has no driving): the server sets it
    // down beside the counter, and the store action is the player's own.
    h.on(1, { verb: "park" }, 10000);
    h.pause(1500);
    h.on(1, { verb: "stand", entityName: "Roadmap_Garage" });
    h.pause(1500);
    h.ctx.look = h.on(1, { verb: "look" }).detail;
    h.on(1, { verb: "use", className: "LF_VehicleStoreAction", entityName: "Roadmap_Garage" }, 10000);
    h.pause(3000);
  },
  arrest(h) {
    const suspect = `Player${h.states[1]?.player ?? 1}`;
    h.on(2, { verb: "stand", entityName: suspect });
    h.pause(2500);
    h.on(2, { verb: "look" });
    const r = h.on(2, { verb: "use", className: "LF_JailArrestAction", entityName: suspect }, 10000);
    h.ctx.use = `${r.status} ${r.detail}`;
    h.pause(5000);
  },
  jail(h) {
    h.pause(4000);
  },
  restart(h) {
    h.restart();
  },
};

// Found an organization at the registry desk as client n.
function foundOrg(h, n, name) {
  h.on(n, { verb: "stand", entityName: "Roadmap_Org_Desk" });
  h.pause(1500);
  h.on(n, { verb: "use", className: "LF_OrganizationDeskAction", entityName: "Roadmap_Org_Desk" }, 10000);
  h.pause(1500);
  const sheet = h.on(n, { verb: "ui", mode: "text" });
  h.ctx.screen = sheet.detail;
  const field = /field:(\w+)=/.exec(sheet.detail || "");
  if (field) {
    h.on(n, { verb: "type", widget: field[1], text: name });
    // The ledger's button is FoundButton, labelled "Sign and pay $250".
    const sign = /button:(\w+)=Sign/.exec(sheet.detail || "") || /button:(\w*Found\w*)=/.exec(sheet.detail || "");
    if (sign) h.on(n, { verb: "click", widget: sign[1] });
    h.pause(2500);
  }
  h.shot(n, `journey-org-${n}`);
  h.on(n, { verb: "input", action: "MenuBack" });
}

// Runs the steps against a driver and returns the rows of the report.
export function runJourney(steps, driver) {
  const rows = [];
  for (const step of steps) {
    const ctx = { c: step.c ?? 1 };
    const shots = [];
    const log = [];
    const h = {
      ctx,
      clients: step.clients,
      c: step.c ?? 1,
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
      state: (n) => parseState(driver.state(n)),
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
        // The booted server logs each sentence the save gave back; a short one can run out
        // before the clients are back, so the log is the proof it was loaded.
        ctx.restoredSentences = parseSentenceRestores(driver.serverLog ? driver.serverLog() : "");
        log.push(`server log restored sentences: ${ctx.restoredSentences.join(",") || "none"}`);
      },
    };
    const before = {};
    for (const n of step.clients) before[n] = parseState(driver.state(n));
    h.states = before;
    driver.log(`step ${step.id}: ${step.title}`);
    driver.beginStep?.(step);
    try {
      ACTIONS[step.id](h);
    } catch (err) {
      log.push(`threw: ${err.message}`);
      ctx.threw = err.message;
    }
    // Every step ends with a picture from each client it touched, so the report shows the whole
    // playthrough, refusals and toasts included, not only the screens a step opens.
    for (const n of step.clients) h.shot(n, `journey-${step.id}-end-${n}`);
    const clip = driver.endStep?.(step) || null;
    if (clip) log.push(clip.ok ? `film client ${clip.client}: ${clip.line}` : `film: ${clip.error}`);
    const after = {};
    for (const n of step.clients) after[n] = parseState(driver.state(n));
    const failures = step.judge({ before, after, ctx });
    if (ctx.threw) failures.unshift(`driver threw: ${ctx.threw}`);
    if (ctx.restartFailed) failures.unshift("session restart failed");
    for (const line of log) driver.log(`  ${line}`);
    driver.log(`${failures.length ? "FAIL" : "pass"} ${step.id}${failures.length ? `: ${failures.join("; ")}` : ""}`);
    rows.push({ id: step.id, title: step.title, clients: step.clients, before, after, failures, shots, log, screen: ctx.screen || "", clip: clip?.ok ? clip : null });
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
      const video = row.clip?.href ? `<p>Video (client ${escapeHtml(row.clip.client)}): <a href="${escapeHtml(row.clip.href)}">${escapeHtml(row.clip.href)}</a></p>` : "";
      return `<article class="${row.failures.length ? "fail-row" : "pass-row"}">
  <div class="pictures">${pictures}</div>
  <div class="text">
    <h2>${escapeHtml(row.id)}: ${escapeHtml(row.title)}</h2>
    ${verdict}
    ${video}
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
      // A session client leaves the engine's BMP; the report and the docs want a PNG.
      if (/\.bmp$/i.test(s.file)) bmpFileToPng(s.file, join(shotDir, name));
      else copyFileSync(s.file, join(shotDir, name));
      s.src = `shots/${name}`;
    }
  }
  const path = join(outDir, "journey.html");
  for (const row of rows) if (row.clip?.out) row.clip.href = relative(outDir, row.clip.out).replaceAll("\\", "/");
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
  const kept = civ + " stash=1 garage=1 parked=none jail=0 pos=0,0,0";
  const park = civ + " stash=1 garage=0 parked=AB12 jail=0 pos=0,0,0";
  if (!judge("garage", civ, civ, civ, civ, { look: "none" }).length) failures.push("garage judge passed an empty look");
  if (judge("garage", park, kept, civ, civ, { look: "LF_VehicleStoreAction@Roadmap_Garage" }).length) failures.push("garage judge failed a stored car");
  if (!judge("garage", park, park, civ, civ, { look: "LF_VehicleStoreAction@Roadmap_Garage" }).length) failures.push("garage judge passed a car that stayed on the street");
  if (!judge("garage", kept, kept, civ, civ, { look: "LF_VehicleStoreAction@Roadmap_Garage" }).length) failures.push("garage judge passed with no car to store");
  // police B and org C judges
  const cop = "faction=POLICE duty=1 wanted=0 rank=0 cash=500 bank=0 job=UNEMPLOYED org=none player=2 toast=#LF-Dispatch_Radio dispatch=1 lastdispatch=#LF-Offence_Robbery@#LF-Sign_Fuel_Title";
  if (judge("clockin", civ, civ, civ, cop).length) failures.push("clockin judge failed an officer on duty");
  if (!judge("clockin", civ, civ, civ, civ).length) failures.push("clockin judge passed a civilian");
  if (judge("dispatch", civ, civ, cop, cop, { boardRead: true }).length) failures.push("dispatch judge failed a good call");
  if (!judge("dispatch", civ, civ, cop, cop.replace("@#LF-Sign_Fuel_Title", "@#LF-Sign_Fuel_Title_Ada"), { boardRead: true }).length) failures.push("dispatch judge passed a call naming the robber");
  if (!judge("board", civ, civ, civ, civ, { officerBoard: "No one is wanted right now.", publicBoard: "WANTED" }).length) failures.push("board judge passed an empty police board");
  if (judge("board", civ, civ, civ, civ, { officerBoard: "WANTED - police notices Robbery", publicBoard: "WANTED" }).length) failures.push("board judge failed good boards");
  if (judge("ticket", civ, civ.replace("cash=500", "cash=450") + " tickets=0", civ, civ, { written: "1", owed: 50 }).length) failures.push("ticket judge failed a paid ticket");
  if (!judge("ticket", civ, civ + " tickets=0", civ, civ, { written: "1", owed: 50 }).length) failures.push("ticket judge passed an unmoved wallet");
  if (judge("stash", kept.replace("stash=1", "stash=0"), kept, civ, civ, { c: 1, stashed: "stash moved=1" }).length) failures.push("stash judge failed a stored apple");
  if (!judge("stash", kept, kept, civ, civ, { c: 1, stashed: "stash moved=0" }).length) failures.push("stash judge passed a refused move");
  if (judge("contraband", civ, civ.replace("cash=500", "cash=700"), civ, civ, { c: 1 }).length) failures.push("contraband judge failed a paid delivery");
  if (!judge("contraband", civ, civ, civ, civ, { c: 1 }).length) failures.push("contraband judge passed no payout");
  if (judge("hideout", civ, civ, civ, civ, { leased: true }).length) failures.push("hideout judge failed a lease");
  const jailed = (sec, x) => `${kept} jail=${sec} pos=${x},0,219`;
  if (judge("arrest", civ.replace("wanted=0", "wanted=1"), jailed(120, 233), civ, cop, {}).length) failures.push("arrest judge failed an arrest");
  if (!judge("arrest", civ.replace("wanted=0", "wanted=1"), civ, civ, cop, {}).length) failures.push("arrest judge passed no sentence");
  if (judge("jail", jailed(120, 233), jailed(114, 233), civ, civ).length) failures.push("jail judge failed a counting sentence");
  if (!judge("jail", jailed(120, 233), jailed(120, 233), civ, civ).length) failures.push("jail judge passed a frozen sentence");
  if (!judge("jail", jailed(120, 233), jailed(114, 300), civ, civ).length) failures.push("jail judge passed a prisoner outside the cell");
  // restart: rank, stash, a stored car and a sentence ride through
  if (!judge("restart", kept, kept.replace("stash=1", "stash=0"), civ, civ).length) failures.push("restart judge passed a lost stash");
  if (!judge("restart", kept, kept.replace("garage=1", "garage=0"), civ, civ).length) failures.push("restart judge passed a lost stored car");
  if (!judge("restart", kept, kept.replace("rank=0", "rank=1"), civ, civ).length) failures.push("restart judge passed a changed rank");
  if (!judge("restart", jailed(100, 233), jailed(0, 233), civ, civ).length) failures.push("restart judge passed a sentence that vanished");
  if (judge("restart", jailed(40, 233), jailed(0, 233), civ, civ, { restoredSentences: parseSentenceRestores("x [LFDebug:Jail] apply-save 00bb remaining=40 serving=1") }).length) failures.push("restart judge failed a sentence the log shows loaded");
  if (parseSentenceRestores("[LFDebug:Jail] apply-save a remaining=12 serving=0").length) failures.push("a finished sentence counts as restored");
  if (judge("restart", jailed(100, 233), jailed(90, 233), civ, civ).length) failures.push("restart judge failed a sentence that kept running");
  if (selectSteps([], "playtest", 3).filter((s) => s.id === "orgc").length !== 1) failures.push("a 3-client run lacks the org-C founding step");
  if (selectSteps([], "playtest", 2).some((s) => s.id === "orgc" || s.clients.includes(3))) failures.push("a 2-client run touches client 3");
  if (selectSteps([], "playtest", 3).find((s) => s.id === "stash").clients.join() !== "3") failures.push("org-C steps do not run on client 3 in a 3-client session");
  if (selectSteps([], "playtest", 2).find((s) => s.id === "stash").clients.join() !== "1") failures.push("org-C steps do not fall back to client 1 in a 2-client session");
  if (selectSteps([], "playtest", 3).find((s) => s.id === "arrive").clients.join() !== "1,2,3") failures.push("arrive skips client 3");
  if (selectSteps([], "debug").some((s) => s.id === "clockin")) failures.push("clockin runs on the bare DebugWorld (no roster)");
  if (!selectSteps([], "playtest").some((s) => s.id === "arrest")) failures.push("arrest is missing from the playtest run");
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

  // --film: a step filmed in-engine links its clip from its row.
  const filmed = [];
  const filmDriver = {
    ...driver,
    beginStep: (s) => filmed.push(`begin ${s.id}`),
    endStep: (s) => {
      filmed.push(`end ${s.id}`);
      return { ok: true, client: 1, out: join(scratch, "agent-box", "reports", "film", "j-atm.mp4"), line: "j-atm.mp4 (15 fps)" };
    },
  };
  const filmRows = runJourney([step("atm")], filmDriver);
  const filmPage = readFileSync(writeJourney(filmRows, scratch, { mission: "debug", when: "now" }), "utf8");
  if (filmed.join() !== "begin atm,end atm") failures.push(`film hooks ran ${filmed.join()}`);
  if (!filmPage.includes('Video (client 1): <a href="film/j-atm.mp4">')) failures.push("a filmed step does not link its clip");
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
  // The journey drives game clients through files in their local profiles, so
  // it runs where the clients run: this PC. A remote pool machine has no
  // desktop session over ssh and runs server-only sessions (session.mjs --machine).
  const mi = argv.indexOf("--machine");
  if (mi !== -1) {
    const want = String(argv[mi + 1] ?? "");
    argv = argv.filter((_, k) => k !== mi && k !== mi + 1);
    if (want !== "local" && want !== "any") {
      console.log(`journey: --machine ${want}: the journey needs game clients, which start only on this PC's desktop; run it with --machine local (or any)`);
      return 2;
    }
  }
  const { readRemotePointer } = await import("./session.mjs");
  const remote = readRemotePointer();
  if (remote) {
    console.log(`journey: the recorded session runs on ${remote.machine} (server only); 'node agent-box/session.mjs down', then 'up' here with clients`);
    return 2;
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
  const joined = (current.clients ?? []).length || 2;
  const steps = selectSteps(argv.filter((a) => !a.startsWith("--")), mission, joined);
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
    serverLog: () => {
      const file = ses.newestLog(join(ses.sessionPaths(dir).server, "logs"));
      return file ? readFileSync(file, "utf8") : "";
    },
    log: (line) => console.log(line),
  };
  // --film records each step inside the game client (tools/film/engine.mjs,
  // LF_FilmRecorder): the client screenshots itself, so it works on the hidden
  // LF-Agents desktop. The filmed client is --film-client (default 1) when the
  // step touches it, else the step's first client. restart closes the clients
  // and is not filmed. The step clips are joined into one mp4 at the end.
  const filmClips = [];
  let filmRun = "";
  if (argv.includes("--film")) {
    const eng = await import("../tools/film/engine.mjs");
    const { stampName } = await import("../tools/film/film.mjs");
    const flag = (name, dflt) => {
      const i = argv.indexOf(name);
      return i === -1 ? dflt : Number(argv[i + 1]);
    };
    const prefer = flag("--film-client", 1);
    const fps = flag("--film-fps", eng.ENGINE_FPS);
    filmRun = stampName("journey");
    let filming = null;
    driver.beginStep = (step) => {
      filming = null;
      if (step.id === "restart") return;
      const client = step.clients.includes(prefer) ? prefer : step.clients[0];
      const name = `journey_${step.id}_${Date.now() % 100000000}`;
      const r = eng.engineStart((spec, t) => driver.send(client, spec, t), name, { fps });
      if (r.ok) filming = { client, name };
      else console.log(`journey: film start on client ${client} failed: ${r.error}`);
    };
    driver.endStep = (step) => {
      if (!filming) return null;
      const { client } = filming;
      filming = null;
      const out = join(ROOT, "agent-box", "reports", "film", `${filmRun}-${step.id}.mp4`);
      const clip = eng.engineStop((spec, t) => driver.send(client, spec, t), (alias) => h.resolveCapture(alias), { out, label: `journey-${step.id}`, fps });
      if (clip.ok) filmClips.push(clip.out);
      return { ...clip, client, line: eng.describe(clip) };
    };
  }
  const rows = runJourney(steps, driver);
  let clip = null;
  if (filmClips.length) {
    const { joinClips } = await import("../tools/film/engine.mjs");
    clip = joinClips(filmClips, join(ROOT, "agent-box", "reports", "film", `${filmRun}.mp4`));
    console.log(clip.ok ? `journey: clip ${clip.out} (${filmClips.length} step clips)` : `journey: joining step clips failed: ${clip.error}`);
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

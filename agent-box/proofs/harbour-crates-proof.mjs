import { readFileSync } from "node:fs";
import { join } from "node:path";
import { arrive, clientBox, newestLog, parseState, sessionDir, sessionPaths } from "../session.mjs";

// The harbour master's crates contract, end to end on the island (#3084), the way a weekend player takes it:
//   node agent-box/session.mjs up --mission island --profile full --wait
//   node agent-box/session.mjs harbour-crates-proof [--drop <entity name of a LF_JobDropoffAction>] [--cmd-timeout ms]
//   node agent-box/session.mjs down
// One client: puts its own TOWN standing at 100 points (Known, tier 1) through the `repgrant` verb, talks to
// Marcel Roux (`harbour_master` dialogue), asks about the crates, takes the contract, reads the tracker, then
// delivers at a drop-off and reads the server's ledger lines (`standing-bonus`) and the standing change.
// PASS or FAIL is printed per step; a FAIL names the PR that should fix it. The standing is the server's, so
// every verdict reads `[LFDebug:...]` lines of the server log, never a client guess.
const MARCEL = "Resident_dock_harbour_master";
const MARCEL_PLACE = "Fit_saint_philippe_harbour_Place";
const TALK = "LF_DialogueTalkAction";
const DROP = "LF_JobDropoffAction";
const CONTRACT = "harbour_crates";
const BASE_PAY = 40; // LF_CivilianJob.PAY_PARCEL
// The island's parcel drops are LF_ParcelDrop mailboxes (Place_saint-philippe_parcel-drops.layer). A run needs the
// drop 300 m or more from where it started (LF_Jobs.conf m_fRunMinDistance); from Marcel's dock cabin the four dock
// drops are all under 100 m, so the one that counts is this district drop, about 510 m away.
const ISLAND_DROP = "Fit_saint_philippe_districts_parcel_drops_drop_b34";

async function runHarbourCratesProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const flag = (name, dflt) => { const i = process.argv.indexOf(name); return i > 0 ? process.argv[i + 1] : dflt; };
  const dropName = flag("--drop", ISLAND_DROP);
  const prefix = flag("--name", "harbour_crates");
  // A loaded pool PC answers a client command in 10+ s; --cmd-timeout raises the floor of every command wait.
  const cmdFloor = Number(flag("--cmd-timeout", "8000"));
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, Math.max(timeout ?? 8000, cmdFloor));
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail.slice(0, 220)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const serverLog = () => {
    const f = newestLog(join(sessionPaths(dir).server, "logs"));
    return f ? readFileSync(f, "utf8") : "";
  };
  const scene = () => on(1, { verb: "dialogue" }, 10000).detail;
  const results = [];
  const step = (name, ok, detail, fixBy) => {
    results.push({ name, ok });
    console.log(`${ok ? "PASS" : "FAIL"}  ${name}${detail ? `: ${detail}` : ""}${!ok && fixBy ? `   -> fix in ${fixBy}` : ""}`);
    return ok;
  };
  const shot = (name) => on(1, { verb: "capture", mode: "ui", name: `${prefix}_${name}` }, 25000);
  // Waits for the replies to show, skipping the NPC's line with confirm the way a player does.
  const untilChoosing = (what) => {
    let state = "";
    for (let i = 0; i < 40; i++) {
      state = scene();
      if (/phase=choosing/.test(state)) return state;
      if (state.startsWith("closed")) return state;
      if (/phase=line/.test(state) && i >= 2) on(1, { verb: "input", action: "MenuSelect" });
      wait(700);
    }
    console.log(`(${what}: replies never showed, last state: ${state})`);
    return state;
  };

  arrive(on, 1, wait);
  on(1, { verb: "input", action: "MenuBack" });
  wait(6000);
  on(1, { verb: "resetplayer", cash: 1000 }, 10000);
  wait(1500);

  // 1. Known standing through the harness verb (the server's dev gate decides).
  on(1, { verb: "repgrant", words: ["TOWN", "100"] }, 10000);
  wait(1500);
  let log = serverLog();
  const grant = /harness standing (\S+) TOWN points=(\d+) tier=(-?\d+)/.exec(log);
  const uid = grant ? grant[1] : "";
  step("1 the character is Known (TOWN 100 points, tier 1)", !!grant && grant[2] === "100" && grant[3] === "1", grant ? `points=${grant[2]} tier=${grant[3]}` : "no `harness standing` line in the server log", "the harness verb (this card)");
  if (!uid) return finish(results);

  // 2. Marcel is on the island and answers.
  // Marcel is a resident (#4466, #4646): he spawns on his timetable when a player is within 150 m of the harbour place, so the
  // player goes to the place marker first and the clock is set inside his 06:00 to 18:00 day.
  on(1, { verb: "stand", entityName: MARCEL_PLACE, side: "s" }, 20000);
  wait(2000);
  on(1, { verb: "time", hour: 10.0 }, 15000);
  wait(10000);
  const stand = on(1, { verb: "stand", entityName: MARCEL, side: "s" }, 10000);
  wait(2500);
  const talk = on(1, { verb: "use", className: TALK, entityName: MARCEL }, 10000);
  wait(800);
  const greet = scene();
  step("2 Marcel Roux stands at the dock and the talk opens", stand.status === "ok" && talk.status === "ok" && /open node=greet/.test(greet), `stand=${stand.status} talk=${talk.status} scene=${greet}`, "#3058 (the harbour master in the dock cabin)");
  if (!/open node=greet/.test(greet)) return finish(results);
  shot("greeting");

  // 3. The crates reply is offered (the 2nd of work / crates / bye) and opens the offer, not the "come back later" line.
  const replies = untilChoosing("greeting");
  // `replies` counts the work and crates rows; Goodbye is the separate always-present 0. row (LF_DialogueScene.ShowRows).
  step("3 the greeting offers the work and crates replies (plus Goodbye)", /replies=2/.test(replies), replies, "#3054 (contract offer dialogue)");
  shot("replies");
  // The focus starts on the first reply; one Down moves it to the crates reply. The keys are spaced out: a Down
  // sent while the capture is still settling is dropped and the confirm then takes the work reply (seen once).
  wait(1000);
  on(1, { verb: "input", action: "MenuDown" });
  wait(900);
  on(1, { verb: "input", action: "MenuSelect" });
  wait(1200);
  const offer = scene();
  step("4 the crates reply leads to the offer line (not the too-early line)", /node=crates_offer/.test(offer), offer, /node=crates_early/.test(offer) ? "#3090 / the standing gate (a Known character was refused)" : "#3054 (contract offer dialogue)");

  // 4. Take the contract: reply 1 of the offer.
  const choosing = untilChoosing("offer");
  const beforeTake = serverLog().length;
  on(1, { verb: "input", action: "MenuSelect" });
  wait(3000);
  log = serverLog();
  const taken = new RegExp(`contract ${CONTRACT} taken by ${uid}`).test(log.slice(beforeTake));
  const started = new RegExp(`dialogue contract ${CONTRACT} started by ${uid}`).test(log.slice(beforeTake));
  step("5 the take reply starts the run and binds the contract", taken && started, taken ? "taken and started" : `no 'contract ${CONTRACT} taken' line after the reply (replies were: ${choosing})`, "#3054 (contract offer dialogue)");
  shot("taken");
  on(1, { verb: "input", action: "MenuBack" });
  wait(2500);

  // 5. The tracker shows the contract and the time left.
  let text = "";
  for (let i = 0; i < 10; i++) {
    text = on(1, { verb: "ui", mode: "text" }, 10000).detail;
    if (/Harbou?r crates/.test(text)) break; // US English title since 2026-11-22
    wait(700);
  }
  const timeWord = /(plenty of time|time is getting on|hurry)/.exec(text);
  step("6 the tracker shows the contract title and the time left", /Harbou?r crates/.test(text) && !!timeWord, timeWord ? `'${timeWord[1]}'` : `ui text: ${text.slice(0, 200)}`, "#2914 tracker (LF_ContractTracker)");
  shot("tracker");

  // 6. The run's drop: an LF_ParcelDrop mailbox 300 m or more from the dock (ISLAND_DROP, or --drop <name>).
  const dropOk = dropName
    ? on(1, { verb: "stand", entityName: dropName, side: "s" }, 10000).status === "ok"
    : false;
  step("7 the run has a drop-off to walk to on the island", dropOk, `at ${dropName}`, "the island parcel drops (Place_saint-philippe_parcel-drops.layer): none 300 m or more from the dock");
  if (!dropOk) return finish(results);
  // The run needs a minimum distance and time before it pays (DropoffRefusal); wait out the time share.
  wait(Number(flag("--run-seconds", "45")) * 1000);
  const cashBefore = Number(parseState(on(1, { verb: "state" }, 10000).detail).cash);
  const mark = serverLog().length;
  on(1, { verb: "use", className: DROP, entityName: dropName }, 10000);
  wait(3500);
  const after = serverLog().slice(mark);
  const settled = new RegExp(`contract ${CONTRACT} finished by ${uid}: pay (\\d+) \\(base (\\d+), bonus (\\d+)\\), award (\\d+) TOWN`).exec(after);
  step("8 the delivery settles the contract", !!settled, settled ? `pay ${settled[1]} (base ${settled[2]}, bonus ${settled[3]}), award ${settled[4]}` : `no 'contract ${CONTRACT} finished' line; drop-off said: ${(/drop-off refused[^\n]*/.exec(after) ?? [""])[0]}`, "the run rules (LF_CivilianJob.DropoffRefusal)");
  if (!settled) return finish(results);
  const base = Number(settled[2]);
  step("9 the pay is x1.4 on the base run", Number(settled[1]) === Math.round(base * 1.4) && base === BASE_PAY, `${settled[1]} for base ${base}`, "#3090 (parcel pacing) or the contract row");
  const bonusLine = new RegExp(`#\\d+ ${uid} \\S+ (\\+?\\d+) \\(standing-bonus\\)`).exec(after);
  step("10 the ledger books the bonus as `standing-bonus`", !!bonusLine && Number(bonusLine[1]) >= Math.round(base * 0.4), bonusLine ? `+${bonusLine[1]}` : "no standing-bonus ledger line", "LF_CivilianJob.Dropoff / LF_ReputationPay");
  const standing = new RegExp(`${uid} TOWN (-?\\d+) -> (-?\\d+) \\(contract:${CONTRACT}\\)`).exec(after);
  const moved = standing ? Number(standing[2]) - Number(standing[1]) : 0;
  step("11 the standing moves +12 for the contract", moved === 12, standing ? `${standing[1]} -> ${standing[2]} (${moved >= 0 ? "+" : ""}${moved})` : `no 'contract:${CONTRACT}' standing line (the daily cap may have cut it: ${(/standing unchanged[^\n]*/.exec(after) ?? [""])[0]})`, "#3090 (TOWN daily cap 120 against the +10 run award)");
  const cashAfter = Number(parseState(on(1, { verb: "state" }, 10000).detail).cash);
  console.log(`cash ${cashBefore} -> ${cashAfter}`);
  return finish(results);
}

function finish(results) {
  const failed = results.filter((r) => !r.ok);
  console.log(`harbour crates proof: ${failed.length ? `FAIL (${failed.map((r) => r.name.split(" ")[0]).join(", ")})` : "PASS"} - ${results.length - failed.length}/${results.length} steps`);
  return failed.length ? 1 : 0;
}

export default {
  name: "harbour-crates-proof",
  usage: "harbour-crates-proof [--drop <LF_JobDropoffAction entity name>] [--cmd-timeout ms] [--run-seconds N] [--name <shot prefix>]   (up --mission island --profile full)",
  run: () => runHarbourCratesProof(),
};

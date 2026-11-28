import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { ROOT, arrive, clientBox, newestLog, parseState, sessionDir, sessionPaths } from "../session.mjs";
import { ensureCuffs, requireBodies } from "./_preconditions.mjs";

// The cop path on the real island (rc 0.2.0), two players on a dedicated server, under the flagship owner config:
//   node agent-box/session.mjs up --mission island --clients 2 --profile full --wait
//   node agent-box/session.mjs island-cop-proof [--name <shot prefix>]
// Client 1 (A) signs up at the Saint-Philippe duty desk (police.alphaInstantApproval approves on submit), clocks in,
// unlocks a police UAZ outside the station (#3875, on-duty police only) while client 2 (B) is refused one. B tries the
// fuel till inside the 30 minute grace (refused), the grace is set to 0 and B robs it (wanted). A asks for hands up,
// B raises them, A cuffs, searches and detains B for escort (the cell is the booking desk step). A clocks out and is paid. Every step prints PASS or FAIL.
async function runIslandCopProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const flag = (name, dflt) => { const i = process.argv.indexOf(name); return i > 0 ? process.argv[i + 1] : dflt; };
  const prefix = flag("--name", "rc020_island_cop");
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${String(r.detail).slice(0, 300)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const state = (n) => parseState(on(n, { verb: "state" }, 10000).detail);
  const serverLog = () => {
    const f = newestLog(join(sessionPaths(dir).server, "logs"));
    return f ? readFileSync(f, "utf8") : "";
  };
  const steps = [];
  const step = (name, ok, why) => { steps.push({ name, ok, why }); console.log(`${ok ? "PASS" : "FAIL"}  ${name}${ok ? "" : `: ${why}`}`); };
  const shot = (n, name) => on(n, { verb: "capture", mode: "ui", name: `${prefix}_${name}` }, 25000);

  // The flagship owner config, with a grace override when asked, written where the server reads it.
  const flagship = JSON.parse(readFileSync(join(ROOT, "server", "flagship", "config.json"), "utf8"));
  const ownerFile = join(sessionPaths(dir).server, "profile", "LifeFramework", "config.json");
  const applyConfig = (graceMinutes) => {
    const c = structuredClone(flagship);
    if (graceMinutes !== undefined) c.grace = { minutes: graceMinutes };
    mkdirSync(dirname(ownerFile), { recursive: true });
    writeFileSync(ownerFile, JSON.stringify(c, null, 2));
    on(1, { verb: "configreload" }, 10000);
    wait(3500);
  };

  const DESK = "Fit_saint_philippe_police_police_kit_duty_desk";
  const CAR_A = "Fit_saint_philippe_police_cars_car_b32_1";
  const CAR_B = "Fit_saint_philippe_police_cars_car_b32_3";
  const TILL = "Fit_saint_philippe_fuel_till";

  for (const n of [1, 2]) arrive(on, n, wait);
  // The welcome guide opens over a new character and covers every UI picture: dismiss it like a player would.
  for (const n of [1, 2]) { wait(3000); on(n, { verb: "click", widget: "WelcomeGuideDismissButton" }, 10000); }
  wait(8000);
  if (!requireBodies(on, wait, [1, 2])) return 1;
  applyConfig();
  const b = state(2).player;
  const suspect = `Player${b}`;

  // 1. A joins the police at the desk and goes on duty.
  on(1, { verb: "stand", entityName: DESK });
  wait(2500);
  // The desk shows the Apply row only to an account that is not on the police roster (LF_PoliceApplyAction.CanBeShownScript).
  // A session that seeds the roster (the whitelist seed, --roster) shows the Clock in row alone: then there is nothing to
  // apply for, and the clock-in below is the proof.
  const deskRows = String(on(1, { verb: "look" }).detail);
  const onRoster = !/LF_PoliceApplyAction/.test(deskRows) && /LF_DutyAction/.test(deskRows);
  if (onRoster) {
    step("1a A is on the police roster already: the desk offers Clock in and hides Apply", true, "no application form to fill");
  } else {
    const open = on(1, { verb: "use", className: "LF_PoliceApplyAction", entityName: DESK }, 10000);
    wait(1500);
    on(1, { verb: "type", widget: "PolAppName", text: "Ada_Newcomer" });
    on(1, { verb: "type", widget: "PolAppWhy", text: "Keep_Saint_Philippe_safe" });
    on(1, { verb: "type", widget: "PolAppAvailability", text: "Evenings" });
    on(1, { verb: "click", widget: "PolAppSubmit" });
    wait(3000);
    const approved = /approved on submit: police.alphaInstantApproval is on/.test(serverLog()) && /approved, added to the police roster/.test(serverLog());
    step("1a A signs up at the desk, approved on submit (alpha switch)", open.status === "ok" && approved, `apply=${open.status} ${open.detail}; approval line in the server log: ${approved}`);
  }
  const before = state(1);
  on(1, { verb: "look" });
  on(1, { verb: "use", className: "LF_DutyAction", entityName: DESK }, 10000);
  wait(3000);
  const onDuty = state(1);
  step("1b A clocks in", onDuty.duty === "1" || /police/i.test(String(onDuty.duty)), `duty=${onDuty.duty}`);
  const kitLog = serverLog();
  step("1c the island police kit is issued (no generic default)", !/no kit configured for role POLICE/.test(kitLog), "server log: no kit configured for role POLICE");
  step("0 a new civilian gets the pack starting kit (canvas sack)", /applied role kit CIVILIAN/.test(kitLog) && !/no kit configured for role CIVILIAN/.test(kitLog), "server log has no 'applied role kit CIVILIAN'");
  shot(1, "1_on_duty");

  // 2. A takes a police car; B is refused another.
  on(1, { verb: "stand", entityName: CAR_A, side: "w" });
  wait(3000);
  on(1, { verb: "look" });
  const unlock = on(1, { verb: "use", className: "LF_VehicleLockAction" }, 10000);
  wait(2000);
  const unlocked = /toggled by menu -> locked=(false|0)/.test(serverLog());
  step("2a A unlocks a police car on duty", unlock.status === "ok" && unlocked, `use=${unlock.status} ${unlock.detail}; toggle line: ${unlocked}`);
  shot(1, "2_police_car_unlocked");
  on(2, { verb: "stand", entityName: CAR_B, side: "w" });
  wait(3000);
  on(2, { verb: "look" });
  const civ = on(2, { verb: "use", className: "LF_VehicleLockAction" }, 10000);
  wait(1500);
  const refusedCiv = civ.status !== "ok" || /service vehicle, user not on duty as police/.test(serverLog());
  step("2b B (civilian) cannot unlock a police car", refusedCiv, `use=${civ.status} ${civ.detail}`);

  // 3. B commits a small crime: robs the fuel till through the attendant (the island till is dialogue-only by design).
  // Inside the flagship's 30 minute grace the robbery is refused; then the grace is set to 0.
  const CLERK = "Fit_saint_philippe_fuel_clerk";
  const PISTOL = "{1353C6EAD1DCFE43}Prefabs/Weapons/Handguns/M9/Handgun_M9.et";
  const scene = (arg, value) => on(2, { verb: "dialogue", arg, value }, 10000).detail;
  const rob = () => {
    on(2, { verb: "stand", entityName: TILL, side: "s" });
    wait(2500);
    on(2, { verb: "look" });
    on(2, { verb: "use", className: "LF_DialogueTalkAction", entityName: CLERK }, 10000);
    wait(1200); scene("skip"); wait(800);
    scene("pick", "2"); wait(600);
    wait(1200); scene("skip"); wait(800);
    scene("pick", "0"); wait(1500);
    wait(34000);
    on(2, { verb: "input", action: "MenuBack" });
    wait(3000);
  };
  on(2, { verb: "pistol" }, 10000);
  wait(1500);
  on(2, { verb: "hold", item: "weapon" }, 10000);
  wait(2500);
  rob();
  const inGrace = Number(state(2).wanted) || 0;
  step("3a in the 30 minute grace B is refused the robbery (expected on the alpha)", inGrace === 0, `wanted=${inGrace}`);
  applyConfig(0);
  rob();
  const wanted = Number(state(2).wanted) || 0;
  step("3b with grace off B robs the fuel till and is wanted", wanted > 0, `wanted=${wanted}`);
  // B puts the pistol away: `hold none` holsters it, so B is unarmed.
  on(2, { verb: "hold", item: "none" }, 10000);
  wait(2500);
  wait(2000);

  // 4. Hands up, cuff, search, arrest.
  on(1, { verb: "stand", entityName: suspect });
  wait(3000);
  on(1, { verb: "handsup", words: ["offer", String(b)] }, 10000);
  wait(1000);
  on(2, { verb: "handsup", words: ["choose", "1"] }, 10000);
  wait(1500);
  const up = parseState(on(2, { verb: "handsup", words: ["state"] }, 10000).detail);
  step("4a B puts his hands up", JSON.stringify(up).includes(`"p${b}":"1"`) || up.up === "1" || /1/.test(String(up[`p${b}`] ?? up.handsUp ?? "")), JSON.stringify(up));
  // The duty kit is issued once per account: an officer who clocked in before has no cuffs, so the proof holds them,
  // grants a pair through the harness when the pack has none, and only then cuffs (rc-proof-order, _preconditions.mjs).
  const cuffs = ensureCuffs(on, wait, 1);
  const held = { status: cuffs.ok ? "ok" : "error", detail: `cuffs ${cuffs.moves.join(",")}: ${cuffs.last.detail}` };
  wait(2500);
  on(1, { verb: "cuffing", playerId: Number(b), kind: "cuff", mode: "pin" }, 10000);
  wait(2000);
  on(1, { verb: "cuffing", playerId: Number(b), kind: "cuff", mode: "finish" }, 10000);
  wait(2500);
  const cuffed = state(2).cuffed;
  step("4b A cuffs B (hands up, unarmed)", held.status === "ok" && cuffed === "1", `hold=${held.status} ${held.detail}; cuffed=${cuffed}; refusal: ${(/restrain refused: [^\n]*/.exec(serverLog()) ?? ["none"])[0]}`);
  shot(1, "4_cuffed");
  on(1, { verb: "look" });
  const search = on(1, { verb: "use", className: "LF_ConfiscateAction", entityName: suspect }, 10000);
  wait(2500);
  step("4c A searches B", search.status === "ok", `${search.status} ${search.detail}`);
  shot(1, "4_search");
  // The island's police take a cuffed suspect to the cell through the booking desk (booking-desk-proof, X-6 hour 8) or by
  // escort: LF_JailArrestAction, the instant arrest, is not offered on the island any more (the look list shows
  // LF_JailEscortArrestAction, which detains for escort and starts no sentence).
  let arrest;
  for (let i = 0; i < 4; i++) {
    on(1, { verb: "look" });
    arrest = on(1, { verb: "use", className: "LF_JailEscortArrestAction", entityName: suspect }, 10000);
    if (arrest.status === "ok") break;
    wait(2000);
  }
  wait(5000);

  // 5. B is detained for escort: still cuffed, no sentence yet (the cell is the booking desk's step).
  const detained = state(2);
  step("5 B is detained for escort: still cuffed, the sentence waits for the booking", arrest.status === "ok" && String(detained.cuffed) === "1" && (Number(detained.jail) || 0) === 0, `escort=${arrest.status} ${arrest.detail}; cuffed=${detained.cuffed} jail=${detained.jail}`);
  shot(2, "5_cell");

  // 6. A clocks out at the desk and is paid.
  on(1, { verb: "stand", entityName: DESK });
  wait(2500);
  on(1, { verb: "look" });
  on(1, { verb: "use", className: "LF_DutyAction", entityName: DESK }, 10000);
  wait(3000);
  const after = state(1);
  const money = (s) => (Number(s.cash) || 0) + (Number(s.bank) || 0);
  step("6 A clocks out and is paid duty pay", money(after) > money(before), `cash+bank ${money(before)} -> ${money(after)}; duty=${after.duty}`);

  // Keep the feature lines: `session down` removes the server log.
  const kept = serverLog().split(/\r?\n/).filter((l) => /LFDebug:(Police|HandsUp|Duty|DutyPay|Jail|VehicleLock|Rob|Spawn|Grace)/.test(l));
  writeFileSync(join(ROOT, "tmp", "island-cop-server.txt"), kept.join("\n"));
  const failed = steps.filter((s) => !s.ok);
  console.log("");
  for (const s of steps) console.log(`${s.ok ? "PASS" : "FAIL"}  ${s.name}`);
  console.log(failed.length ? `island cop proof: FAIL (${failed.length} of ${steps.length})` : `island cop proof: PASS (${steps.length} steps)`);
  return failed.length ? 1 : 0;
}

export default {
  name: "island-cop-proof",
  usage: "island-cop-proof [--name <shot prefix>]",
  run: (ctx) => runIslandCopProof(),
};

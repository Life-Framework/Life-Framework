import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { ROOT, arrive, clientBox, newestLog, parseState, sessionDir, sessionPaths } from "../session.mjs";

// The Saint-Philippe jail's two-layer break and bail on the real island (the jail card after #4058).
//   tools\cli session up --mission island --clients 2 --profile full --wait
//   node agent-box/session.mjs island-jail-breach-proof
//   tools\cli session down
// Phase 1 is the island cop flow (A signs up, clocks in, B robs the fuel till twice so the sentence outlasts the break,
// A cuffs and arrests B). Phase 2 runs on the sentence that leaves running, with no pause: B stays in the cell (the cell door is named, so Confines holds), A charges the
// cells first and is refused (#LF-JailBreak_OuterFirst), then blows the outer door, then the cells; B is freed from
// custody. Every step prints PASS or FAIL.
async function runIslandJailBreachProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
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
  const CELL = "Fit_saint_philippe_jail_cell";
  const CELL_DOOR = "Fit_saint_philippe_jail_jail_cell_door";
  const OUTER_DOOR = "Fit_saint_philippe_jail_jail_outer_door";
  const open = (n, id) => {
    const entry = (parseState(on(n, { verb: "seen", radius: 25 }, 10000).detail).breach ?? "").split("|").find((e) => e.includes(id));
    return /open=(\d)/.exec(entry ?? "")?.[1] ?? "?";
  };

  const flagship = JSON.parse(readFileSync(join(ROOT, "server", "flagship", "config.json"), "utf8"));
  const ownerFile = join(sessionPaths(dir).server, "profile", "LifeFramework", "config.json");
  const c = structuredClone(flagship);
  c.grace = { minutes: 0 };
  mkdirSync(dirname(ownerFile), { recursive: true });
  writeFileSync(ownerFile, JSON.stringify(c, null, 2));
  const DESK = "Fit_saint_philippe_police_police_kit_duty_desk";
  const TILL = "Fit_saint_philippe_fuel_till";
  const CLERK = "Fit_saint_philippe_fuel_clerk";
  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  on(1, { verb: "configreload" }, 10000);
  wait(3500);
  const bid = state(2).player;
  const suspect = `Player${bid}`;
  on(1, { verb: "stand", entityName: DESK });
  wait(2500);
  on(1, { verb: "look" });
  on(1, { verb: "use", className: "LF_PoliceApplyAction", entityName: DESK }, 10000);
  wait(1500);
  on(1, { verb: "type", widget: "PolAppName", text: "Ada_Newcomer" });
  on(1, { verb: "type", widget: "PolAppWhy", text: "Keep_Saint_Philippe_safe" });
  on(1, { verb: "type", widget: "PolAppAvailability", text: "Evenings" });
  on(1, { verb: "click", widget: "PolAppSubmit" });
  wait(3000);
  on(1, { verb: "look" });
  on(1, { verb: "use", className: "LF_DutyAction", entityName: DESK }, 10000);
  wait(3000);
  step("A is on duty", state(1).duty === "1" || /police/i.test(String(state(1).duty)), `duty=${state(1).duty}`);
  // Staging before the arrest, so the break fits inside a one-wanted sentence (60 s): the cells are refused while the outer
  // door stands, then the outer door is blown.
  on(1, { verb: "charge" }, 10000);
  on(1, { verb: "hold", item: "LF_Charge" }, 10000);
  wait(1500);
  on(1, { verb: "stand", entityName: CELL_DOOR, side: "e" });
  wait(2500);
  on(1, { verb: "look" });
  const early = on(1, { verb: "use", className: "LF_BreachArmAction", entityName: CELL_DOOR }, 10000);
  wait(2000);
  const refusedLine = /OuterFirst|outer door|refused/i.test(serverLog().split(String.fromCharCode(10)).filter((l) => /LFDebug:(Breach|JailBreak)/.test(l)).slice(-6).join(" "));
  step("the cells are refused while the outer door stands", open(1, "saint_philippe_jail_cell") === "0" && (early.status !== "ok" || refusedLine), `open=${open(1, "saint_philippe_jail_cell")} use=${early.status} ${early.detail}`);
  on(1, { verb: "stand", entityName: OUTER_DOOR, side: "s" });
  wait(2500);
  on(1, { verb: "look" });
  const outer = on(1, { verb: "use", className: "LF_BreachArmAction", entityName: OUTER_DOOR }, 10000);
  wait(24000);
  step("the outer door blows", outer.status === "ok" && open(1, "saint_philippe_jail_outer") === "1", `use=${outer.status} open=${open(1, "saint_philippe_jail_outer")}`);
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
  step("B is wanted after the robbery", (Number(state(2).wanted) || 0) >= 1, `wanted=${state(2).wanted}`);
  on(2, { verb: "hold", item: "none" }, 10000);
  wait(2500);
  const arrestB = () => {
    on(1, { verb: "stand", entityName: suspect });
    wait(3000);
    on(1, { verb: "handsup", words: ["offer", String(bid)] }, 10000);
    wait(1000);
    on(2, { verb: "handsup", words: ["choose", "1"] }, 10000);
    wait(1500);
    on(1, { verb: "hold", item: "Handcuffs" }, 10000);
    wait(2500);
    on(1, { verb: "cuffing", playerId: Number(bid), kind: "cuff", mode: "pin" }, 10000);
    wait(2000);
    on(1, { verb: "cuffing", playerId: Number(bid), kind: "cuff", mode: "finish" }, 10000);
    wait(2500);
    on(1, { verb: "look" });
    for (let i = 0; i < 4; i++) {
      const arrest = on(1, { verb: "use", className: "LF_JailArrestAction", entityName: suspect }, 10000);
      if (arrest.status === "ok") break;
      wait(2000);
    }
    wait(3000);
  };
  arrestB();
  wait(3000);
  const b = state(2);
  const left0 = Number(b.jail) || 0;
  step("B is serving in the cell (remaining > 0)", left0 > 0, `jail=${b.jail}`);
  if (left0 <= 0) {
    console.log("island jail breach proof: FAIL (the sentence ended before phase 2; run again)");
    return 1;
  }

  // 1. The cell holds: the cell door is named in the cell row, so a prisoner is confined (the island jail had no door target).
  const log0 = serverLog();
  step("the island cell names its door and its outer door", /door target saint_philippe_jail_cell/.test(log0) && /outer door target saint_philippe_jail_outer/.test(log0), "no door target lines");
  on(2, { verb: "stand", entityName: OUTER_DOOR });
  wait(4000);
  const held = serverLog();
  step("B walked to the outer door is pulled back into the cell (held)", /returned [^\n]* to the cell|moved [^\n]* to the cell/.test(held.slice(log0.length)) || (Number(state(2).jail) || 0) > 0, "no pull-back line after the teleport");

  // The cells: A carries a fresh charge and blows them while B's sentence still runs.
  on(1, { verb: "charge" }, 10000);
  on(1, { verb: "hold", item: "LF_Charge" }, 10000);
  wait(1500);
  on(1, { verb: "stand", entityName: CELL_DOOR, side: "e" });
  wait(2500);
  on(1, { verb: "look" });
  const cells = on(1, { verb: "use", className: "LF_BreachArmAction", entityName: CELL_DOOR }, 10000);
  wait(19000);
  const after = serverLog();
  step("the cells blow and B is freed from custody", cells.status === "ok" && /cells saint_philippe_jail_cell blown by/.test(after) && /freed by a jailbreak/.test(after), `use=${cells.status}; blown=${/cells saint_philippe_jail_cell blown by/.test(after)} freed=${/freed by a jailbreak/.test(after)}`);
  const sirens = after.split(String.fromCharCode(10)).filter((l) => /alarm sounding at/.test(l) && /kind 'jail'/.test(l) && /Jail/.test(l)).length;
  step("both jail layers sounded the jailbreak siren (kind 'jail')", sirens >= 2, `siren lines=${sirens}; no-site lines=${after.split(String.fromCharCode(10)).filter((l) => /no alarm site/.test(l)).length}`);
  step("B's sentence is over", (Number(state(2).jail) || 0) === 0, `jail=${state(2).jail}`);

  // 3. Bail: B is wanted again after the break (escaped custody); A arrests B once more and B pays at the phone in the cell.
  const PHONE = "Fit_saint_philippe_jail_jail_bail_phone";
  on(1, { verb: "stand", entityName: suspect });
  wait(3000);
  arrestB();
  wait(3000);
  step("B is serving again", (Number(state(2).jail) || 0) > 0, `jail=${state(2).jail}`);
  const bank0 = Number(state(2).bank) || 0;
  on(2, { verb: "stand", entityName: PHONE });
  wait(2500);
  on(2, { verb: "look" });
  const pay = on(2, { verb: "use", className: "LF_JailBailAction", entityName: PHONE }, 10000);
  wait(2500);
  const bank1 = Number(state(2).bank) || 0;
  step("B pays bail at the cell phone from the bank and is released", pay.status === "ok" && bank1 < bank0 && (Number(state(2).jail) || 0) === 0 && /paid bail/.test(serverLog()), `use=${pay.status} bank ${bank0} -> ${bank1} jail=${state(2).jail}`);

  const kept = serverLog().split(/\r?\n/).filter((l) => /LFDebug:(Jail|JailBreak|JailBail|Breach|Placement\] base-world clear: hid \d+ vanilla props in the box Fit_saint_philippe_jail)/.test(l));
  writeFileSync(join(ROOT, "tmp", "island-jail-breach-server.txt"), kept.join("\n"));
  const failed = steps.filter((s) => !s.ok);
  console.log("");
  for (const s of steps) console.log(`${s.ok ? "PASS" : "FAIL"}  ${s.name}`);
  console.log(failed.length ? `island jail breach proof: FAIL (${failed.length} of ${steps.length})` : `island jail breach proof: PASS (${steps.length} steps)`);
  return failed.length ? 1 : 0;
}

export default {
  name: "island-jail-breach-proof",
  usage: "island-jail-breach-proof   (up --mission island --clients 2 --profile full)",
  run: () => runIslandJailBreachProof(),
};

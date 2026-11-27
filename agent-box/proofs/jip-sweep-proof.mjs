import { spawnSync } from "node:child_process";
import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { ROOT, arrive, clientBox, newestLog, parseState, sessionDir, sessionPaths } from "../session.mjs";
import { ensureCuffs } from "./_preconditions.mjs";

// Late-joiner sweep before the 0.3.0 RC (2026-11-27, docs/testing/jip-sweep-2026-11-27.md), on the real island on a
// dedicated server:
//   node agent-box/session.mjs up --mission island --clients 2 --profile lowspec --wait
//   node agent-box/session.mjs jip-sweep-proof
//   node agent-box/session.mjs down
// Client 1 (A, civilian) buys a car (it comes out of the garage locked), buys a house, opens a door; client 2 (B) joins
// the police and cuffs A; the clock is set to night. Then client 3 joins late and stands where A's state is. Each
// thing a player would notice is read on client 1 (there all along) and on client 3 (the late joiner) through the
// harness `seen`, `peers` and `perfstress count` verbs, which read THIS client's replicated copy. A row passes when the
// late joiner reads what client 1 reads.
//
//   jip-sweep-proof --selftest      the token parsers on made-up lines, no game
const FIT = "Fit_saint_philippe_";
const GARAGE = `${FIT}carlot_garage`;
const LOT = `${FIT}carlot_vehicle_lot`;
const DESK = `${FIT}police_police_kit_duty_desk`;
const SIGN = "HomeSign_saint_philippe_sp_b01";

// One token's value from a `seen` line (`veh=a|b doors=...`): the text up to the next space.
export function token(line, name) {
  const m = new RegExp(`(?:^|\\s)${name}=(\\S*)`).exec(String(line ?? ""));
  return m ? m[1] : null;
}

// The first entry of a list token whose prefab part contains `like`, or the first entry.
export function entry(list, like = "") {
  if (!list || list === "none") return null;
  const all = list.split("|");
  return all.find((e) => e.includes(like)) ?? (like ? null : all[0]);
}

// A field of one entry (`S105:d=3:lock=1:plate=AB-12` -> field(e, "lock") = "1").
export function field(e, name) {
  const m = new RegExp(`(?:^|:)${name}=([^:|]*)`).exec(String(e ?? ""));
  return m ? m[1] : null;
}

// The peers entry for one player id, as a map of its key=value words.
export function peer(line, playerId) {
  for (const part of String(line ?? "").split(" | ")) {
    const words = Object.fromEntries(part.trim().split(/\s+/).map((w) => w.split("=")).filter((kv) => kv.length === 2));
    if (words.player === String(playerId)) return words;
  }
  return null;
}

// `perfstress count` reply: the lights-on number.
export function lightsOn(detail) {
  const m = /lights(?:_|\s)?on[=:\s](\d+)/i.exec(String(detail ?? "")) ?? /on=(\d+)/.exec(String(detail ?? ""));
  return m ? Number(m[1]) : null;
}

function selftest() {
  const f = [];
  const line = "me=CIV/0/0 cash=5 veh=LF_S105_Red:d=3:eng=0:lock=1:ign=1:stolen=0:plate=EV-123|UAZ:d=9:eng=0:lock=0:ign=1:stolen=0:plate=- doors=Door_A:d=2:state=0.98 housesign=saint-philippe.home.sp-b01:sold=1 house=x:owned=1:locked=0 siteflag=none hideout=none";
  const veh = entry(token(line, "veh"), "S105");
  if (field(veh, "plate") !== "EV-123" || field(veh, "lock") !== "1") f.push("veh fields");
  if (field(token(line, "housesign"), "sold") !== "1") f.push("housesign sold");
  if (field(entry(token(line, "doors")), "state") !== "0.98") f.push("door state");
  if (token(line, "siteflag") !== "none") f.push("siteflag none");
  const p = peer("player=1 downed=0 life=ALIVE carrier=0 seat=0 at=1,2,3 cuffed=1 worn=1 ragdoll=0 stance=STAND | player=2 downed=0 cuffed=0 worn=0", 1);
  if (!p || p.cuffed !== "1" || p.worn !== "1") f.push("peer cuffed/worn");
  if (lightsOn("entities=10 static=4 lights=6 lightsOn=3 shadows=1") !== 3) f.push("lights on");
  for (const x of f) console.log(`FAIL selftest: ${x}`);
  if (!f.length) console.log("jip-sweep-proof selftest ok");
  return f.length ? 1 : 0;
}

async function runJipSweep() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${String(r.detail).slice(0, 1200)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const state = (n) => parseState(on(n, { verb: "state" }, 10000).detail);
  const serverLog = () => {
    const f = newestLog(join(sessionPaths(dir).server, "logs"));
    return f ? readFileSync(f, "utf8") : "";
  };
  const rows = [];
  const row = (system, did, early, late, ok) => {
    rows.push({ system, did, early, late, ok });
    console.log(`${ok ? "PASS" : "FAIL"}  ${system}: client 1 ${early} / late joiner ${late}`);
  };

  const flagship = JSON.parse(readFileSync(join(ROOT, "server", "flagship", "config.json"), "utf8"));
  const ownerFile = join(sessionPaths(dir).server, "profile", "LifeFramework", "config.json");
  mkdirSync(dirname(ownerFile), { recursive: true });

  for (const n of [1, 2]) arrive(on, n, wait);
  for (const n of [1, 2]) { wait(3000); on(n, { verb: "click", widget: "WelcomeGuideDismissButton" }, 10000); }
  wait(8000);
  writeFileSync(ownerFile, JSON.stringify(flagship, null, 2));
  on(1, { verb: "configreload" }, 10000);
  wait(3500);
  const a = state(1).player;

  // A: money for a car and a house, a licence, the car (out of the garage it comes locked).
  on(1, { verb: "resetplayer", cash: 9000, keepInventory: true }, 10000);
  wait(1500);
  on(1, { verb: "licence", name: "DRIVING_LICENCE" }, 10000);
  wait(1500);
  on(1, { verb: "stand", entityName: LOT });
  wait(1500);
  on(1, { verb: "use", className: "LF_VehicleBuyAction", entityName: LOT }, 10000);
  wait(1500);
  on(1, { verb: "click", widget: "Car1_Action" });
  wait(2500);
  on(1, { verb: "click", widget: "LotClose" });
  wait(1000);
  on(1, { verb: "park" }, 10000);
  on(1, { verb: "stand", entityName: GARAGE, side: "e5" });
  wait(2500);
  on(1, { verb: "use", className: "LF_VehicleStoreAction", entityName: GARAGE }, 10000);
  wait(3000);
  on(1, { verb: "use", className: "LF_VehicleRetrieveAction", entityName: GARAGE }, 10000);
  wait(3000);

  // A buys the house behind sign sp-b01 and opens the nearest door there.
  on(1, { verb: "stand", entityName: SIGN });
  wait(2500);
  on(1, { verb: "look" });
  on(1, { verb: "use", className: "LF_HouseSaleBuyAction", entityName: SIGN }, 10000);
  wait(3000);
  const look = on(1, { verb: "look" }, 10000);
  if (/SCR_DoorUserAction/.test(String(look.detail))) on(1, { verb: "use", className: "SCR_DoorUserAction" }, 10000);
  wait(3000);

  // Night: the server clock to 22:00.
  on(1, { verb: "time", hour: 22 }, 10000);
  wait(3000);

  // B joins the police, clocks in and cuffs A beside the house sign.
  on(2, { verb: "resetcrime" }, 10000);
  on(2, { verb: "stand", entityName: DESK });
  wait(2500);
  on(2, { verb: "look" });
  on(2, { verb: "use", className: "LF_PoliceApplyAction", entityName: DESK }, 10000);
  wait(1500);
  on(2, { verb: "type", widget: "PolAppName", text: "Bob_Officer" });
  on(2, { verb: "type", widget: "PolAppWhy", text: "Late_joiner_sweep" });
  on(2, { verb: "type", widget: "PolAppAvailability", text: "Evenings" });
  on(2, { verb: "click", widget: "PolAppSubmit" });
  wait(3000);
  on(2, { verb: "look" });
  if (state(2).duty !== "1") on(2, { verb: "use", className: "LF_DutyAction", entityName: DESK }, 10000);
  wait(3000);
  on(2, { verb: "stand", entityName: `Player${a}` });
  wait(3000);
  on(2, { verb: "handsup", words: ["offer", String(a)] }, 10000);
  wait(1000);
  on(1, { verb: "handsup", words: ["choose", "1"] }, 10000);
  wait(1500);
  ensureCuffs(on, wait, 2);
  wait(2500);
  on(2, { verb: "cuffing", playerId: Number(a), kind: "cuff", mode: "pin" }, 10000);
  wait(2000);
  on(2, { verb: "cuffing", playerId: Number(a), kind: "cuff", mode: "finish" }, 10000);
  wait(2500);
  console.log(`A cuffed on the server: ${state(1).cuffed}`);

  // Client 3 joins late.
  const joined = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "session", "join", "3"], { windowsHide: true, stdio: "inherit" });
  if (joined.status !== 0) { console.log("FAIL  join: client 3 did not join late"); return 1; }
  arrive(on, 3, wait);
  wait(3000);
  on(3, { verb: "click", widget: "WelcomeGuideDismissButton" }, 10000);
  wait(5000);

  const both = (spot, read) => {
    const out = {};
    for (const n of [1, 3]) {
      if (spot) { on(n, { verb: "stand", ...spot }); wait(4000); }
      out[n] = read(n);
    }
    return out;
  };

  // Car: A is cuffed, but `stand` is a server move and still works. Client 1 reads first, then client 3 at the same spot.
  const car = both({ entityName: GARAGE, side: "n12" }, (n) => on(n, { verb: "seen", radius: 25 }, 10000).detail);
  const carOf = (n) => entry(token(car[n], "veh"), "") ?? "";
  for (const f of ["plate", "lock", "ign", "stolen"]) {
    const e = field(carOf(1), f);
    const l = field(carOf(3), f);
    row(`vehicle ${f}`, "bought the car, took it out of the garage (locked)", e, l, e !== null && e === l);
    // "-" on both: the prefab has no plate points, so no client draws a plate (#5129); the registration still replicates.
    if (f === "plate" && e === "-") console.log("NOTE  vehicle plate: the bought car draws no plate on any client (no plate points on the prefab, #5129)");
  }

  // House and door: both stand at the sign.
  const house = both({ entityName: SIGN }, (n) => on(n, { verb: "seen", radius: 30 }, 10000).detail);
  for (const [t, f] of [["housesign", "sold"], ["house", "owned"], ["house", "locked"]]) {
    const e = field(token(house[1], t), f);
    const l = field(token(house[3], t), f);
    row(`${t} ${f}`, "bought house sp-b01", e, l, e !== null && e === l);
  }
  const doorsE = token(house[1], "doors") ?? "none";
  const doorsL = token(house[3], "doors") ?? "none";
  const openE = doorsE.split("|").filter((d) => Math.abs(Number(field(d, "state"))) > 0.5).length;
  const openL = doorsL.split("|").filter((d) => Math.abs(Number(field(d, "state"))) > 0.5).length;
  row("doors open near the sign", "opened the nearest door", `${openE} open`, `${openL} open`, openE === openL);
  const lockedE = (token(house[1], "door") ?? "none").replace(/:d=[^:]*/g, "");
  const lockedL = (token(house[3], "door") ?? "none").replace(/:d=[^:]*/g, "");
  row("LF door locks near the sign", "-", lockedE, lockedL, lockedE.split("|").sort().join() === lockedL.split("|").sort().join());

  // Night lights: lights on within 80 m of the sign on each client.
  const lights = both(null, (n) => lightsOn(on(n, { verb: "perfstress", words: ["count", SIGN, "80"] }, 15000).detail));
  row("night lights on (80 m)", "clock set to 22:00", lights[1], lights[3], lights[1] !== null && lights[1] === lights[3] && lights[1] > 0);

  // Cuffs: A's body as client 2 (there all along) and client 3 read it.
  const peers = { 2: on(2, { verb: "peers" }, 10000).detail, 3: on(3, { verb: "peers" }, 10000).detail };
  const pe = peer(peers[2], a) ?? {};
  const pl = peer(peers[3], a) ?? {};
  row("cuffed flag", "B cuffed A", pe.cuffed, pl.cuffed, pe.cuffed === "1" && pl.cuffed === "1");
  row("worn handcuffs model", "B cuffed A", pe.worn, pl.worn, pe.worn === "1" && pl.worn === "1");

  // Site flag and hideout read where they are when in range (none near the sign is a read, not a failure).
  row("site flag (nearest)", "-", token(house[1], "siteflag"), token(house[3], "siteflag"), token(house[1], "siteflag") === token(house[3], "siteflag"));

  const out = join(dir, "jip-sweep-result.json");
  writeFileSync(out, JSON.stringify({ rows, serverLogTail: serverLog().split("\n").filter((l) => /Restrain|HouseSale|Property|VehicleLock|LicensePlate/.test(l)).slice(-40) }, null, 2));
  const failed = rows.filter((r) => !r.ok).length;
  console.log(`SUMMARY jip-sweep-proof ${failed ? "fail" : "ok"} rows=${rows.length} failed=${failed} result=${out}`);
  return failed ? 1 : 0;
}

export default {
  name: "jip-sweep-proof",
  noReset: true,
  usage: "jip-sweep-proof [--selftest]   (session up --mission island --clients 2 --wait; it joins client 3 itself)",
  run: (ctx) => ((ctx?.args ?? []).includes("--selftest") ? selftest() : runJipSweep()),
};

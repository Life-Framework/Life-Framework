import { spawnSync } from "node:child_process";
import { mkdirSync, readFileSync, writeFileSync } from "node:fs";
import { dirname, join } from "node:path";
import { ROOT, arrive, clientBox, newestLog, parseState, sessionDir, sessionPaths } from "../session.mjs";
import { ensureCuffs } from "./_preconditions.mjs";

// Late-joiner sweep before the 0.3.0 RC (2026-11-27, docs/testing/jip-sweep-2026-11-27.md), on the real island on a
// dedicated server:
//   node agent-box/session.mjs up --mission island --clients 2 --profile lowspec --hold 20 --wait
//   node agent-box/session.mjs jip-sweep-proof
//   node agent-box/session.mjs down
// Client 1 (A, civilian) founds a crew and takes the farm compound's hideout lease (the site flag then names the crew),
// opens the barn door, buys a car (it comes out of the garage locked), buys a house, takes an item off a supermarket
// shelf and pays for it; the clock is set to night. Client 2 (B) joins the police, cuffs A and books A into the
// Saint-Philippe cell. Then client 3 joins late. Each thing a player would notice is read on client 2 (there all along)
// and on client 3 (the late joiner) at the same spot through the harness `seen`, `peers` and `perfstress count` verbs,
// which read THIS client's replicated copy. A row passes when both read the same and A's own read (taken when A set the
// state) shows the state was really set. Client 1 is jailed by the time client 3 joins, so A's own reads are taken early.
//
//   jip-sweep-proof --selftest      the token parsers on made-up lines, no game
const FIT = "Fit_saint_philippe_";
const GARAGE = `${FIT}carlot_garage`;
const LOT = `${FIT}carlot_vehicle_lot`;
const DESK = `${FIT}police_police_kit_duty_desk`;
const BOOKING = `${FIT}police_police_kit_booking_desk`;
const CELL_DOOR = `${FIT}jail_jail_cell_door`;
const REGISTRY = `${FIT}org_registry_desk`;
const SHELVES = `${FIT}supermarket_supermarket_shelves`;
const TILL = `${FIT}supermarket_supermarket_till`;
const SIGN = "HomeSign_saint_philippe_sp_b01";
const CREW = "JipCrew";
// The base sites a new crew can lease, by hideout id: the barn door and the flag pole at each.
const SITES = {
  farm_compound: { door: "Fit_farmcompound_compound_barn_door", flag: "Fit_farmcompound_compound_site_flag" },
  hilltop_mast: { door: "Fit_hilltopmast_a_door", flag: "Fit_hilltopmast_a_flag" },
};

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

// A list token without its distances, sorted: two clients standing a step apart read the same list.
export function flat(list) {
  return String(list ?? "none").replace(/:d=[^:|]*/g, "").split("|").sort().join("|");
}

// How many doors of a `doors=` list stand open (state off zero: a hideout door swings to -0.41, a house door to 0.98).
export function openCount(list) {
  if (!list || list === "none") return 0;
  return list.split("|").filter((d) => Math.abs(Number(field(d, "state"))) > 0.05).length;
}

// The peers entry for one player id, as a map of its key=value words.
export function peer(line, playerId) {
  for (const part of String(line ?? "").split(" | ")) {
    const words = Object.fromEntries(part.trim().split(/\s+/).map((w) => w.split("=")).filter((kv) => kv.length === 2));
    if (words.player === String(playerId)) return words;
  }
  return null;
}

// A peers `at=x,y,z` value as numbers, or null.
export function at(words) {
  const p = String(words?.at ?? "").split(",").map(Number);
  return p.length === 3 && p.every(Number.isFinite) ? p : null;
}

export function distance(a, b) {
  return a && b ? Math.hypot(a[0] - b[0], a[1] - b[1], a[2] - b[2]) : Infinity;
}

// `perfstress count` reply: the lights-on number.
export function lightsOn(detail) {
  const m = /lights(?:_|\s)?on[=:\s](\d+)/i.exec(String(detail ?? "")) ?? /on=(\d+)/.exec(String(detail ?? ""));
  return m ? Number(m[1]) : null;
}

// The prefab file name (no extension) of a `shelf took ... item={GUID}Prefabs/Items/X.et` reply.
export function shelfItemName(detail) {
  const m = /item=(\S+)/.exec(String(detail ?? ""));
  return m ? (m[1].split("/").pop() ?? "").replace(/\.et$/, "") : null;
}

// The hideout id of the newest `lease org=... hideout=...` server log line.
export function leasedHideout(log) {
  const all = [...String(log ?? "").matchAll(/lease org=\S+ hideout=(\S+)/g)];
  return all.length ? all[all.length - 1][1] : null;
}

function selftest() {
  const f = [];
  const line = "me=CIV/0/0 cash=5 veh=LF_S105_Red:d=3:eng=0:lock=1:ign=1:stolen=0:plate=EV-123|UAZ:d=9:eng=0:lock=0:ign=1:stolen=0:plate=- doors=Door_A:d=2:state=0.98|Door_B:d=4:state=0 housesign=saint-philippe.home.sp-b01:sold=1 house=x:owned=1:locked=0 siteflag=farm_compound:holder=JipCrew:flag=3 hideout=farm_compound:owner=JipCrew loose=4";
  const veh = entry(token(line, "veh"), "S105");
  if (field(veh, "plate") !== "EV-123" || field(veh, "lock") !== "1") f.push("veh fields");
  if (field(token(line, "housesign"), "sold") !== "1") f.push("housesign sold");
  if (field(entry(token(line, "doors")), "state") !== "0.98") f.push("door state");
  if (openCount(token(line, "doors")) !== 1) f.push("open count");
  if (flat(token(line, "doors")) !== "Door_A:state=0.98|Door_B:state=0") f.push("flat");
  if (field(token(line, "siteflag"), "holder") !== "JipCrew") f.push("siteflag holder");
  if (token(line, "loose") !== "4") f.push("loose");
  const peers = "player=1 downed=0 life=ALIVE carrier=0 seat=0 at=1,2,3 cuffed=1 worn=1 ragdoll=0 stance=STAND | player=2 downed=0 cuffed=0 worn=0 at=9,9,9";
  const p = peer(peers, 1);
  if (!p || p.cuffed !== "1" || p.worn !== "1") f.push("peer cuffed/worn");
  if (distance(at(p), [1, 2, 4]) !== 1) f.push("peer at");
  if (lightsOn("entities=10 static=4 lights=6 lightsOn=3 shadows=1") !== 3) f.push("lights on");
  if (shelfItemName("shelf took store=everon item={ABC}Prefabs/Items/Food/Apple.et from=1,2,3") !== "Apple") f.push("shelf item name");
  if (leasedHideout("x\nlease org=o1 hideout=farm_compound\nlease org=o2 hideout=hilltop_mast") !== "hilltop_mast") f.push("leased hideout");
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
    console.log(`${ok ? "PASS" : "FAIL"}  ${system}: there all along ${early} / late joiner ${late}`);
  };
  const seen = (n, extra) => on(n, { verb: "seen", radius: 30, ...extra }, 10000).detail;

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
  const early = {}; // A's own reads, taken as each state was set (A is jailed by the time client 3 joins)

  // A: money, a crew and its hideout lease, the barn door open, a car, a house, a shelf item.
  on(1, { verb: "resetplayer", cash: 9000, keepInventory: true }, 10000);
  wait(1500);
  on(1, { verb: "licence", name: "DRIVING_LICENCE" }, 10000);
  wait(1500);

  // Crew: found it at the registry desk and take the free base site's lease; the site flag then names the crew (5 s reconcile).
  on(1, { verb: "stand", entityName: REGISTRY });
  wait(2500);
  on(1, { verb: "use", className: "LF_OrganizationDeskAction", entityName: REGISTRY }, 10000);
  wait(1500);
  on(1, { verb: "type", widget: "OrgName", text: CREW });
  on(1, { verb: "click", widget: "FoundButton" });
  wait(3000);
  on(1, { verb: "click", widget: "OrgHideoutRow_Action" });
  wait(2500);
  on(1, { verb: "click", widget: "CloseButton" });
  wait(1500);
  const site = SITES[leasedHideout(serverLog())];
  console.log(`crew ${CREW} leased: ${leasedHideout(serverLog()) ?? "no lease line"}`);
  if (site) {
    on(1, { verb: "stand", entityName: site.door });
    wait(2500);
    on(1, { verb: "look" });
    on(1, { verb: "use", className: "SCR_DoorUserAction", entityName: site.door }, 10000);
    wait(7000);
    early.site = seen(1);
  }

  // Car: bought, stored and taken out of the garage (it comes out locked).
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

  // House: A buys the house behind sign sp-b01.
  on(1, { verb: "stand", entityName: SIGN });
  wait(2500);
  on(1, { verb: "look" });
  on(1, { verb: "use", className: "LF_HouseSaleBuyAction", entityName: SIGN }, 10000);
  wait(3000);
  early.house = seen(1);

  // Shop shelf: A takes one item off the supermarket shelf and pays at the till. `loose=` counts the loose copies of it.
  on(1, { verb: "stand", entityName: SHELVES });
  wait(2500);
  // The item is only known from the take; its reply carries the server's stock after the take (`stock=N`), which A's
  // own loose count must equal (the taken copy left the world). A restock between reads shows as a NOTE.
  const took = on(1, { verb: "shelf" }, 10000);
  const shelfItem = shelfItemName(took.detail);
  wait(1500);
  if (shelfItem) early.shelf = seen(1, { radius: 8, item: shelfItem });
  on(1, { verb: "stand", entityName: TILL, side: "s" });
  wait(1500);
  on(1, { verb: "use", className: "LF_StoreTillAction", entityName: TILL }, 10000);
  wait(1500);

  // Night: the server clock to 22:00.
  on(1, { verb: "time", hour: 22 }, 10000);
  wait(3000);

  // B joins the police, clocks in, cuffs A and books A into the cell.
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
  const charge = on(1, { verb: "bookcharge", words: ["robbery"] }, 10000);
  wait(1500);
  console.log(`A charged: ${charge.status} ${charge.detail}`);
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
  const cuffedOnServer = state(1).cuffed;
  console.log(`A cuffed on the server: ${cuffedOnServer}`);

  // The booking desk: B opens the book, ticks the first row and signs; A, the suspect, takes the time with the interact key.
  on(2, { verb: "stand", entityName: BOOKING, side: "e" });
  wait(2500);
  on(1, { verb: "stand", entityName: BOOKING, side: "w" });
  wait(3000);
  on(2, { verb: "look" });
  on(2, { verb: "use", className: "LF_BookingDeskBookAction", entityName: BOOKING }, 10000);
  h.setBoxes([clientBox(dir, 2)]);
  h.waitForText("Sign", 12000);
  on(2, { verb: "ui", mode: "text" }, 10000);
  on(2, { verb: "click", widget: "ChargeSheetRow0_Action" }, 10000);
  wait(800);
  on(2, { verb: "ui", mode: "text" }, 10000);
  on(2, { verb: "click", widget: "ChargeSheetSign" }, 10000);
  wait(2500);
  {
    let aim;
    const started = Date.now();
    const selected = () => /selected=LF_BookingDeskAnswerAction/.test(aim.detail);
    do {
      for (const target of [-45, -30, -20, -55, -10]) {
        on(1, { verb: "input", action: "CharacterPitch", value: target }, 8000);
        wait(800);
        aim = on(1, { verb: "aim", words: [] }, 10000);
        if (selected()) break;
      }
      if (!selected()) wait(4000);
    } while (!selected() && Date.now() - started < 30000);
    for (let i = 0; i < 3 && !/min/.test((/ name=(.*?) menu=/.exec(aim.detail) ?? [0, ""])[1]); i++) {
      on(1, { verb: "interact", step: "next" }, 8000);
      wait(700);
      aim = on(1, { verb: "aim", words: [] }, 10000);
    }
    on(1, { verb: "interact", step: "confirm" }, 8000);
    wait(3500);
    on(1, { verb: "input", action: "CharacterPitch", value: 0 }, 8000);
  }
  wait(3000);
  const jailLeft = Number(state(1).jail) || 0;
  console.log(`A in the cell: jail=${jailLeft} s`);
  if (jailLeft <= 0) console.log("NOTE  A is not serving a sentence; the jail rows below will fail (re-run once)");

  // Client 3 joins late.
  const joined = spawnSync(process.execPath, [join(ROOT, "tools", "cli.mjs"), "session", "join", "3"], { windowsHide: true, stdio: "inherit" });
  if (joined.status !== 0) { console.log("FAIL  join: client 3 did not join late"); return 1; }
  arrive(on, 3, wait);
  wait(3000);
  on(3, { verb: "click", widget: "WelcomeGuideDismissButton" }, 10000);
  wait(5000);

  // Client 2 (there all along) and client 3 each stand at the same spot and read their own copy.
  const both = (spot, read) => {
    const out = {};
    for (const n of [2, 3]) {
      if (spot) { on(n, { verb: "stand", ...spot }); wait(4000); }
      out[n] = read(n);
    }
    return out;
  };
  const same = (x, y) => x !== null && x !== undefined && x === y;

  // Car.
  const car = both({ entityName: GARAGE, side: "n12" }, (n) => seen(n, { radius: 25 }));
  const carOf = (n) => entry(token(car[n], "veh"), "") ?? "";
  for (const f of ["plate", "lock", "ign", "stolen"]) {
    const e = field(carOf(2), f);
    const l = field(carOf(3), f);
    row(`vehicle ${f}`, "bought the car, took it out of the garage (locked)", e, l, same(e, l));
    // "-" on both: the prefab has no plate points, so no client draws a plate (#5129); the registration still replicates.
    if (f === "plate" && e === "-") console.log("NOTE  vehicle plate: the bought car draws no plate on any client (no plate points on the prefab, #5129)");
  }

  // House.
  const house = both({ entityName: SIGN }, (n) => seen(n));
  for (const [t, f] of [["housesign", "sold"], ["house", "owned"], ["house", "locked"]]) {
    const e = field(token(house[2], t), f);
    const l = field(token(house[3], t), f);
    row(`${t} ${f}`, "bought house sp-b01", e, l, same(e, l));
  }
  row("house was bought", "A's own read", field(token(early.house, "housesign"), "sold"), "-", field(token(early.house, "housesign"), "sold") === "1");
  row("LF door locks near the sign", "-", flat(token(house[2], "door")), flat(token(house[3], "door")), flat(token(house[2], "door")) === flat(token(house[3], "door")));

  // Night lights: lights on within 80 m of the sign on each client.
  const lights = both(null, (n) => lightsOn(on(n, { verb: "perfstress", words: ["count", SIGN, "80"] }, 15000).detail));
  row("night lights on (80 m)", "clock set to 22:00", lights[2], lights[3], lights[2] !== null && lights[2] === lights[3] && lights[2] > 0);

  // Hideout site: the barn door A opened, the site flag naming the crew, the hideout owner.
  if (!site) {
    row("hideout site", "A founded a crew and leased a base site", "no lease", "-", false);
  } else {
    const yard = both({ entityName: site.door }, (n) => seen(n));
    const e = token(early.site, "doors") ?? "none";
    row("barn door set open", "A opened the hideout door", `${openCount(e)} open (A's read)`, "-", openCount(e) > 0);
    row("doors open at the barn", "A opened the hideout door", `${openCount(token(yard[2], "doors"))} open`, `${openCount(token(yard[3], "doors"))} open`, openCount(token(yard[2], "doors")) === openCount(token(yard[3], "doors")) && openCount(token(yard[3], "doors")) > 0);
    row("doors list at the barn", "-", flat(token(yard[2], "doors")), flat(token(yard[3], "doors")), flat(token(yard[2], "doors")) === flat(token(yard[3], "doors")));
    const flag2 = token(yard[2], "siteflag");
    const flag3 = token(yard[3], "siteflag");
    row("site flag holder", "A's crew leased the hideout", flag2, flag3, same(flag2, flag3) && field(flag3, "holder") === CREW);
    row("hideout owner", "A's crew leased the hideout", token(yard[2], "hideout"), token(yard[3], "hideout"), same(token(yard[2], "hideout"), token(yard[3], "hideout")) && field(token(yard[3], "hideout"), "owner") === CREW);
  }

  // Shop shelf: the loose copies of the item A took, at the shelves.
  if (!shelfItem) {
    row("shop shelf", "A took one item off the shelf", `shelf verb: ${took.detail}`, "-", false);
  } else {
    const shelf = both({ entityName: SHELVES }, (n) => token(seen(n, { radius: 8, item: shelfItem }), "loose"));
    const stock = /stock=(\d+)/.exec(String(took.detail))?.[1] ?? null;
    const own = token(early.shelf, "loose");
    row("shelf copy taken", `A took one ${shelfItem}`, `A sees ${own} loose`, `server stock ${stock}`, own !== null && own === stock);
    row("shop shelf copies", `A took one ${shelfItem}`, shelf[2], shelf[3], same(shelf[2], shelf[3]));
    if (shelf[2] !== own) console.log(`NOTE  shelf: ${shelfItem} loose copies ${own} right after the take, ${shelf[2]} later (a restock ran)`);
  }

  // Cuffs and the jail: A sits in the cell (booked), B and client 3 stand at the cell door and read A's body and the door lock.
  const cell = both({ entityName: CELL_DOOR }, (n) => ({ peers: on(n, { verb: "peers" }, 10000).detail, seen: seen(n) }));
  const pe = peer(cell[2].peers, a) ?? {};
  const pl = peer(cell[3].peers, a) ?? {};
  row("jailed body in the cell", `B booked A, sentence ${jailLeft} s`, pe.at ?? "none", pl.at ?? "none", distance(at(pe), at(pl)) < 1.5);
  row("jailed player cuffed flag", "B cuffed A, then booked A", pe.cuffed, pl.cuffed, same(pe.cuffed, pl.cuffed));
  row("jailed player worn cuffs", "B cuffed A, then booked A", pe.worn, pl.worn, same(pe.worn, pl.worn));
  const lockE = flat(token(cell[2].seen, "door"));
  const lockL = flat(token(cell[3].seen, "door"));
  row("cell door lock", "A is in the cell", lockE, lockL, lockE !== "none" && lockE === lockL);
  row("jail sentence running", "B booked A", `${jailLeft} s`, "-", jailLeft > 0);

  const out = join(dir, "jip-sweep-result.json");
  writeFileSync(out, JSON.stringify({ rows, serverLogTail: serverLog().split("\n").filter((l) => /Restrain|HouseSale|Property|VehicleLock|LicensePlate|Jail|Organizations|Hideout|Booking/.test(l)).slice(-60) }, null, 2));
  const failed = rows.filter((r) => !r.ok).length;
  console.log(`SUMMARY jip-sweep-proof ${failed ? "fail" : "ok"} rows=${rows.length} failed=${failed} result=${out}`);
  return failed ? 1 : 0;
}

export default {
  name: "jip-sweep-proof",
  noReset: true,
  usage: "jip-sweep-proof [--selftest]   (session up --mission island --clients 2 --hold 20 --wait; it joins client 3 itself)",
  run: (ctx) => ((ctx?.args ?? []).includes("--selftest") ? selftest() : runJipSweep()),
};

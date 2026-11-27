import { copyFileSync, existsSync, mkdirSync } from "node:fs";
import { extname, join } from "node:path";
import { ROOT } from "../session.mjs";
import { openKit } from "./_island-kit.mjs";
import { LOT, LOT_PAD, enterWhenThere, kv, openShutGate } from "./drive-proof.mjs";
import { EAST_COMPOUND, TRUCK_YARD, SCRAPYARD, planOf } from "./drive-long-proof.mjs";

// The night tour (2026-11-27, `docs/playtest/rounds/2026-11-27-night-drive.md`): one car from the Saint-Philippe lot, headlights
// on, the clock pinned to 22:00, driven by road to the east compound, East Rise, Highstone, the north outpost, the NE truck yard and
// back to the square. At every arrival: a picture from the driver's seat, one standing beside the car, and one standing at each
// counter, sign or NPC of the site. On the way: a picture from the seat every 75 s and one 30 to 110 m before the end of a leg (the
// road turn-off and the gate). On `session up --mission island --profile full --hold 20` (one client):
//   node agent-box/session.mjs drive-night-tour [--speed 55] [--legs 1,2,3,4,5,6] [--buy] [--hour 22]
// PNGs are copied to tmp/night-drive/ (the session's capture folder is cleared by `session down`). Each leg prints one LEG line.
export const EAST_RISE = [6684, 7915];
export const HIGHSTONE = [4959, 8460];
export const OUTPOST_GATE = [7660.5, 8384];
export const SQUARE = [4437, 10670];

export const LEGS = [
  { id: "1_east_compound", label: "Saint-Philippe to the east compound (smelter gate)", to: EAST_COMPOUND, atRoadEnd: true, sites: ["Fit_eastcompound_smelter_smelter_furnace"] },
  { id: "2_east_rise", label: "east compound to East Rise", to: EAST_RISE, atRoadEnd: true, sites: ["Fit_eastrise_mine_mine_sign"] },
  { id: "3_highstone", label: "East Rise to Highstone", to: HIGHSTONE, atRoadEnd: true, sites: ["Fit_highstone_mine_mine_sign", "Fit_highstone_mine_mine_ore_2"] },
  { id: "4_north_outpost", label: "Highstone to the north outpost gate", to: OUTPOST_GATE, atRoadEnd: false, sites: ["Fit_outpost_north_camp_outpost_npc_quartermaster"] },
  { id: "5_truck_yard", label: "north outpost to the NE truck yard gate", to: TRUCK_YARD, near: SCRAPYARD, atRoadEnd: false, sites: ["Fit_ne_compound_truckyard_sign_gate", "Fit_ne_compound_truckyard_examiner"] },
  { id: "6_square", label: "NE truck yard back to the square", to: SQUARE, atRoadEnd: false, sites: [] },
];

// The last road point before the goal (a plan's `near=` list ends with the goal itself), or the goal when the route has no
// off-road tail (a goal on the road). `drive plan <goal> near=<goal>` answers `near=x,z;x,z;...`.
export function roadEndOf(detail, goal) {
  const near = String(detail ?? "").replace(/^.*near=/, "").split(";").filter(Boolean);
  if (!/near=/.test(String(detail ?? "")) || near.length < 2) return goal;
  const end = near[near.length - 2].split(",").map(Number);
  return end.length === 2 && end.every(Number.isFinite) ? end : goal;
}

// A drive that ended `stuck` (or `close` with this much left) hands the rest to the player on foot: out of the car, `walk to` the
// goal. FOOT_ARRIVE_M is how near the walker must end; FOOT_SECONDS the walk's budget (sprinting is about 5 m/s).
export const FOOT_ARRIVE_M = 8;
export const FOOT_SECONDS = 150;
export const FOOT_MIN_M = 25;
export const FOOT_MAX_M = 400;
export function needsFoot(status, restM) {
  return /^(stuck|close|distance|timeout)/.test(String(status ?? "")) && restM > FOOT_MIN_M && restM <= FOOT_MAX_M;
}

const arg = (name, fallback) => {
  const i = process.argv.indexOf(name);
  return i > 0 && process.argv[i + 1] ? process.argv[i + 1] : fallback;
};

function selftest() {
  const f = [];
  const e = roadEndOf("plan points=812 route=6324m straight=4563m near=6400.1,6460.2;6425.5,6468.0;6430,6470", EAST_COMPOUND);
  if (e[0] !== 6425.5 || e[1] !== 6468) f.push("road end is the point before the goal");
  const g = roadEndOf("plan points=3 route=100m straight=90m", [1, 2]);
  if (g[0] !== 1 || g[1] !== 2) f.push("a plan with no near list keeps the goal");
  if (planOf("plan points=1 route=2m straight=3m")?.route !== 2) f.push("planOf is re-exported");
  if (!needsFoot("stuck", 150) || needsFoot("stuck", 10) || needsFoot("arrived", 150) || needsFoot("stuck", 2500)) f.push("a stuck drive far from its goal walks on");
  if (LEGS.length !== 6 || LEGS.some((l) => !l.id || !l.to)) f.push("six legs, each with an id and a goal");
  for (const x of f) console.log(`FAIL  ${x}`);
  console.log(f.length ? "drive-night-tour selftest: FAIL" : "drive-night-tour selftest: PASS");
  return f.length ? 1 : 0;
}

async function run(args) {
  if (args.includes("--selftest")) return selftest();
  const speed = arg("--speed", "55");
  const hour = Number(arg("--hour", "22"));
  const only = String(arg("--legs", "1,2,3,4,5,6")).split(",").map(Number);
  const k = await openKit("drive-night-tour");
  const { h, on, wait, step, state } = k;
  const outDir = join(ROOT, "tmp", "night-drive");
  mkdirSync(outDir, { recursive: true });
  const pics = [];
  const dist = (a, b) => Math.hypot(a[0] - b[0], a[1] - b[1]);
  const pin = () => on({ verb: "time", hour }, 15000);
  const veh = () => kv(on({ verb: "vehicle", words: ["state"] }).detail);
  const posOf = (s) => {
    // `vehicle state` pos is x,y,z (y the height): the ground position is x and z.
    const m = /^(-?[\d.]+),(-?[\d.]+),(-?[\d.]+)/.exec(String(s.pos ?? ""));
    return m ? [Number(m[1]), Number(m[3])] : null;
  };
  // A picture of the client's screen, copied out of the capture folder under a name that says where it was taken.
  const shot = (name) => {
    const r = on({ verb: "capture", mode: "ui", name: `nd_${name}` }, 40000);
    wait(1200);
    const png = h.resolveCapture(r.detail);
    if (png && existsSync(png)) {
      // The engine writes a BMP; keep the real extension so a viewer opens it (the shell tools convert to JPG for a PR).
      const to = join(outDir, `${name}${extname(png).toLowerCase() || ".bmp"}`);
      copyFileSync(png, to);
      pics.push(to);
      console.log(`PICTURE ${to}`);
      return to;
    }
    console.log(`PICTURE-MISSING ${name} (${r.status} ${String(r.detail).slice(0, 120)})`);
    return "";
  };

  // At the wheel of the owned car: enter; when the server puts the driver out (a locked car), unlock it from beside a door
  // (as drive-long-proof does) and enter again.
  const seated = () => {
    const isPilot = () => kv(on({ verb: "vehicle", words: ["state"] }).detail).seat === "pilot";
    if (isPilot()) return true;
    const p = String(state().parkedat ?? "").split(",").map(Number);
    if (p.length !== 3) return false;
    for (let n = 0; n < 9; n++) {
      if (n > 0) {
        const a = ((n - 1) * Math.PI) / 4;
        on({ verb: "capture", mode: "world", name: "nd_door", x: p[0], y: p[1], z: p[2], ox: Number((3 * Math.cos(a)).toFixed(2)), oy: 0.3, oz: Number((3 * Math.sin(a)).toFixed(2)), hour }, 30000);
        wait(1200);
        on({ verb: "use", className: "LF_VehicleLockAction" }, 10000);
        wait(1300);
      }
      on({ verb: "vehicle", words: ["enter", "driver", `at=${p[0]},${p[2]}`, "r=25"] }, 15000);
      wait(3000);
      if (isPilot()) return true;
    }
    return false;
  };

  // ---- 0: a car from the lot, headlights on, at 22:00 --------------------------------------------------------------------
  const buy = () => {
    on({ verb: "checkpoint", words: ["10"] }, 20000);
    wait(3000);
    on({ verb: "licence", name: "DRIVING_LICENCE" }, 10000);
    wait(1000);
    on({ verb: "stand", entityName: LOT });
    wait(2000);
    on({ verb: "use", className: "LF_VehicleBuyAction", entityName: LOT }, 10000);
    wait(2000);
    const widget = /Car\d+_Action/.exec(String(on({ verb: "ui" }).detail))?.[0] ?? "Car0_Action";
    on({ verb: "click", widget });
    wait(4000);
    on({ verb: "click", widget: "LotClose" });
    return state();
  };
  let st = state();
  if (args.includes("--buy") || !st.parked || st.parked === "none") st = buy();
  step("0 a car is bought and parked", !!st.parked && st.parked !== "none", `parked=${st.parked} parkedat=${st.parkedat}`);
  const clock = pin();
  console.log(`clock: ${String(clock.detail).slice(0, 160)}`);
  const enter = enterWhenThere(on, wait, LOT_PAD, 6);
  wait(1500);
  step("0b at the wheel", enter.status === "ok", String(enter.detail).slice(0, 120));
  on({ verb: "vehicle", words: ["lights", "on"] });
  wait(800);
  const lamp = veh();
  console.log(`headlights: lights=${lamp.lights} fuel=${lamp.fuel}`);
  shot("0_lot_seat");

  // On foot to the leg's goal: stop the car, get out, `walk to` the spot until `walk state` says arrived or timeout. The leg
  // counts as arrived (`onfoot`) when the walker ends within FOOT_ARRIVE_M of it; the LEG line says so and how far it walked.
  const walkTo = (leg, goal, rest) => {
    on({ verb: "vehicle", words: ["stop"] }, 20000);
    wait(1500);
    const out = on({ verb: "vehicle", words: ["exit", "force"] }, 10000);
    wait(1800);
    const from = String(on({ verb: "walk", words: ["state"] }).detail ?? "");
    const go = on({ verb: "walk", words: ["to", goal.join(","), String(FOOT_SECONDS), String(FOOT_ARRIVE_M)] }, 10000);
    console.log(`ONFOOT ${leg.id}: the car stopped ${rest.toFixed(0)} m short; out of the car: ${out.status}; walk to ${goal.join(",")}: ${go.status} ${String(go.detail).slice(0, 100)}`);
    let state = "";
    const w0 = Date.now();
    while (go.status === "ok" && Date.now() - w0 < (FOOT_SECONDS + 30) * 1000) {
      wait(4000);
      state = String(on({ verb: "walk", words: ["state"] }).detail ?? "");
      console.log(`${leg.id} on foot t=${((Date.now() - w0) / 1000).toFixed(0)}s ${state}`);
      if (/walk=(arrived|timeout)/.test(state)) break;
    }
    const left = Number(/left=(\d+)/.exec(state)?.[1] ?? 9999);
    const ok = /walk=arrived/.test(state) || left <= FOOT_ARRIVE_M;
    const line = `${ok ? "arrived" : "short"} on foot, car ${rest.toFixed(0)} m short, ${left} m left after ${((Date.now() - w0) / 1000).toFixed(0)} s (${state})`;
    console.log(`ONFOOT ${leg.id}: ${line}`);
    return { status: ok ? "onfoot_arrived" : "onfoot_short", line, from };
  };

  // One leg: plan for the road end where it matters, drive, take pictures on the way and at the end.
  const legsOut = [];
  const driveLeg = (leg) => {
    let goal = leg.to;
    const planWords = ["plan", leg.to.join(","), `near=${(leg.near ?? leg.to).join(",")}`];
    const plan = on({ verb: "drive", words: planWords }, 60000);
    const pl = planOf(plan.detail);
    if (leg.atRoadEnd) goal = roadEndOf(plan.detail, leg.to);
    console.log(`PLAN ${leg.id}: ${pl ? `${pl.route} m by road for ${pl.straight} m straight, ${pl.points} points` : String(plan.detail).slice(0, 160)}; goal ${goal.join(",")}`);
    pin();
    on({ verb: "vehicle", words: ["lights", "on"] });
    const from = posOf(veh());
    const words = ["road", goal.join(","), `speed=${speed}`, "tol=10", "mode=input"];
    const t0 = Date.now();
    let end = { status: "not_started" };
    // A shut gate or boom barrier across the way ends the drive (`why=...shut_door_...`): open it as a player does and
    // drive on, at most twice.
    for (let attempt = 0; attempt < 4; attempt++) {
    if (attempt > 0 && !/shut_door/.test(end.why ?? "")) {
      // Stuck in trees far from the goal (a car left in a forest by the leg before): back out and try the drive again.
      if (end.status !== "stuck" || dist(posOf(veh()) ?? goal, goal) <= FOOT_MAX_M) break;
      console.log(`RETRY ${leg.id}: stuck ${dist(posOf(veh()) ?? goal, goal).toFixed(0)} m from the goal (${end.why}); driving again`);
    } else if (attempt > 0) {
      const carAt = String(veh().pos ?? "").split(",").map(Number);
      if (carAt.length !== 3) break;
      const gate = openShutGate(on, wait, carAt);
      console.log(`GATE ${leg.id}: ${gate.doors} door(s) round the car, opened ${gate.opened || "none"}, back at the wheel ${gate.entered}`);
      if (!gate.opened || !gate.entered) break;
      on({ verb: "vehicle", words: ["lights", "on"] });
    }
    const started = on({ verb: "drive", words }, 60000);
    let n = 0;
    let approach = false;
    let lastShot = Date.now();
    let lastPin = Date.now();
    let s = {};
    if (started.status === "ok") {
      const route = Number(/route=(\d+)/.exec(String(started.detail))?.[1] ?? pl?.route ?? 0);
      while (Date.now() - t0 < 1500000) {
        wait(4000);
        s = veh();
        console.log(`${leg.id} t=${((Date.now() - t0) / 1000).toFixed(0)}s pos=${s.pos} ${s.speed}km/h drive=${s.drive} driven=${s.driven} fuel=${s.fuel} lights=${s.lights}`);
        if (Date.now() - lastPin > 30000) {
          pin();
          lastPin = Date.now();
        }
        const left = route - Number(s.driven ?? 0);
        if (!approach && route > 0 && left < 250 && left > 25) {
          approach = true;
          shot(`${leg.id}_approach_${Math.round(left)}m`);
        } else if (Date.now() - lastShot > 75000) {
          shot(`${leg.id}_route_${++n}`);
          lastShot = Date.now();
        }
        if (!/^(driving|braking|backing|routing)$/.test(s.drive ?? "")) break;
      }
      const done = on({ verb: "drive", words: ["wait"] }, 60000);
      end = kv(done.detail);
      end.status = String(done.detail ?? "").split(" ")[0];
      end.route = started.detail;
    } else end.why = started.detail;
    }
    // The off-road tail (the north outpost's track, the truck yard's gate): a straight drive from the road end to the goal.
    if (!leg.atRoadEnd && /arrived|distance|close/.test(end.status) && dist(posOf(veh()) ?? goal, leg.to) > 25) {
      const tail = on({ verb: "drive", words: ["to", leg.to.join(","), "speed=20", "timeout=120", "tol=8", "mode=input"] }, 30000);
      if (tail.status === "ok") {
        shot(`${leg.id}_tail_start`);
        for (let i = 0; i < 40; i++) {
          wait(3000);
          if (!/^(driving|braking|backing|routing)$/.test(veh().drive ?? "")) break;
        }
        const tailEnd = kv(on({ verb: "drive", words: ["wait"] }, 60000).detail);
        end.tail = `status=${String(tailEnd.status ?? "?")} driven=${tailEnd.driven} err=${tailEnd.err}`;
      }
    }
    // The car cannot get closer (the creep gave up in trees or on a bank): park and walk the rest as a player does.
    const rest = dist(posOf(veh()) ?? goal, goal);
    if (needsFoot(end.status, rest)) {
      const foot = walkTo(leg, goal, rest);
      end.status = foot.status;
      end.foot = foot.line;
    }
    const last = veh();
    const wall = (Date.now() - t0) / 1000;
    const row = { leg, from, end, last, wall, route: pl?.route, straight: pl?.straight, goal };
    legsOut.push(row);
    console.log(`LEG ${leg.id}: ${end.status} driven=${end.driven} m time=${end.time}s wall=${wall.toFixed(0)}s err=${end.err} max=${end.max} backups=${end.backups} replans=${end.replans} fuel=${last.fuel}${end.why ? ` why=${end.why}` : ""}${end.stuckat ? ` stuckat=${end.stuckat}` : ""}${end.tail ? ` tail ${end.tail}` : ""}${end.foot ? ` ONFOOT ${end.foot}` : ""}`);
    return row;
  };

  // The pictures at an arrival: seat, beside the car, then each counter, sign or NPC of the site.
  const arriveAt = (leg) => {
    on({ verb: "vehicle", words: ["stop"] }, 20000);
    wait(1500);
    pin();
    wait(500);
    shot(`${leg.id}_arrive_seat`);
    on({ verb: "vehicle", words: ["exit", "force"] }, 10000);
    wait(1800);
    shot(`${leg.id}_arrive_stand`);
    let i = 0;
    for (const site of leg.sites) {
      const r = on({ verb: "stand", entityName: site }, 30000);
      wait(3500);
      pin();
      wait(500);
      const look = String(on({ verb: "look" }).detail ?? "").replace(/\s+/g, " ").slice(0, 200);
      console.log(`SITE ${leg.id} ${site}: ${r.status}; look: ${look}`);
      shot(`${leg.id}_site${++i}_${site.replace(/^Fit_/, "").slice(0, 40)}`);
    }
  };

  for (const leg of LEGS) {
    const idx = Number(leg.id.split("_")[0]);
    if (!only.includes(idx)) continue;
    if (kv(on({ verb: "vehicle", words: ["state"] }).detail).seat !== "pilot" && !seated()) {
      step(`${leg.id} at the wheel`, false, "could not get back into the car");
      break;
    }
    on({ verb: "vehicle", words: ["lights", "on"] });
    const row = driveLeg(leg);
    const arrived = /arrived|distance|close|onfoot_arrived/.test(row.end.status);
    step(`${leg.id} ${leg.label}`, arrived, `${row.end.status} ${row.end.driven} m in ${row.end.time} s, err ${row.end.err}, ${row.end.backups} back-ups${row.end.why ? `, why ${row.end.why}` : ""}`);
    arriveAt(leg);
  }

  console.log("---- SUMMARY");
  for (const r of legsOut) {
    console.log(`LEG ${r.leg.id}: ${r.end.status} route=${r.route} m straight=${r.straight} m driven=${r.end.driven} m game=${r.end.time}s wall=${r.wall.toFixed(0)}s backups=${r.end.backups} fuel=${r.last.fuel}`);
  }
  console.log(`PICTURES ${pics.length} in ${outDir}`);
  for (const p of pics) console.log(`  ${p}`);
  return k.finish();
}

export default {
  name: "drive-night-tour",
  usage: "drive-night-tour [--speed 55] [--legs 1,2,3,4,5,6] [--buy] [--hour 22] [--selftest]   (on `up --mission island --profile full --hold 20`; one car, headlights on, east compound, East Rise, Highstone, north outpost, NE truck yard, square)",
  run: (ctx) => run(ctx?.args ?? []),
};

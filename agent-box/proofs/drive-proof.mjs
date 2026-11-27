import { openKit } from "./_island-kit.mjs";

// The harness drives a car like a player (LF_DriveHarness.c): buy a car at the Saint-Philippe lot by the square,
// sit at the wheel, drive it along the road network to the districts garage, stop on the respray pad, get out.
// On `session up --mission island` (one client):
//   node agent-box/session.mjs drive-proof [--speed 35] [--mode input|sim]
// Every two seconds the car's position, speed and the drive's status are printed; the last lines give the distance
// driven, the time and the stop error from the pad. The pad points come from the pack layers:
//   lot pad  = Fit_saint_philippe_carlot_vehicle_lot (4388.974, 10627.352) + m_vOffset (1, 11.8)
//   bay pad  = Fit_saint_philippe_districts_garage_livery_bay (4691.615, 10751.696) + m_vOffset (-4, 0)
export const LOT = "Fit_saint_philippe_carlot_vehicle_lot";
export const LOT_PAD = [4390.0, 10639.2];
export const BAY_PAD = [4687.6, 10751.7];
// The respray bay counts a car as in the bay within 6 m (LF_LiveryShop); the proof asks for 4.
export const STOP_TOLERANCE_M = 4;

// key=value pairs of a `vehicle state` or `drive wait` answer.
export function kv(detail) {
  const out = {};
  for (const m of String(detail ?? "").matchAll(/(\w+)=(\S+)/g)) out[m[1]] = m[2];
  return out;
}

// A bool the game printed (1 or true).
export const yes = (v) => /^(1|true)$/i.test(String(v ?? ""));

// Polls `vehicle state` until the drive ends; prints one trace line per poll. Returns the last state.
export function follow(on, wait, { everyMs = 2000, maxMs = 200000, label = "drive" } = {}) {
  const t0 = Date.now();
  let last = {};
  while (Date.now() - t0 < maxMs) {
    wait(everyMs);
    const r = on({ verb: "vehicle", words: ["state"] });
    last = kv(r.detail);
    console.log(`${label} t=${((Date.now() - t0) / 1000).toFixed(0)}s pos=${last.pos} speed=${last.speed} drive=${last.drive} driven=${last.driven}`);
    if (!/^(driving|braking|stop|backing|routing)$/.test(last.drive ?? "")) break;
  }
  return last;
}

// Drives the car this client sits in; `words` are the drive verb's words (to|path|road <where> [options]).
// Returns { started, end } with end the `drive wait` answer's key=values (status first word).
export function driveAndFollow(on, wait, words, opts) {
  const started = on({ verb: "drive", words });
  if (started.status !== "ok") return { started, end: { status: "not_started" } };
  follow(on, wait, opts);
  const done = on({ verb: "drive", words: ["wait"] }, 30000);
  const end = kv(done.detail);
  end.status = String(done.detail ?? "").split(" ")[0];
  end.answer = done.status;
  return { started, end };
}

// The doors `drive probe` lists (`name@<d>m,h<h>,at=x;y;z,door`): a boom barrier's bar, a yard gate.
// `,door:shut` or `,door:open` is the state the probe reads (a bare `,door` is an older build: taken as shut).
export function doorsOf(detail) {
  const out = [];
  for (const m of String(detail ?? "").matchAll(/(\S+?)@[\d.]+m,h[\d.]+,at=(-?[\d.]+);(-?[\d.]+);(-?[\d.]+),door(?::(open|shut))?/g)) out.push({ name: m[1], at: [Number(m[2]), Number(m[3]), Number(m[4])], open: m[5] === "open" });
  return out;
}

// A drive that ended `why=...shut_door_...` stands at a shut gate or boom barrier: get out, open it with its door action
// the way a player does (standing round it until the action shows), and get back in. Returns what it did.
export function openShutGate(on, wait, car) {
  on({ verb: "vehicle", words: ["stop"] });
  wait(1000);
  on({ verb: "vehicle", words: ["exit", "force"] }, 10000);
  wait(1500);
  const probe = () => doorsOf(on({ verb: "drive", words: ["probe", `${car[0]},${car[2]}`, "r=10"] }, 20000).detail);
  const doors = probe();
  const stateOf = (d) => probe().find((x) => x.name === d.name && Math.hypot(x.at[0] - d.at[0], x.at[2] - d.at[2]) < 0.5);
  let opened = "";
  // A `use` finds the nearest door action in reach, which can be another door (a guardhouse's) than the one wanted: only a
  // door the probe then reads open counts. A bar's action is a few metres along it from its pivot.
  for (const d of doors.filter((x) => !x.open).sort((a, b) => (/BarGate/.test(b.name) ? 1 : 0) - (/BarGate/.test(a.name) ? 1 : 0))) {
    for (const r of [4.5, 3.5, 2.5, 1.5]) {
      for (let k = 0; k < 8 && !opened; k++) {
        const a = (k * Math.PI) / 4;
        on({ verb: "capture", mode: "world", name: "drive_gate", x: d.at[0], y: d.at[1], z: d.at[2], ox: Number((r * Math.cos(a)).toFixed(2)), oy: 0, oz: Number((r * Math.sin(a)).toFixed(2)) }, 30000);
        wait(800);
        if (on({ verb: "use", className: "SCR_DoorUserAction" }, 10000).status !== "ok") continue;
        // Re-probe until the door reads open (a bar swings slowly); something else may have opened instead: use again
        // only when it still reads shut after the last look.
        for (let p = 0; p < 3 && !opened; p++) {
          wait(p === 0 ? 2500 : 2000);
          if (stateOf(d)?.open) opened = d.name;
        }
      }
    }
    if (opened) break;
  }
  wait(4000);
  const enter = on({ verb: "vehicle", words: ["enter", "driver", `at=${car[0]},${car[2]}`, "r=10"] }, 15000);
  wait(1500);
  return { doors: doors.length, opened, entered: enter.status === "ok" };
}

// A car handed out on a pad can take a few seconds to stand there: ask again while none is in reach.
export function enterWhenThere(on, wait, pad, radius, tries = 8) {
  let r;
  for (let i = 0; i < tries; i++) {
    r = on({ verb: "vehicle", words: ["enter", "driver", `at=${pad.join(",")}`, `r=${radius}`] }, 15000);
    if (r.status === "ok" || !/no_vehicle_within/.test(String(r.detail))) return r;
    wait(2000);
  }
  return r;
}

function selftest() {
  const f = [];
  const s = kv("seat=pilot veh=S105.et speed=12.5kmh pos=1,2,3 drive=driving");
  if (s.seat !== "pilot" || s.pos !== "1,2,3" || s.drive !== "driving") f.push("kv reads a state line");
  const doors = doorsOf("probe ground=43.6 n=2 BarGate_01_bar.et@4.4m,h0.5,at=7450.1;44.9;8283.2,door GuardHouse_01_beige.et@11.9m,h8.1,at=7460;44.4;8290");
  if (doors.length !== 1 || doors[0].name !== "BarGate_01_bar.et" || doors[0].at[2] !== 8283.2) f.push("doorsOf reads the doors a probe lists");
  const states = doorsOf("probe n=2 BarGate_01_bar.et@4.4m,h0.5,at=1;2;3,door:shut layers[Main:- x] Door_01.et@5m,h2,at=4;5;6,door:open");
  if (states.length !== 2 || states[0].open || !states[1].open) f.push("doorsOf reads the open or shut state");
  if (Math.round(Math.hypot(BAY_PAD[0] - LOT_PAD[0], BAY_PAD[1] - LOT_PAD[1])) < 250) f.push("the pads are about 300 m apart");
  for (const x of f) console.log(`FAIL  ${x}`);
  console.log(f.length ? "drive-proof selftest: FAIL" : "drive-proof selftest: PASS");
  return f.length ? 1 : 0;
}

function opt(args, name, fallback) {
  const i = args.indexOf(name);
  return i >= 0 && args[i + 1] ? args[i + 1] : fallback;
}

async function run(args) {
  if (args.includes("--selftest")) return selftest();
  const speed = opt(args, "--speed", "35");
  const mode = opt(args, "--mode", "input");
  const k = await openKit("drive-proof");
  const { on, wait, step, state } = k;

  // 0: money and licences, then the cheapest car from the lot; it waits on the lot's pad.
  on({ verb: "checkpoint", words: ["10"] }, 20000);
  wait(3000);
  on({ verb: "stand", entityName: LOT });
  wait(2000);
  on({ verb: "use", className: "LF_VehicleBuyAction", entityName: LOT }, 10000);
  wait(2000);
  const widget = /Car\d+_Action/.exec(String(on({ verb: "ui" }).detail))?.[0] ?? "Car0_Action";
  on({ verb: "click", widget });
  wait(4000);
  const st = state();
  step("0 a car is bought and parked", !!st.parked && st.parked !== "none", `parked=${st.parked} parkedat=${st.parkedat}`);

  // 1: at the wheel of the car on the lot pad.
  const enter = enterWhenThere(on, wait, LOT_PAD, 6);
  wait(1500);
  const seated = kv(on({ verb: "vehicle", words: ["state"] }).detail);
  step("1 the character sits at the wheel and this client owns the car", enter.status === "ok" && seated.seat === "pilot" && yes(seated.owner), `${enter.status} seat=${seated.seat} owner=${seated.owner} veh=${seated.veh} pos=${seated.pos}`);
  const startPos = String(seated.pos ?? "").split(",").map(Number);

  // 2: the road to the garage, stopping on the respray pad.
  const t0 = Date.now();
  let run1 = driveAndFollow(on, wait, ["road", BAY_PAD.join(","), `speed=${speed}`, "timeout=240", `tol=2`, `mode=${mode}`], { maxMs: 250000 });
  if (run1.started.status !== "ok") {
    console.log(`drive road refused (${run1.started.detail}); trying a straight drive`);
    run1 = driveAndFollow(on, wait, ["to", BAY_PAD.join(","), `speed=${speed}`, "timeout=240", `tol=2`, `mode=${mode}`], { maxMs: 250000 });
  }
  const seconds = (Date.now() - t0) / 1000;
  const end = run1.end;
  const err = parseFloat(end.err ?? "999");
  console.log(`drive-proof: start=${startPos.join(",")} end=${end.at} driven=${end.driven} game_time=${end.time} wall=${seconds.toFixed(1)}s max=${end.max} err=${end.err} backups=${end.backups} mode=${end.mode}`);
  step("2 the car drove the road to the garage", end.status === "arrived" && parseFloat(end.driven) > 250, `${run1.started.detail} -> ${end.status} driven=${end.driven}`);
  step("3 it stopped on the respray pad", err <= STOP_TOLERANCE_M, `err=${err} m (tolerance ${STOP_TOLERANCE_M} m)`);
  const still = kv(on({ verb: "vehicle", words: ["state"] }).detail);
  step("3b it stands still with the handbrake on", parseFloat(still.speed) < 1.5 && yes(still.handbrake), `speed=${still.speed} handbrake=${still.handbrake} health=${still.health}`);

  // 4: out of the car.
  const exit = on({ verb: "vehicle", words: ["exit"] }, 10000);
  wait(1500);
  const out = kv(on({ verb: "vehicle", words: ["state"] }).detail);
  step("4 the character got out", exit.status === "ok" && out.seat === "none", `${exit.status} ${exit.detail}`);
  return k.finish();
}

export default {
  name: "drive-proof",
  usage: "drive-proof [--speed kmh] [--mode input|sim] [--selftest]   (on `up --mission island`; buys a car, drives it lot -> respray pad)",
  run: (ctx) => run(ctx?.args ?? []),
};

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

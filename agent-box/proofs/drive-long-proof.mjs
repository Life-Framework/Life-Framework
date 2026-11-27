import { openKit } from "./_island-kit.mjs";
import { LOT, LOT_PAD, driveAndFollow, enterWhenThere, kv } from "./drive-proof.mjs";

// Long road drives with the harness driver (LF_DriveHarness.c): the routes Saint-Philippe to the east compound,
// the north outpost and the NE truck yard (`drive plan`, no driving), then the drive Saint-Philippe to the east
// compound N times in a row, driving back to the lot by road between runs. On `session up --mission island` (one client):
//   node agent-box/session.mjs drive-long-proof [--runs 3] [--speed 50] [--to x,z] [--tol m] [--no-plans] [--no-drive] [--buy] [--return]
// Every run starts on the car lot pad in a car bought there (the tight lot exit is part of the test; --buy buys one
// for run 1 too); with --return a later run first drives the same car back to the street by the lot (a RETURN line). Each run prints one RUN line:
// status, metres, game seconds, back-ups, re-plans and, when it did not arrive, why and where it stopped.
export const EAST_COMPOUND = [6430, 6470];
export const NORTH_OUTPOST = [7688, 8240];
export const TRUCK_YARD = [5024, 10848];
export const SCRAPYARD = [5045, 10900];
// Where a truck bought at the yard stands (Fit_ne_compound_truckyard_lot 5086.6, 10844.3 + m_vOffset -22.5, 4.5).
export const TRUCK_PAD = [5064, 10849];
// The street just north of the car lot (on the road every run takes out of town): the return legs end here, not
// on the lot pad, where the display cars box a returning car in.
export const TOWN_STREET = [4409, 10684];

function opt(args, name, fallback) {
  const i = args.indexOf(name);
  return i >= 0 && args[i + 1] ? args[i + 1] : fallback;
}

export function planOf(detail) {
  const m = /plan points=(\d+) route=(\d+)m straight=(\d+)m/.exec(String(detail ?? ""));
  return m ? { points: Number(m[1]), route: Number(m[2]), straight: Number(m[3]) } : null;
}

function selftest() {
  const f = [];
  const p = planOf("plan points=812 route=6324m straight=4563m near=5040.1,10890.2");
  if (!p || p.route !== 6324 || p.straight !== 4563) f.push("planOf reads a plan answer");
  if (planOf("error road_graph_too_big") !== null) f.push("an error is no plan");
  for (const x of f) console.log(`FAIL  ${x}`);
  console.log(f.length ? "drive-long-proof selftest: FAIL" : "drive-long-proof selftest: PASS");
  return f.length ? 1 : 0;
}

async function run(args) {
  if (args.includes("--selftest")) return selftest();
  const runs = Math.max(0, Number(opt(args, "--runs", "3")));
  const speed = opt(args, "--speed", "50");
  const tol = opt(args, "--tol", "10");
  const to = opt(args, "--to", EAST_COMPOUND.join(",")).split(",").map(Number);
  const k = await openKit("drive-long-proof");
  const { on, wait, step, state } = k;

  // At the wheel of the owned car: enter; when the server puts the driver out (a locked car), unlock it from
  // beside a door (as drive-haul-proof does) and enter again.
  const seated = () => {
    const isPilot = () => kv(on({ verb: "vehicle", words: ["state"] }).detail).seat === "pilot";
    if (isPilot()) return true;
    const p = String(state().parkedat ?? "").split(",").map(Number);
    if (p.length !== 3) return false;
    for (let k = 0; k < 9; k++) {
      if (k > 0) {
        const a = ((k - 1) * Math.PI) / 4;
        on({ verb: "capture", mode: "world", name: "drive_long_door", x: p[0], y: p[1], z: p[2], ox: Number((3 * Math.cos(a)).toFixed(2)), oy: 0.3, oz: Number((3 * Math.sin(a)).toFixed(2)), hour: 9 }, 30000);
        wait(1200);
        on({ verb: "use", className: "LF_VehicleLockAction" }, 10000);
        wait(1300);
      }
      on({ verb: "vehicle", words: ["enter", "driver", `at=${p[0]},${p[2]}`, "r=15"] }, 15000);
      wait(3000);
      if (isPilot()) return true;
    }
    return false;
  };


  // 0: money and licences, a car from the lot, at the wheel.
  // checkpoint resets the pack (the car key with it), so only before a purchase.
  const buy = () => {
    on({ verb: "checkpoint", words: ["10"] }, 20000);
    wait(3000);
    on({ verb: "stand", entityName: LOT });
    wait(2000);
    on({ verb: "use", className: "LF_VehicleBuyAction", entityName: LOT }, 10000);
    wait(2000);
    const widget = /Car\d+_Action/.exec(String(on({ verb: "ui" }).detail))?.[0] ?? "Car0_Action";
    on({ verb: "click", widget });
    wait(4000);
    return state();
  };
  let st = state();
  if (args.includes("--buy") || !st.parked || st.parked === "none") st = buy();
  step("0 a car is bought and parked", !!st.parked && st.parked !== "none", `parked=${st.parked} parkedat=${st.parkedat}`);
  const onLot = Math.hypot(Number(String(st.parkedat).split(",")[0]) - LOT_PAD[0], Number(String(st.parkedat).split(",")[2]) - LOT_PAD[1]) < 10;
  const enter = onLot ? enterWhenThere(on, wait, LOT_PAD, 6) : { status: seated() ? "ok" : "error", detail: "seated where it stands" };
  wait(1500);
  step("0b at the wheel", enter.status === "ok", String(enter.detail).slice(0, 120));

  // 1: the long routes, planned only.
  if (!args.includes("--no-plans")) {
    const plans = [
      ["east compound", EAST_COMPOUND, ""],
      ["north outpost", NORTH_OUTPOST, ""],
      ["NE truck yard", TRUCK_YARD, `near=${SCRAPYARD.join(",")}`],
      ["truck yard to town", LOT_PAD, `from=${TRUCK_YARD.join(",")}`],
      ["truck pad to town", LOT_PAD, `from=${TRUCK_PAD.join(",")} near=${SCRAPYARD.join(",")}`],
    ];
    for (const [label, [x, z], extra] of plans) {
      const words = ["plan", `${x},${z}`];
      if (extra) words.push(...extra.split(" "));
      const r = on({ verb: "drive", words }, 60000);
      const p = planOf(r.detail);
      step(`1 a road route Saint-Philippe to the ${label}`, r.status === "ok" && !!p && p.route > 0, p ? `${p.route} m by road for ${p.straight} m straight, ${p.points} points` : String(r.detail).slice(0, 160));
      if (/near=/.test(String(r.detail))) console.log(`PLAN ${label} near the scrapyard: ${String(r.detail).replace(/^.*near=/, "")}`);
    }
  }

  // The east compound smelter stands inside tin walls: a drive "to the smelter" ends at the last road point
  // before it (the compound gate), not at the straight line from there through the wall.
  if (!args.includes("--exact")) {
    const r = on({ verb: "drive", words: ["plan", to.join(","), `near=${to.join(",")}`] }, 60000);
    const near = String(r.detail ?? "").replace(/^.*near=/, "").split(";").filter(Boolean);
    if (near.length >= 2) {
      const end = near[near.length - 2].split(",").map(Number);
      console.log(`drive goal: the road end ${end.join(",")} before ${to.join(",")}`);
      to.splice(0, 2, ...end);
    }
  }

  // 2: the drive, `runs` times.
  const results = [];
  const returns = [];
  for (let i = 1; i <= runs && !args.includes("--no-drive"); i++) {
    // Each later run starts the way run 1 does: a car bought on the lot pad with a full tank (31 km on one tank
    // ran a T601 dry on 2026-11-27). No teleport: a car set down by `parkat` under a seated driver will not move,
    // and a proof does not re-enter a locked car reliably. --return drives the same car back to town instead.
    if (i > 1 && !args.includes("--return")) {
      on({ verb: "vehicle", words: ["stop"] }, 20000);
      wait(1000);
      on({ verb: "vehicle", words: ["exit", "force"] }, 10000);
      wait(1500);
      const fresh = buy();
      const again = enterWhenThere(on, wait, LOT_PAD, 6);
      wait(1500);
      if (again.status !== "ok") {
        step(`2.${i} at the wheel of a fresh car on the lot pad`, false, `${again.detail} parked=${fresh.parked}`);
        break;
      }
    }
    if (i > 1 && args.includes("--return")) {
      const back = driveAndFollow(on, wait, ["road", TOWN_STREET.join(","), `speed=${speed}`, "tol=8", "mode=input"], { label: `back${i}`, everyMs: 5000, maxMs: 1500000 });
      const b2 = back.end;
      returns.push(b2.status);
      console.log(`RETURN ${i - 1}: ${b2.status} driven=${b2.driven} time=${b2.time} err=${b2.err} max=${b2.max} backups=${b2.backups} replans=${b2.replans}${b2.why ? ` why=${b2.why}` : ""}${b2.stuckat ? ` stuckat=${b2.stuckat}` : ""}`);
      on({ verb: "vehicle", words: ["stop"] }, 20000);
      wait(1000);
    }
    const t0 = Date.now();
    const r = driveAndFollow(on, wait, ["road", to.join(","), `speed=${speed}`, `tol=${tol}`, "mode=input"], { label: `run${i}`, everyMs: 5000, maxMs: 1500000 });
    const e = r.end;
    const wall = (Date.now() - t0) / 1000;
    results.push(e.status);
    console.log(`RUN ${i}: ${e.status} driven=${e.driven} time=${e.time} wall=${wall.toFixed(0)}s err=${e.err} max=${e.max} backups=${e.backups} replans=${e.replans} progress=${e.progress}/${e.route}${e.why ? ` why=${e.why}` : ""}${e.stuckat ? ` stuckat=${e.stuckat}` : ""} (${r.started.detail})`);
    step(`2.${i} the drive Saint-Philippe to ${to.join(",")} arrives`, e.status === "arrived", `${e.status} ${e.driven} m in ${e.time} s, backups ${e.backups}`);
  }
  if (returns.length) console.log(`RETURNS ${returns.filter((x) => x === "arrived").length} of ${returns.length} arrived (${returns.join(", ")})`);
  if (results.length) console.log(`RUNS ${results.filter((s) => s === "arrived").length} of ${results.length} arrived (${results.join(", ")})`);
  return k.finish();
}

export default {
  name: "drive-long-proof",
  usage: "drive-long-proof [--runs 3] [--speed 50] [--to x,z] [--tol m] [--no-plans] [--no-drive] [--selftest]   (on `up --mission island`; long road routes and drives)",
  run: (ctx) => run(ctx?.args ?? []),
};

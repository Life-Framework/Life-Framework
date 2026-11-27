import { openKit, selftestKit, toastOf } from "./_island-kit.mjs";
import { BAY_PAD, driveAndFollow, enterWhenThere, kv } from "./drive-proof.mjs";

// The impound toast as a player reads it (flows2-1127). The harness `toast=` and the client log print the raw key
// (`#LF-Impound_Collected`); what the screen shows is the translated string with the plate in it. Two phases around a
// `session restart` (a restart impounds every car left on the street):
//   node agent-box/session.mjs island-impound-proof --phase buy        (checkpoint 10 money and licences, buy the cheapest car)
//   node agent-box/session.mjs restart
//   node agent-box/session.mjs island-impound-proof --phase collect    (the desk's F row, a picture of the toast on screen)
// `--phase collect` fails when the toast the player sees still starts with `#LF-` (a raw key) or the plate is missing.
const LOT = process.env.LOT ?? "Fit_saint_philippe_carlot_vehicle_lot";
const DESK = process.env.IMPOUND_DESK ?? "Fit_saint_philippe_carlot_impound_desk";
// The desk hands the car out on its pad: the desk (4396.787, 10627.88) + m_vOffset (9, 14.5) in the carlot layer.
const IMPOUND_PAD = [4405.8, 10642.4];

// The text a toast shows on the player's screen: true when it is a readable sentence, not a localization key.
export function readable(text) {
  return /\s/.test(String(text)) && !/^#?LF-/.test(String(text).trim());
}

async function run(args) {
  if (args.includes("--selftest")) {
    const f = selftestKit();
    if (readable("#LF-Impound_Collected")) f.push("a raw key is not readable");
    if (!readable("Released: AB CD 1234 is waiting in the yard.")) f.push("a sentence is readable");
    if (toastOf("toast=Released") !== "Released") f.push("toast read");
    for (const x of f) console.log(`FAIL  ${x}`);
    console.log(f.length ? "island-impound-proof selftest: FAIL" : "island-impound-proof selftest: PASS");
    return f.length ? 1 : 0;
  }
  const pi = args.indexOf("--phase");
  const phase = pi >= 0 ? args[pi + 1] : "buy";
  const k = await openKit("island-impound-proof", { arriveFirst: phase === "buy" });
  const { on, wait, step, state, cash, shot, serverLog } = k;

  if (phase === "buy") {
    on({ verb: "checkpoint", words: ["10"] }, 20000);
    wait(3000);
    on({ verb: "stand", entityName: LOT });
    wait(2000);
    on({ verb: "use", className: "LF_VehicleBuyAction", entityName: LOT }, 10000);
    wait(2000);
    const widget = /Car\d+_Action/.exec(String(on({ verb: "ui" }).detail))?.[0] ?? "Car0_Action";
    on({ verb: "click", widget });
    wait(3000);
    const st = state();
    step("0 a car is bought and owned", !!st.parked && st.parked !== "none", `parked=${st.parked} cash=${st.cash} bank=${st.bank}`);
    return k.finish();
  }

  // phase collect: the character is back after the restart and the car is impounded (parkedat=none).
  wait(2000);
  let st = state();
  step("0 the car is owned and impounded after the restart", !!st.parked && st.parked !== "none" && st.parkedat === "none",`parked=${st.parked} parkedat=${st.parkedat}`);
  const cashBefore = cash();
  on({ verb: "stand", entityName: DESK });
  wait(2000);
  on({ verb: "look" });
  const mark = serverLog().length;
  const press = on({ verb: "use", className: "LF_ImpoundCollectAction", entityName: DESK }, 10000);
  wait(1200);
  // The toast shows for 5 s: the screen is photographed first, then the client's last toast key is read.
  shot("impound_toast_on_screen");
  const key = toastOf(on({ verb: "seen", words: ["4"] }).detail);
  wait(2500);
  st = state();
  const log = serverLog().slice(mark);
  step("1 the desk releases the car", press.status === "ok" && /impound collected/.test(log) && !!st.parkedat && st.parkedat !== "none", `${press.status}; cash ${cashBefore} -> ${st.cash}; parkedat=${st.parkedat}`);
  step("1b the toast the client received is the collected key", /Impound_Collected/.test(key), key);
  // 2: the released car is driven like a player would, 100 m along the road toward the garage (LF_DriveHarness).
  const enter = enterWhenThere(on, wait, IMPOUND_PAD, 10);
  const seat = kv(on({ verb: "vehicle", words: ["state"] }).detail);
  step("2 the owner sits at the wheel of the released car", enter.status === "ok" && seat.seat === "pilot", `${enter.status} ${enter.detail}`);
  const { started, end } = driveAndFollow(on, wait, ["road", BAY_PAD.join(","), "speed=30", "timeout=90", "dist=100"], { maxMs: 100000, label: "impound drive" });
  step("2b the released car drives 100 m", end.status === "distance" && parseFloat(end.driven) >= 100, `${started.status} ${started.detail} -> ${end.status} driven=${end.driven} time=${end.time} at=${end.at}`);
  on({ verb: "vehicle", words: ["exit"] }, 10000);
  console.log(`The picture impound_toast_on_screen shows what the player reads; open it and confirm the text is a sentence with the plate (the string row is "Released: %1 is waiting in the yard.").`);
  return k.finish();
}

export default {
  name: "island-impound-proof",
  usage: "island-impound-proof --phase buy|collect [--selftest]   (on `up --mission island --profile full`; `session.mjs restart` between the phases)",
  // A reset clears the pockets (the owner's key row); this proof reads the character the previous phase left.
  noReset: true,
  run: (ctx) => run(ctx?.args ?? []),
};

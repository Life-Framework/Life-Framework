import { openKit } from "./_island-kit.mjs";

// The woodlot saw worked from four sides (#5075): the sawn boards must land beside whoever works the saw, so the
// player picks them up without stepping away. `up --mission island --profile full --hold 20`, one client already
// arrived (this proof never arrives: the fresh account's quiz and form are the intro flow's job).
//   node agent-box/session.mjs saw-sides-proof [--sides e,w,n,s]
// Per side: clear the pockets (sell at the wholesale counter beside the saw), stand there (`stand <saw> <side>`),
// `itemgive` one hardwood log, press the saw's Saw hardwood row, wait for the server's `processed` line, then
// `pickup HardwoodBoard`: its reply names the distance the board was picked up from, which must be inside the 3 m reach.
const SAW = "Fit_saint_philippe_woodlot_saw";
const DESK = "Fit_saint_philippe_woodlot_desk";
const WHOLESALE = "Fit_saint_philippe_woodlot_wholesale";
const SAW_ACTION = process.env.SAW_ACTION ?? "LF_ProcessAction__3";
const LOG = "{FC7D57CC9C0798D7}Prefabs/Resources/WoodCutting/WoodLog_Hardwood.et";
const REACH_M = 3;

async function run(args) {
  const k = await openKit("saw-sides-proof", { arriveFirst: false });
  const { on, wait, step, carried, shot, serverLog } = k;
  const sidesArg = args.includes("--sides") ? args[args.indexOf("--sides") + 1] : "e,w,n,s";
  on({ verb: "levelset", words: ["LUMBERJACK", "3"] }, 10000);
  wait(1500);
  on({ verb: "guide", words: ["close"] });
  on({ verb: "stand", entityName: DESK });
  wait(1500);
  on({ verb: "use", className: "LF_WorkplaceJobAction", entityName: DESK }, 10000);
  wait(1500);
  // Empty the pockets at the wholesale counter; a pocket holds four boards and a full one refuses the next pickup.
  const emptyPockets = () => {
    on({ verb: "stand", entityName: WHOLESALE });
    wait(1200);
    for (let p = 0; p < 8 && carried("HardwoodBoard") > 0; p++) {
      on({ verb: "use", className: "LF_TraderSellAction", entityName: WHOLESALE });
      wait(1000);
    }
  };
  // Boards left on the ground by an earlier side would be picked up instead of the new ones: sweep them first.
  const sweep = () => {
    for (let p = 0; p < 12; p++) {
      const r = on({ verb: "pickup", item: "HardwoodBoard" }, 15000);
      if (r.status !== "ok") break;
      wait(500);
      if (carried("HardwoodBoard") >= 4) emptyPockets();
      on({ verb: "stand", entityName: SAW });
      wait(800);
    }
  };
  on({ verb: "stand", entityName: SAW });
  wait(1200);
  sweep();
  for (const side of sidesArg.split(",")) {
    emptyPockets();
    on({ verb: "stand", entityName: SAW, side });
    wait(1500);
    on({ verb: "itemgive", prefab: LOG }, 10000);
    wait(1500);
    const mark = serverLog().length;
    const proc = on({ verb: "use", className: SAW_ACTION, entityName: SAW }, 10000);
    let out = "";
    for (let i = 0; i < 40 && !out; i++) {
      out = /\[LFDebug:Processing\] processed [^\n]*/.exec(serverLog().slice(mark))?.[0] ?? "";
      if (!out) wait(1000);
    }
    wait(1500);
    shot(`saw_${side}_boards_beside`);
    const pick = on({ verb: "pickup", item: "HardwoodBoard" }, 15000);
    wait(1000);
    const dist = Number(/dist=([\d.]+)/.exec(String(pick.detail))?.[1] ?? NaN);
    step(`saw from ${side}: the boards land inside the operator's reach`, proc.status === "ok" && !!out && pick.status === "ok" && dist <= REACH_M, `${proc.status}; ${out.slice(0, 110)}; pickup ${pick.status} dist=${dist} m (reach ${REACH_M} m)`);
    sweep();
  }
  return k.finish();
}

export default {
  name: "saw-sides-proof",
  usage: "saw-sides-proof [--sides e,w,n,s]   (on `up --mission island --profile full --hold 20`, an arrived client: the woodlot saw from each side, boards picked up without moving)",
  run: (ctx) => run(ctx?.args ?? []),
};

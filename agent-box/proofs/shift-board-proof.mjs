import { arrive, clientBox, sessionDir } from "../session.mjs";

// The weekly shift boards at the two island work sites (#4713, WK-W22 #4712). On a running island session
// (`tools\cli session up --mission island --profile full --wait`, one client): at each site the character takes
// a few thousand XP in that site's trade (`levelupcard grant`, the real AddXp path, which publishes the award the
// board counts), walks to the printed board, reads it ("Read the shift board") and the sheet must list the
// character's line "<Trade> <level>, <n> XP". Shots of the board and of the sheet go in the PR.
//   node agent-box/session.mjs up --mission island --profile full --wait
//   node agent-box/session.mjs shift-board-proof [mine|sawmill|both]
//   node agent-box/session.mjs down
const ACTION = "LF_ShiftBoardReadAction";
const SITES = {
  mine: { board: "Fit_coalmans_brow_mine_mine_board", job: "MINER", trade: "Mining", xp: 1500 },
  sawmill: { board: "Mapper_everon_sawmill_yard_M1", job: "LUMBERJACK", trade: "Lumber", xp: 900 },
};

async function runShiftBoardProof(args) {
  const which = String(args?.[0] ?? "both");
  const sites = which === "both" ? ["mine", "sawmill"] : [which];
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${String(r.detail).slice(0, 240)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  arrive(on, 1, wait);
  wait(4000);
  on(1, { verb: "guide", words: ["close"] });
  for (const key of sites) {
    const s = SITES[key];
    if (!s) {
      fails.push(`unknown site ${key} (mine, sawmill or both)`);
      continue;
    }
    const stand = on(1, { verb: "stand", entityName: s.board }, 15000);
    if (stand.status !== "ok") {
      fails.push(`${key}: could not stand at ${s.board}: ${stand.status} ${stand.detail}`);
      continue;
    }
    wait(2500);
    on(1, { verb: "levelupcard", words: ["grant", s.job, String(s.xp)] }, 10000);
    wait(2500);
    on(1, { verb: "guide", words: ["close"] });
    on(1, { verb: "capture", mode: "ui", name: `shiftboard_${key}_board` }, 25000);
    on(1, { verb: "look" });
    const read = on(1, { verb: "use", className: ACTION, entityName: s.board }, 15000);
    if (read.status !== "ok") {
      fails.push(`${key}: the read action did not run: ${read.status} ${read.detail}`);
      continue;
    }
    wait(2500);
    const ui = on(1, { verb: "ui", mode: "text" }, 10000);
    on(1, { verb: "capture", mode: "ui", name: `shiftboard_${key}_read` }, 25000);
    const text = String(ui.detail ?? "");
    if (!new RegExp(`${s.trade} \\d+, [\\d,]+ XP`).test(text)) fails.push(`${key}: the sheet shows no "${s.trade} <level>, <n> XP" line: ${text.slice(0, 300)}`);
    on(1, { verb: "guide", words: ["close"] });
    wait(1000);
  }
  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "shift board proof: FAIL" : `shift board proof: PASS (${sites.join(", ")}: the earned XP shows on the site's board)`);
  return fails.length ? 1 : 0;
}

export default {
  name: "shift-board-proof",
  usage: "shift-board-proof [mine|sawmill|both]",
  run: (ctx) => runShiftBoardProof(ctx?.args ?? []),
};

import { readFileSync } from "node:fs";
import { join } from "node:path";
import { arrive, clientBox, newestLog, parseState, sessionDir, sessionPaths } from "../session.mjs";

// Export orders end to end (#4580): take an order, bring the goods to the crane spot, hand in, get paid.
//   node agent-box/session.mjs up --mission grid --profile full --wait   (the pack grid header, so the
//     export order overlay applies; the board and the spot stand in grid cell slice-4411)
//   node agent-box/session.mjs export-crane-proof [--board <entity>] [--spot <entity>] [--name <prefix>]
//   node agent-box/session.mjs down
// On the island pass --board and --spot with the Fit_ names of the dock rows. One client:
//   1. a level-1 hauler uses the board's real "Take the order" row and the server refuses it on level;
//   2. `levelupcard grant DELIVERY 5000` earns level 10 and the board's real row takes an order (#4596);
//   3. `exportorder give short` puts count-1 of the goods in the pockets: "Hand in the export order" at the
//      spot is refused and the wallet stays;
//   4. `exportorder give rest` adds the last unit: the hand-in pays the order;
//   5. a second hand-in pays nothing.
// Verdicts read the wallet (`state`) and the server's [LFDebug:ExportOrder] lines.
const BOARD = "Grid_slice_4411_R0";
const SPOT = "Grid_slice_4411_CraneSpot";
const HAND_IN = "LF_ExportCraneHandInAction";

async function runExportCraneProof() {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const flag = (name, dflt) => { const i = process.argv.indexOf(name); return i > 0 ? process.argv[i + 1] : dflt; };
  const board = flag("--board", BOARD);
  const spot = flag("--spot", SPOT);
  const prefix = flag("--name", "export_crane");
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${String(r.detail).slice(0, 220)}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const exportLines = () => {
    const f = newestLog(join(sessionPaths(dir).server, "logs"));
    return f ? readFileSync(f, "utf8").split("\n").filter((l) => l.includes("[LFDebug:ExportOrder]")) : [];
  };
  const newLines = (from) => exportLines().slice(from).join("\n");
  const fails = [];
  const step = (name, ok, detail = "") => {
    console.log(`${ok ? "PASS" : "FAIL"}  ${name}${detail ? `: ${detail}` : ""}`);
    if (!ok) fails.push(name);
  };
  const cash = () => Number(parseState(on(1, { verb: "state" }, 10000).detail).cash || 0);
  const shot = (name) => { on(1, { verb: "guide", words: ["close"] }); return on(1, { verb: "capture", mode: "ui", name: `${prefix}_${name}` }, 25000); };
  const handIn = () => { on(1, { verb: "use", className: HAND_IN, entityName: spot }, 10000); wait(2500); };

  arrive(on, 1, wait);
  wait(3000);
  on(1, { verb: "resetplayer", cash: 0 }, 15000);
  on(1, { verb: "guide", words: ["close"] });

  // 1. The board's real take row, for a level-1 hauler.
  // The first stand from the far spawn can land short while the island streams in: stand twice.
  on(1, { verb: "stand", entityName: board }, 10000);
  wait(8000);
  on(1, { verb: "stand", entityName: board }, 10000);
  wait(4000);
  shot("board");
  let mark = exportLines().length;
  // The take rows show once the face's rows reach the client (a board read round trip).
  for (let i = 0; i < 12; i++) {
    if (on(1, { verb: "use", className: "LF_ExportBoardTakeAction", entityName: board }, 10000).status === "ok") break;
    wait(2000);
  }
  wait(2500);
  step("the board refuses a level-1 hauler", /take refused: \S+ is level \d+, orders need \d+/.test(newLines(mark)), newLines(mark).slice(0, 200));

  // 2. Earn DELIVERY level 10 (5000 XP on the 30-level table, #4596) and take the order with the board's real row.
  on(1, { verb: "levelupcard", words: ["grant", "DELIVERY", "5000"] }, 10000);
  wait(2500);
  mark = exportLines().length;
  let took = false;
  for (let i = 0; i < 12 && !took; i++) {
    on(1, { verb: "use", className: "LF_ExportBoardTakeAction", entityName: board }, 10000);
    wait(2500);
    took = /\S+ took order \d+/.test(newLines(mark));
  }
  shot("taken");
  step("a level 10 hauler takes an order at the board", took, newLines(mark).slice(0, 200));

  // 3. Short by one: refused, nothing paid.
  const before = cash();
  on(1, { verb: "exportorder", words: ["give", "short"] }, 10000);
  wait(1500);
  on(1, { verb: "stand", entityName: spot }, 10000);
  wait(2500);
  shot("spot");
  mark = exportLines().length;
  handIn();
  shot("short");
  step("the server refuses the short count", /hand-in refused: \S+ brought \d+ of \d+/.test(newLines(mark)), newLines(mark).slice(0, 200));
  step("a short hand-in pays nothing", cash() === before, `cash ${cash()} (before ${before})`);

  // 4. The last unit: paid.
  on(1, { verb: "exportorder", words: ["give", "rest"] }, 10000);
  wait(1500);
  mark = exportLines().length;
  handIn();
  shot("paid");
  const paidMatch = newLines(mark).match(/export order handed in by \S+: \$(\d+)/);
  const paid = paidMatch ? Number(paidMatch[1]) : 0;
  const after = cash();
  step("the full hand-in pays the order", paid > 0 && after === before + paid, `paid $${paid}, cash ${after}`);

  // 5. Again: nothing held, nothing paid.
  handIn();
  step("a second hand-in pays nothing", cash() === after, `cash ${cash()}`);

  console.log(fails.length ? `export crane proof: FAIL (${fails.join("; ")})` : `export crane proof: PASS (short refused, full paid $${paid}, second paid nothing)`);
  return fails.length ? 1 : 0;
}

export default {
  name: "export-crane-proof",
  usage: "export-crane-proof [--board <entity>] [--spot <entity>] [--name <prefix>]   (up --mission grid --profile full)",
  run: () => runExportCraneProof(),
};

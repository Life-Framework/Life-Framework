import { ROB_TARGET, arrive, clientBox, sessionDir } from "../session.mjs";

// SPIKE (docs/architecture/visual-boards-spike.md) on a running `up --clients 2` session: both
// clients photograph the wanted board (the posters they got at join), client 1 robs the fuel
// till, both photograph the board again (the robber's poster arrived by broadcast). The
// `[LFDebug:VisualBoard]` lines land in each client's log; read them after `down`.
const BOARD = { x: 235.4, y: 0.55, z: 242.9 };

async function runVisualBoardProof(ctx) {
  const h = await import("../harness.mjs");
  const dir = sessionDir();
  const on = (n, spec, timeout) => {
    h.setBoxes([clientBox(dir, n)]);
    const r = h.sendCommand(spec, timeout);
    console.log(`client ${n}: ${r.line.replace(/^\d+ /, "")} -> ${r.status} ${r.detail}`);
    return r;
  };
  const wait = (ms) => Atomics.wait(new Int32Array(new SharedArrayBuffer(4)), 0, 0, ms);
  const fails = [];
  const dz = Number(process.env.LF_BOARD_DZ ?? 1.3);
  const shot = (n, name) => {
    const r = on(n, { verb: "capture", mode: "world", name, ...BOARD, ox: 0, oy: 0, oz: dz, hour: 12 }, 40000);
    if (r.status !== "ok") fails.push(`${name}: ${r.status} ${r.detail}`);
  };

  for (const n of [1, 2]) arrive(on, n, wait);
  wait(8000);
  for (const n of [1, 2]) shot(n, `visual_board_before_client${n}`);

  on(1, { verb: "stand", entityName: ROB_TARGET });
  wait(2500);
  on(1, { verb: "look" });
  const rob = on(1, { verb: "use", className: "LF_RobAction", entityName: ROB_TARGET }, 10000);
  if (rob.status !== "ok") fails.push(`rob returned ${rob.status} ${rob.detail}`);
  wait(6000);
  const robbed = on(1, { verb: "state" }, 10000).detail;
  if (!robbed.includes("wanted=1")) fails.push(`client 1 after the robbery: ${robbed}`);

  for (const n of [2, 1]) shot(n, `visual_board_after_client${n}`);

  for (const f of fails) console.log(`FAIL  ${f}`);
  console.log(fails.length ? "visual board proof: FAIL" : "visual board proof: PASS (posters at join on both clients, robbery, new poster on both)");
  return fails.length ? 1 : 0;
}

export default {
  name: "visual-board-proof",
  usage: "visual-board-proof",
  run: (ctx) => runVisualBoardProof(ctx),
};
